#include "aswell/ui/error_box.hpp"
#include "aswell/ui/terminal.hpp"

namespace aswell {

void ErrorBox::render_not_found(const std::string& cmd,
                                const std::vector<std::string>& suggestions) {
    bool unicode = Terminal::supports_unicode();
    std::string tl = unicode ? "╭─" : "+-";
    std::string bl = unicode ? "╰" : "+";
    std::string bar = unicode ? "│" : "|";
    std::string horiz = unicode ? "─" : "-";

    std::cerr << "\033[1;31m" << tl << " Aswell Error\033[0m\n"
              << "\033[1;31m" << bar << "\033[0m\n"
              << "\033[1;31m" << bar << "\033[0m  Command not found: \033[1;37m" << cmd << "\033[0m\n";

    if (!suggestions.empty()) {
        std::cerr << "\033[1;31m" << bar << "\033[0m\n"
                  << "\033[1;31m" << bar << "\033[0m  Did you mean:\n";
        for (const auto& s : suggestions) {
            std::cerr << "\033[1;31m" << bar << "\033[0m    \033[1;36m" << s << "\033[0m\n";
        }
    }

    std::cerr << "\033[1;31m" << bar << "\033[0m\n"
              << "\033[1;31m" << bl;
    for (int i = 0; i < 24; ++i) std::cerr << horiz;
    std::cerr << "\033[0m\n";
}

void ErrorBox::render_error(const std::string& title, const std::string& message) {
    bool unicode = Terminal::supports_unicode();
    std::string tl = unicode ? "╭─" : "+-";
    std::string bl = unicode ? "╰" : "+";
    std::string bar = unicode ? "│" : "|";
    std::string horiz = unicode ? "─" : "-";

    std::cerr << "\033[1;31m" << tl << " " << title << "\033[0m\n"
              << "\033[1;31m" << bar << "\033[0m  " << message << "\n"
              << "\033[1;31m" << bl;
    for (int i = 0; i < 24; ++i) std::cerr << horiz;
    std::cerr << "\033[0m\n";
}

} // namespace aswell
