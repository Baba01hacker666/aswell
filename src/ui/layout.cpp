#include "aswell/ui/layout.hpp"
#include "aswell/ui/animation.hpp"

namespace aswell {

std::shared_ptr<LayoutNode> LayoutEngine::build_node(std::shared_ptr<UIElement> elem,
                                                     uint64_t timestamp_ms) {
    if (!elem) return nullptr;
    if (elem->computed_style.display == DisplayType::NONE) return nullptr;

    auto node = std::make_shared<LayoutNode>();
    node->element = elem;

    if (elem->tag == "newline" || elem->tag == "br") {
        node->is_newline = true;
        node->width = 0;
        node->height = 1;
        return node;
    }

    // Evaluate content with possible animation
    std::string content = elem->text_content;
    if (elem->computed_style.animation.type == AnimationType::SPIN) {
        content = AnimationEngine::evaluate_glyph(elem->computed_style.animation, content, timestamp_ms);
    }
    node->text = content;

    for (auto& child : elem->children) {
        auto child_node = build_node(child, timestamp_ms);
        if (child_node) {
            node->children.push_back(child_node);
        }
    }

    // Calculate width
    int inner_w = static_cast<int>(str_util::visual_width(node->text));
    for (const auto& c : node->children) {
        inner_w += c->width;
    }

    const auto& st = elem->computed_style;
    int pad_w = st.padding.left + st.padding.right;
    int border_w = (st.border_type != BorderType::NONE) ? 2 : 0;
    int margin_w = st.margin.left + st.margin.right;

    node->width = inner_w + pad_w + border_w + margin_w;
    node->height = 1;

    return node;
}

std::shared_ptr<LayoutNode> LayoutEngine::compute_layout(std::shared_ptr<UIElement> root,
                                                         int /*max_width*/,
                                                         uint64_t timestamp_ms) {
    if (!root) return nullptr;
    return build_node(root, timestamp_ms);
}

} // namespace aswell
