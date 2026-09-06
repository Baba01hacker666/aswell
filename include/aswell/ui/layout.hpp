#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/dom.hpp"
#include "aswell/ui/style.hpp"

namespace aswell {

struct LayoutNode {
    std::shared_ptr<UIElement> element;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 1;
    std::string text;
    std::vector<std::shared_ptr<LayoutNode>> children;
    bool is_newline = false;
};

class LayoutEngine {
public:
    static std::shared_ptr<LayoutNode> compute_layout(std::shared_ptr<UIElement> root,
                                                       int max_width = 80,
                                                       uint64_t timestamp_ms = 0);

private:
    static std::shared_ptr<LayoutNode> build_node(std::shared_ptr<UIElement> elem,
                                                  uint64_t timestamp_ms);
};

} // namespace aswell
