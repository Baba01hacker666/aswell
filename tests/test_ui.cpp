#include <cassert>
#include <iostream>
#include "aswell/ui/dom.hpp"
#include "aswell/ui/layout.hpp"
#include "aswell/ui/render.hpp"
#include "aswell/ui/color.hpp"

using namespace aswell;

void test_dom_parsing() {
    std::string html = "<prompt class=\"main\"><user>alice</user><text>@</text><hostname>box</hostname></prompt>";
    auto dom = DOMParser::parse(html);
    assert(dom != nullptr);
    assert(dom->tag == "prompt");
    assert(dom->has_class("main"));
    assert(dom->children.size() == 3);
    assert(dom->children[0]->tag == "user");
    assert(dom->children[0]->text_content == "alice");
    std::cout << "[PASS] test_dom_parsing\n";
}

void test_color_and_visual_width() {
    Color c = Color::from_hex("#ff5555");
    assert(!c.is_none);
    assert(c.r == 255 && c.g == 85 && c.b == 85);

    std::string ansi_colored = "\033[31mhello\033[0m world";
    assert(str_util::visual_width(ansi_colored) == 11);
    assert(str_util::strip_ansi(ansi_colored) == "hello world");

    std::cout << "[PASS] test_color_and_visual_width\n";
}

int main() {
    std::cout << "--- Running UI & Layout Tests ---\n";
    test_dom_parsing();
    test_color_and_visual_width();
    std::cout << "All UI Tests Passed!\n";
    return 0;
}
