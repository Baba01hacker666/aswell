#include <cassert>
#include <iostream>
#include "aswell/ui/css_parser.hpp"

using namespace aswell;

void test_css_parsing() {
    std::string css = R"(
prompt.main {
    padding: 0 1;
    border: 1px solid #00f0ff;
    border-radius: 4px;
}
directory {
    color: #50fa7b;
    font-weight: bold;
}
symbol {
    color: #ff79c6;
    animation: pulse 1500ms infinite;
}
)";

    StyleSheet sheet = CSSParser::parse(css);
    assert(sheet.rules().size() >= 3);

    Style dir_style = sheet.compute_style("directory");
    assert(dir_style.bold == true);
    assert(dir_style.color.r == 80 && dir_style.color.g == 250 && dir_style.color.b == 123);

    Style sym_style = sheet.compute_style("symbol");
    assert(sym_style.animation.type == AnimationType::PULSE);
    assert(sym_style.animation.duration_ms == 1500);

    Style prompt_style = sheet.compute_style("prompt", {"main"});
    assert(prompt_style.border_type == BorderType::ROUNDED || prompt_style.border_type == BorderType::SOLID);
    assert(prompt_style.padding.right == 1);

    std::cout << "[PASS] test_css_parsing\n";
}

int main() {
    std::cout << "--- Running CSS Parser Tests ---\n";
    test_css_parsing();
    std::cout << "All CSS Parser Tests Passed!\n";
    return 0;
}
