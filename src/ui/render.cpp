#include "aswell/ui/render.hpp"
#include "aswell/ui/animation.hpp"

namespace aswell {

static void apply_style_ansi(const Style& st,
                             std::string& out,
                             uint64_t timestamp_ms,
                             bool truecolor) {
    if (st.bold) out += "\033[1m";
    if (st.dim) out += "\033[2m";
    if (st.italic) out += "\033[3m";
    if (st.underline) out += "\033[4m";
    if (st.strikethrough) out += "\033[9m";

    Color fg = st.color;
    if (st.animation.type != AnimationType::NONE) {
        fg = AnimationEngine::evaluate_color(st.animation, fg, timestamp_ms);
    }
    if (!fg.is_none) {
        out += fg.to_fg_ansi(truecolor);
    }

    if (!st.bg_color.is_none) {
        out += st.bg_color.to_bg_ansi(truecolor);
    }
}

void TerminalRenderer::render_recursive(std::shared_ptr<LayoutNode> node,
                                        std::string& out,
                                        int& cur_line_width,
                                        int& line_count,
                                        uint64_t timestamp_ms,
                                        bool truecolor,
                                        bool unicode) {
    if (!node) return;
    if (node->element && node->element->computed_style.display == DisplayType::NONE) return;

    if (node->is_newline) {
        out += "\033[0m\n";
        line_count++;
        cur_line_width = 0;
        return;
    }

    const auto& st = node->element->computed_style;

    // Margin left
    for (int i = 0; i < st.margin.left; ++i) {
        out += ' ';
        cur_line_width++;
    }

    // Border Left
    if (st.border_type != BorderType::NONE) {
        Color bc = st.border_color.is_none ? st.color : st.border_color;
        if (!bc.is_none) out += bc.to_fg_ansi(truecolor);
        if (unicode) {
            out += "│";
        } else {
            out += "|";
        }
        cur_line_width += 1;
        out += "\033[0m";
    }

    // Padding left
    for (int i = 0; i < st.padding.left; ++i) {
        out += ' ';
        cur_line_width++;
    }

    // Apply text styling
    apply_style_ansi(st, out, timestamp_ms, truecolor);

    // Text content
    if (!node->text.empty()) {
        out += node->text;
        cur_line_width += static_cast<int>(str_util::visual_width(node->text));
    }

    // Render children
    for (const auto& child : node->children) {
        render_recursive(child, out, cur_line_width, line_count, timestamp_ms, truecolor, unicode);
    }

    // Reset styles
    out += "\033[0m";

    // Padding right
    for (int i = 0; i < st.padding.right; ++i) {
        out += ' ';
        cur_line_width++;
    }

    // Border Right
    if (st.border_type != BorderType::NONE) {
        Color bc = st.border_color.is_none ? st.color : st.border_color;
        if (!bc.is_none) out += bc.to_fg_ansi(truecolor);
        if (unicode) {
            out += "│";
        } else {
            out += "|";
        }
        cur_line_width += 1;
        out += "\033[0m";
    }

    // Margin right
    for (int i = 0; i < st.margin.right; ++i) {
        out += ' ';
        cur_line_width++;
    }
}

RenderResult TerminalRenderer::render(std::shared_ptr<LayoutNode> root,
                                      uint64_t timestamp_ms,
                                      bool truecolor,
                                      bool unicode) {
    RenderResult result;
    if (!root) return result;

    int cur_width = 0;
    int lines = 1;

    render_recursive(root, result.ansi_output, cur_width, lines, timestamp_ms, truecolor, unicode);

    result.total_lines = lines;
    result.last_line_width = cur_width;

    return result;
}

} // namespace aswell
