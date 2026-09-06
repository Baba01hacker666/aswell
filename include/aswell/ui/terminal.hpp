#pragma once

#include "aswell/common.hpp"
#include <sys/ioctl.h>

namespace aswell {

enum class Key {
    NONE = 0,
    CHAR,
    ENTER,
    BACKSPACE,
    TAB,
    BACKTAB,
    ESC,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    HOME,
    END,
    DELETE,
    PAGE_UP,
    PAGE_DOWN,
    CTRL_A,
    CTRL_B,
    CTRL_C,
    CTRL_D,
    CTRL_E,
    CTRL_F,
    CTRL_K,
    CTRL_L,
    CTRL_R,
    CTRL_U,
    CTRL_W,
    CTRL_Y,
    ALT_B,
    ALT_F,
    ALT_D,
    ALT_ENTER
};

struct KeyEvent {
    Key key = Key::NONE;
    std::string ch; // utf-8 char or string
};

struct TerminalSize {
    int rows = 24;
    int cols = 80;
};

class Terminal {
public:
    static bool is_interactive_tty();
    static bool supports_truecolor();
    static bool supports_unicode();
    static TerminalSize get_size();

    static void enable_raw_mode();
    static void disable_raw_mode();
    static bool is_raw_mode() { return raw_mode_active_; }

    // Read key with timeout in milliseconds (-1 for blocking)
    static KeyEvent read_key(int timeout_ms = -1);

    // ANSI Cursor & Screen control
    static void move_cursor(int row, int col);
    static void cursor_up(int n = 1);
    static void cursor_down(int n = 1);
    static void cursor_left(int n = 1);
    static void cursor_right(int n = 1);
    static void clear_screen();
    static void clear_line();
    static void clear_to_eol();
    static void hide_cursor();
    static void show_cursor();
    static void save_cursor();
    static void restore_cursor();

private:
    static bool raw_mode_active_;
    static struct termios orig_termios_;
};

class RawModeGuard {
public:
    RawModeGuard() { Terminal::enable_raw_mode(); }
    ~RawModeGuard() { Terminal::disable_raw_mode(); }
};

} // namespace aswell
