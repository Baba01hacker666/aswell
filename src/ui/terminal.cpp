#include "aswell/ui/terminal.hpp"
#include <sys/select.h>

namespace aswell {

bool Terminal::raw_mode_active_ = false;
struct termios Terminal::orig_termios_;

bool Terminal::is_interactive_tty() {
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

bool Terminal::supports_truecolor() {
    const char* ct = std::getenv("COLORTERM");
    if (ct && (std::strcmp(ct, "truecolor") == 0 || std::strcmp(ct, "24bit") == 0)) {
        return true;
    }
    const char* term = std::getenv("TERM");
    if (term) {
        if (std::strstr(term, "24bit") || std::strstr(term, "truecolor") ||
            std::strstr(term, "kitty") || std::strstr(term, "alacritty") ||
            std::strstr(term, "xterm-256color") || std::strstr(term, "foot") ||
            std::strstr(term, "wezterm")) {
            return true;
        }
    }
    return false;
}

bool Terminal::supports_unicode() {
    const char* lang = std::getenv("LC_ALL");
    if (!lang) lang = std::getenv("LC_CTYPE");
    if (!lang) lang = std::getenv("LANG");
    if (lang && (std::strstr(lang, "UTF-8") || std::strstr(lang, "utf8") || std::strstr(lang, "UTF8"))) {
        return true;
    }
    return true; // Modern Linux systems default to UTF-8
}

TerminalSize Terminal::get_size() {
    TerminalSize size;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        size.rows = ws.ws_row;
        size.cols = ws.ws_col;
    }
    return size;
}

void Terminal::enable_raw_mode() {
    if (raw_mode_active_ || !isatty(STDIN_FILENO)) return;

    if (tcgetattr(STDIN_FILENO, &orig_termios_) == 0) {
        struct termios raw = orig_termios_;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
            raw_mode_active_ = true;
        }
    }
}

void Terminal::disable_raw_mode() {
    if (!raw_mode_active_ || !isatty(STDIN_FILENO)) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
    raw_mode_active_ = false;
}

KeyEvent Terminal::read_key(int timeout_ms) {
    if (timeout_ms >= 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) {
            return {Key::NONE, ""};
        }
    }

    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) {
        return {Key::NONE, ""};
    }

    if (c == '\r' || c == '\n') return {Key::ENTER, "\n"};
    if (c == 127 || c == '\b') return {Key::BACKSPACE, ""};
    if (c == '\t') return {Key::TAB, "\t"};

    // Ctrl keys
    if (c == 1) return {Key::CTRL_A, ""};
    if (c == 2) return {Key::CTRL_B, ""};
    if (c == 3) return {Key::CTRL_C, ""};
    if (c == 4) return {Key::CTRL_D, ""};
    if (c == 5) return {Key::CTRL_E, ""};
    if (c == 6) return {Key::CTRL_F, ""};
    if (c == 11) return {Key::CTRL_K, ""};
    if (c == 12) return {Key::CTRL_L, ""};
    if (c == 18) return {Key::CTRL_R, ""};
    if (c == 21) return {Key::CTRL_U, ""};
    if (c == 23) return {Key::CTRL_W, ""};
    if (c == 25) return {Key::CTRL_Y, ""};

    // Escape sequences
    if (c == 27) { // ESC
        // Check if more chars available immediately
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 30000}; // 30ms timeout
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) {
            return {Key::ESC, ""};
        }

        char seq[8];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return {Key::ESC, ""};

        if (seq[0] == '[') {
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) return {Key::ESC, ""};

            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) <= 0) return {Key::ESC, ""};
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return {Key::HOME, ""};
                        case '3': return {Key::DELETE, ""};
                        case '4': return {Key::END, ""};
                        case '5': return {Key::PAGE_UP, ""};
                        case '6': return {Key::PAGE_DOWN, ""};
                        case '7': return {Key::HOME, ""};
                        case '8': return {Key::END, ""};
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return {Key::UP, ""};
                    case 'B': return {Key::DOWN, ""};
                    case 'C': return {Key::RIGHT, ""};
                    case 'D': return {Key::LEFT, ""};
                    case 'H': return {Key::HOME, ""};
                    case 'F': return {Key::END, ""};
                    case 'Z': return {Key::BACKTAB, ""};
                }
            }
        } else if (seq[0] == 'O') {
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) return {Key::ESC, ""};
            switch (seq[1]) {
                case 'H': return {Key::HOME, ""};
                case 'F': return {Key::END, ""};
            }
        } else if (seq[0] == 'b') {
            return {Key::ALT_B, ""};
        } else if (seq[0] == 'f') {
            return {Key::ALT_F, ""};
        } else if (seq[0] == 'd') {
            return {Key::ALT_D, ""};
        } else if (seq[0] == '\r' || seq[0] == '\n') {
            return {Key::ALT_ENTER, ""};
        }

        return {Key::ESC, ""};
    }

    // UTF-8 Multibyte character reading
    unsigned char uc = static_cast<unsigned char>(c);
    std::string utf8_char(1, c);
    int extra_bytes = 0;
    if ((uc & 0xE0) == 0xC0) extra_bytes = 1;
    else if ((uc & 0xF0) == 0xE0) extra_bytes = 2;
    else if ((uc & 0xF8) == 0xF0) extra_bytes = 3;

    for (int i = 0; i < extra_bytes; ++i) {
        char extra_c = 0;
        if (read(STDIN_FILENO, &extra_c, 1) > 0) {
            utf8_char += extra_c;
        }
    }

    return {Key::CHAR, utf8_char};
}

void Terminal::move_cursor(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H";
}

void Terminal::cursor_up(int n) {
    if (n > 0) std::cout << "\033[" << n << "A";
}

void Terminal::cursor_down(int n) {
    if (n > 0) std::cout << "\033[" << n << "B";
}

void Terminal::cursor_left(int n) {
    if (n > 0) std::cout << "\033[" << n << "D";
}

void Terminal::cursor_right(int n) {
    if (n > 0) std::cout << "\033[" << n << "C";
}

void Terminal::clear_screen() {
    std::cout << "\033[2J\033[H";
}

void Terminal::clear_line() {
    std::cout << "\033[2K\r";
}

void Terminal::clear_to_eol() {
    std::cout << "\033[K";
}

void Terminal::hide_cursor() {
    std::cout << "\033[?25l";
}

void Terminal::show_cursor() {
    std::cout << "\033[?25h";
}

void Terminal::save_cursor() {
    std::cout << "\033[s";
}

void Terminal::restore_cursor() {
    std::cout << "\033[u";
}

} // namespace aswell
