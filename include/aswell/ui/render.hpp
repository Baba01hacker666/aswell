#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/layout.hpp"
#include "aswell/ui/color.hpp"

namespace aswell {

struct RenderResult {
    std::string ansi_output;
    int total_lines = 1;
    int last_line_width = 0;
};

class TerminalRenderer {
public:
    static RenderResult render(std::shared_ptr<LayoutNode> root,
                               uint64_t timestamp_ms = 0,
                               bool truecolor = true,
                               bool unicode = true);

private:
    static void render_recursive(std::shared_ptr<LayoutNode> node,
                                 std::string& out,
                                 int& cur_line_width,
                                 int& line_count,
                                 uint64_t timestamp_ms,
                                 bool truecolor,
                                 bool unicode);
};

} // namespace aswell
