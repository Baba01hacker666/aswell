#include "aswell/shell/builtins.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace aswell {

namespace {

// termios flag tables: name + field selector.
struct FlagEntry {
    const char* name;
    int field; // 0=c_iflag 1=c_oflag 2=c_cflag 3=c_lflag
    tcflag_t bit;
};

constexpr int F_IFLAG = 0;
constexpr int F_OFLAG = 1;
constexpr int F_CFLAG = 2;
constexpr int F_LFLAG = 3;

const FlagEntry kInputFlags[] = {
    {"ignbrk", F_IFLAG, IGNBRK}, {"brkint", F_IFLAG, BRKINT},
    {"ignpar", F_IFLAG, IGNPAR}, {"parmrk", F_IFLAG, PARMRK},
    {"inpck", F_IFLAG, INPCK}, {"istrip", F_IFLAG, ISTRIP},
    {"inlcr", F_IFLAG, INLCR}, {"igncr", F_IFLAG, IGNCR},
    {"icrnl", F_IFLAG, ICRNL}, {"ixon", F_IFLAG, IXON},
    {"ixoff", F_IFLAG, IXOFF},
#ifdef IXANY
    {"ixany", F_IFLAG, IXANY},
#endif
#ifdef IMAXBEL
    {"imaxbel", F_IFLAG, IMAXBEL},
#endif
#ifdef IUCLC
    {"iuclc", F_IFLAG, IUCLC},
#endif
};

const FlagEntry kOutputFlags[] = {
    {"opost", F_OFLAG, OPOST},
#ifdef OLCUC
    {"olcuc", F_OFLAG, OLCUC},
#endif
    {"ocrnl", F_OFLAG, OCRNL}, {"onocr", F_OFLAG, ONOCR},
    {"onlret", F_OFLAG, ONLRET},
#ifdef OFILL
    {"ofill", F_OFLAG, OFILL},
#endif
#ifdef OFDEL
    {"ofdel", F_OFLAG, OFDEL},
#endif
};

const FlagEntry kControlFlags[] = {
    {"parenb", F_CFLAG, PARENB}, {"parodd", F_CFLAG, PARODD},
#ifdef CMSPAR
    {"cmspar", F_CFLAG, CMSPAR},
#endif
    {"hupcl", F_CFLAG, HUPCL}, {"cstopb", F_CFLAG, CSTOPB},
    {"cread", F_CFLAG, CREAD}, {"clocal", F_CFLAG, CLOCAL},
#ifdef CRTSCTS
    {"crtscts", F_CFLAG, CRTSCTS},
#endif
};

const FlagEntry kLocalFlags[] = {
    {"isig", F_LFLAG, ISIG}, {"icanon", F_LFLAG, ICANON},
#ifdef XCASE
    {"xcase", F_LFLAG, XCASE},
#endif
    {"echo", F_LFLAG, ECHO}, {"echoe", F_LFLAG, ECHOE},
    {"echok", F_LFLAG, ECHOK}, {"echonl", F_LFLAG, ECHONL},
    {"noflsh", F_LFLAG, NOFLSH}, {"tostop", F_LFLAG, TOSTOP},
#ifdef IEXTEN
    {"iexten", F_LFLAG, IEXTEN},
#endif
#ifdef ECHOCTL
    {"echoctl", F_LFLAG, ECHOCTL},
#endif
#ifdef ECHOKE
    {"echoke", F_LFLAG, ECHOKE},
#endif
#ifdef PENDIN
    {"pendin", F_LFLAG, PENDIN},
#endif
};

struct CcEntry {
    const char* name;
    int index;
};

const CcEntry kCc[] = {
    {"intr", VINTR}, {"quit", VQUIT}, {"erase", VERASE}, {"kill", VKILL},
    {"eof", VEOF}, {"eol", VEOL},
#ifdef VEOL2
    {"eol2", VEOL2},
#endif
    {"susp", VSUSP},
#ifdef VSWTC
    {"swtch", VSWTC},
#endif
    {"start", VSTART}, {"stop", VSTOP},
#ifdef VREPRINT
    {"rprnt", VREPRINT},
#endif
#ifdef VWERASE
    {"werase", VWERASE},
#endif
#ifdef VLNEXT
    {"lnext", VLNEXT},
#endif
#ifdef VFLUSH
    {"flush", VFLUSH},
#endif
#ifdef VDISCARD
    {"discard", VDISCARD},
#endif
};

struct SpeedEntry {
    int baud;
    speed_t speed;
};

const SpeedEntry kSpeeds[] = {
    {0, B0}, {50, B50}, {75, B75}, {110, B110}, {134, B134}, {150, B150},
    {200, B200}, {300, B300}, {600, B600}, {1200, B1200}, {1800, B1800},
    {2400, B2400}, {4800, B4800}, {9600, B9600}, {19200, B19200},
    {38400, B38400}, {57600, B57600}, {115200, B115200}, {230400, B230400},
#ifdef B460800
    {460800, B460800},
#endif
#ifdef B500000
    {500000, B500000},
#endif
#ifdef B576000
    {576000, B576000},
#endif
#ifdef B921600
    {921600, B921600},
#endif
#ifdef B1000000
    {1000000, B1000000},
#endif
#ifdef B1152000
    {1152000, B1152000},
#endif
#ifdef B1500000
    {1500000, B1500000},
#endif
#ifdef B2000000
    {2000000, B2000000},
#endif
#ifdef B2500000
    {2500000, B2500000},
#endif
#ifdef B3000000
    {3000000, B3000000},
#endif
#ifdef B3500000
    {3500000, B3500000},
#endif
#ifdef B4000000
    {4000000, B4000000},
#endif
};

const FlagEntry* find_flag(const std::string& name) {
    for (const auto& e : kInputFlags) {
        if (name == e.name) return &e;
    }
    for (const auto& e : kOutputFlags) {
        if (name == e.name) return &e;
    }
    for (const auto& e : kControlFlags) {
        if (name == e.name) return &e;
    }
    for (const auto& e : kLocalFlags) {
        if (name == e.name) return &e;
    }
    return nullptr;
}

const CcEntry* find_cc(const std::string& name) {
    for (const auto& e : kCc) {
        if (name == e.name) return &e;
    }
    return nullptr;
}

speed_t baud_to_speed(int baud, bool& ok) {
    for (const auto& e : kSpeeds) {
        if (e.baud == baud) {
            ok = true;
            return e.speed;
        }
    }
    ok = false;
    return B9600;
}

int speed_to_baud(speed_t speed) {
    for (const auto& e : kSpeeds) {
        if (e.speed == speed) return e.baud;
    }
    return -1;
}

tcflag_t* field_ptr(struct termios& tio, int field) {
    switch (field) {
        case F_IFLAG: return &tio.c_iflag;
        case F_OFLAG: return &tio.c_oflag;
        case F_CFLAG: return &tio.c_cflag;
        default: return &tio.c_lflag;
    }
}

// Parse a control-character value: ^X, ^-, undef, a single char, or a
// decimal number. Returns false on invalid input.
bool parse_cc_value(const std::string& s, cc_t& out) {
    if (s == "^-" || s == "undef") {
        out = static_cast<cc_t>(0); // _POSIX_VDISABLE on Linux
        return true;
    }
    if (s.size() == 2 && s[0] == '^') {
        char c = s[1];
        if (c == '?') {
            out = static_cast<cc_t>(127);
            return true;
        }
        if (c >= '@' && c <= '_') {
            out = static_cast<cc_t>(c - '@');
            return true;
        }
        if (c >= 'a' && c <= 'z') {
            out = static_cast<cc_t>(c - 'a' + 1);
            return true;
        }
        return false;
    }
    if (s.size() == 1) {
        out = static_cast<cc_t>(static_cast<unsigned char>(s[0]));
        return true;
    }
    try {
        size_t pos = 0;
        long v = std::stol(s, &pos);
        if (pos != s.size() || v < 0 || v > 255) return false;
        out = static_cast<cc_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

std::string format_cc(cc_t v) {
    if (v == 0) return "<undef>";
    if (v < 32) {
        std::string s = "^";
        s += static_cast<char>(v + '@');
        return s;
    }
    if (v == 127) return "^?";
    if (v >= 32 && v < 127) return std::string(1, static_cast<char>(v));
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(v));
    return buf;
}

int char_size_bits(tcflag_t cflag) {
    switch (cflag & CSIZE) {
        case CS5: return 5;
        case CS6: return 6;
        case CS7: return 7;
        default: return 8;
    }
}

void print_flags(std::ostream& os, const struct termios& tio, bool all) {
    auto field_of = [&](int field) -> tcflag_t {
        switch (field) {
            case F_IFLAG: return tio.c_iflag;
            case F_OFLAG: return tio.c_oflag;
            case F_CFLAG: return tio.c_cflag;
            default: return tio.c_lflag;
        }
    };
    auto show = [&](const FlagEntry* table, size_t n) {
        bool first = true;
        for (size_t i = 0; i < n; ++i) {
            const FlagEntry& e = table[i];
            bool on = (field_of(e.field) & e.bit) != 0;
            if (!all && !on) continue;
            if (!first) os << " ";
            first = false;
            if (!on) os << "-";
            os << e.name;
        }
        if (!first) os << "\n";
    };
    show(kInputFlags, sizeof(kInputFlags) / sizeof(kInputFlags[0]));
    show(kOutputFlags, sizeof(kOutputFlags) / sizeof(kOutputFlags[0]));
    // Control flags: character size renders as csN (like GNU).
    {
        bool first = true;
        for (const auto& e : kControlFlags) {
            bool on = (tio.c_cflag & e.bit) != 0;
            if (!all && !on) continue;
            if (!first) os << " ";
            first = false;
            if (!on) os << "-";
            os << e.name;
        }
        if (!first) os << " ";
        os << "cs" << char_size_bits(tio.c_cflag) << "\n";
    }
    show(kLocalFlags, sizeof(kLocalFlags) / sizeof(kLocalFlags[0]));
}

void print_cc_line(std::ostream& os, const struct termios& tio) {
    bool first = true;
    for (const auto& e : kCc) {
        if (!first) os << "; ";
        first = false;
        os << e.name << " = " << format_cc(tio.c_cc[e.index]);
    }
    os << "; min = " << static_cast<unsigned>(tio.c_cc[VMIN]);
    os << "; time = " << static_cast<unsigned>(tio.c_cc[VTIME]) << ";\n";
}

// Aswell save format: numeric termios dump our own -g parser reads back.
// (GNU's -g blob is glibc-opaque, so we use a documented stable format.)
const std::string kSavePrefix = "aswell-stty:";

std::string save_settings(const struct termios& tio, int rows, int cols) {
    std::ostringstream ss;
    ss << kSavePrefix << tio.c_iflag << ":" << tio.c_oflag << ":" << tio.c_cflag << ":"
       << tio.c_lflag << ":" << static_cast<unsigned>(tio.c_line) << ":";
    for (int i = 0; i < NCCS; ++i) {
        if (i > 0) ss << ",";
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned>(tio.c_cc[i]);
    }
    ss << std::dec << ":" << rows << ":" << cols;
    return ss.str();
}

bool restore_settings(const std::string& s, struct termios& tio, int& rows, int& cols) {
    if (s.compare(0, kSavePrefix.size(), kSavePrefix) != 0) return false;
    std::vector<std::string> parts = str_util::split(s.substr(kSavePrefix.size()), ':');
    // iflag:oflag:cflag:lflag:line:cc(rows of hex):rows:cols
    if (parts.size() != 8) return false;
    try {
        size_t pos = 0;
        tio.c_iflag = static_cast<tcflag_t>(std::stoul(parts[0], &pos));
        if (pos != parts[0].size()) return false;
        tio.c_oflag = static_cast<tcflag_t>(std::stoul(parts[1], &pos));
        if (pos != parts[1].size()) return false;
        tio.c_cflag = static_cast<tcflag_t>(std::stoul(parts[2], &pos));
        if (pos != parts[2].size()) return false;
        tio.c_lflag = static_cast<tcflag_t>(std::stoul(parts[3], &pos));
        if (pos != parts[3].size()) return false;
        unsigned long line = std::stoul(parts[4], &pos);
        if (pos != parts[4].size() || line > 255) return false;
        tio.c_line = static_cast<cc_t>(line);
        std::vector<std::string> cc = str_util::split(parts[5], ',');
        if (static_cast<int>(cc.size()) != NCCS) return false;
        for (int i = 0; i < NCCS; ++i) {
            if (cc[static_cast<size_t>(i)].size() != 2) return false;
            unsigned long v = std::stoul(cc[static_cast<size_t>(i)], &pos, 16);
            if (pos != 2 || v > 255) return false;
            tio.c_cc[i] = static_cast<cc_t>(v);
        }
        long r = std::stol(parts[6], &pos);
        if (pos != parts[6].size() || r < 0 || r > 10000) return false;
        long c = std::stol(parts[7], &pos);
        if (pos != parts[7].size() || c < 0 || c > 10000) return false;
        rows = static_cast<int>(r);
        cols = static_cast<int>(c);
        return true;
    } catch (...) {
        return false;
    }
}

void print_short_status(const struct termios& tio, int rows, int cols) {
    int ibaud = speed_to_baud(cfgetispeed(&tio));
    int obaud = speed_to_baud(cfgetospeed(&tio));
    int baud = ibaud >= 0 ? ibaud : obaud;
    std::cout << "speed " << baud << " baud; rows " << rows << "; columns " << cols << "; line = "
              << static_cast<unsigned>(tio.c_line) << ";\n";
    print_cc_line(std::cout, tio);
    print_flags(std::cout, tio, false);
}

void print_all_status(const struct termios& tio, int rows, int cols) {
    int ibaud = speed_to_baud(cfgetispeed(&tio));
    int obaud = speed_to_baud(cfgetospeed(&tio));
    int baud = ibaud >= 0 ? ibaud : obaud;
    std::cout << "speed " << baud << " baud; rows " << rows << "; columns " << cols
              << "; line = " << static_cast<unsigned>(tio.c_line) << ";\n";
    print_cc_line(std::cout, tio);
    print_flags(std::cout, tio, true);
}

void apply_sane(struct termios& tio) {
    tio.c_iflag = BRKINT | ICRNL | IXON;
#ifdef IMAXBEL
    tio.c_iflag |= IMAXBEL;
#endif
    tio.c_oflag = OPOST | ONLCR;
    tio.c_cflag = CREAD | CS8;
    tio.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
#ifdef IEXTEN
    tio.c_lflag |= IEXTEN;
#endif
#ifdef ECHOCTL
    tio.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
    tio.c_lflag |= ECHOKE;
#endif
    const cc_t defaults[][2] = {
        {VINTR, 3}, {VQUIT, 28}, {VERASE, 127}, {VKILL, 21}, {VEOF, 4},
        {VEOL, 0}, {VMIN, 1}, {VTIME, 0}, {VSUSP, 26}, {VSTART, 17}, {VSTOP, 19},
    };
    for (const auto& d : defaults) tio.c_cc[d[0]] = d[1];
#ifdef VEOL2
    tio.c_cc[VEOL2] = 0;
#endif
#ifdef VREPRINT
    tio.c_cc[VREPRINT] = 18;
#endif
#ifdef VWERASE
    tio.c_cc[VWERASE] = 23;
#endif
#ifdef VLNEXT
    tio.c_cc[VLNEXT] = 22;
#endif
#ifdef VDISCARD
    tio.c_cc[VDISCARD] = 0;
#endif
}

void apply_raw(struct termios& tio) {
    tio.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    tio.c_oflag &= static_cast<tcflag_t>(~OPOST);
    tio.c_cflag |= CS8;
    tio.c_cflag &= static_cast<tcflag_t>(~(PARENB));
    tio.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
}

bool parse_int_operand(const std::string& s, long& out) {
    try {
        size_t pos = 0;
        out = std::stol(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

void print_stty_help() {    std::cout
        << "Usage: stty [-F DEVICE] [SETTING]... | stty [-a | -g | --help]\n"
        << "Display or change terminal line settings.\n\n"
        << "Display: (no args) short status; -a long status; -g saved settings;\n"
        << "  size (rows cols); speed (baud rate).\n"
        << "Set: sane | raw | cooked | echo | -echo | icanon | -icanon | isig | -isig |\n"
        << "  ixon | -ixon | ixoff | -ixoff | opost | -opost | icrnl | -icrnl |\n"
        << "  rows N | cols N | columns N | speed N | ispeed N | ospeed N |\n"
        << "  erase C | kill C | intr C | quit C | susp C | eof C | ... (C is ^X, ^-, undef)\n"
        << "Restore: stty \"$(stty -g)\"  (aswell save format)\n";
}

} // namespace

// Validate operands without touching any terminal, so typos are reported
// even when stdin is not a tty (matching GNU stty ordering).
bool validate_operands(const std::vector<std::string>& operands) {
    for (size_t i = 0; i < operands.size(); ++i) {
        const std::string& op = operands[i];
        auto need = [&](const char* what, std::string& out) {
            if (i + 1 >= operands.size()) {
                std::cerr << "aswell: stty: '" << op << "': option requires an argument (" << what
                          << ")\n";
                return false;
            }
            out = operands[++i];
            return true;
        };
        if (op == "-a" || op == "-g" || op == "size" || op == "sane" || op == "raw" ||
            op == "cooked" || op == "-raw" || op == "--") {
            continue;
        }
        if ((op == "speed" || op == "ispeed" || op == "ospeed" || op == "baud") &&
            !(op == "speed" && i + 1 >= operands.size())) {
            std::string v;
            if (!need("speed", v)) return false;
            long n = 0;
            bool ok = parse_int_operand(v, n) && n >= 0 && n <= 10000000;
            if (ok) baud_to_speed(static_cast<int>(n), ok);
            if (!ok) {
                std::cerr << "aswell: stty: invalid speed '" << v << "'\n";
                return false;
            }
            continue;
        }
        if (op == "speed") continue; // bare trailing "speed": display verb
        if (op == "rows" || op == "cols" || op == "columns" || op == "min" || op == "time") {
            std::string v;
            long n = 0;
            long limit = (op == "min" || op == "time") ? 255 : 10000;
            if (!need(op.c_str(), v) || !parse_int_operand(v, n) || n < 0 || n > limit) {
                std::cerr << "aswell: stty: invalid " << op << " value '" << v << "'\n";
                return false;
            }
            continue;
        }
        if (!op.empty() && op[0] != '-') {
            if (find_flag(op) != nullptr) continue;
            const CcEntry* cc = find_cc(op);
            if (cc != nullptr) {
                std::string v;
                cc_t val = 0;
                if (!need(op.c_str(), v) || !parse_cc_value(v, val)) {
                    std::cerr << "aswell: stty: invalid " << op << " value '" << v << "'\n";
                    return false;
                }
                continue;
            }
            if (op.compare(0, kSavePrefix.size(), kSavePrefix) == 0) {
                struct termios tmp{};
                int r = 0;
                int c = 0;
                if (!restore_settings(op, tmp, r, c)) {
                    std::cerr << "aswell: stty: invalid saved settings\n";
                    return false;
                }
                continue;
            }
            std::cerr << "aswell: stty: invalid argument '" << op << "'\n";
            return false;
        }
        if (op.size() > 1 && find_flag(op.substr(1)) != nullptr) continue;
        std::cerr << "aswell: stty: invalid argument '" << op << "'\n";
        return false;
    }
    return true;
}

int Builtins::builtin_stty(const std::vector<std::string>& args, Environment& /*env*/) {
    std::string device;
    std::vector<std::string> operands;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-F" && i + 1 < args.size()) {
            device = args[++i];
        } else if (args[i].compare(0, 7, "--file=") == 0) {
            device = args[i].substr(7);
        } else {
            operands.push_back(args[i]);
        }
    }

    for (const auto& op : operands) {
        if (op == "--help" || op == "-h") {
            print_stty_help();
            return 0;
        }
    }

    // Validate first so typos are reported even when stdin is not a tty.
    if (!validate_operands(operands)) {
        return 1;
    }

    int fd = STDIN_FILENO;
    int owned_fd = -1;
    if (!device.empty()) {
        owned_fd = open(device.c_str(), O_RDWR | O_NOCTTY);
        if (owned_fd < 0) {
            std::cerr << "aswell: stty: " << device << ": " << std::strerror(errno) << "\n";
            return 1;
        }
        fd = owned_fd;
    }

    struct termios tio{};
    if (tcgetattr(fd, &tio) != 0) {
        std::string who = device.empty() ? "standard input" : device;
        std::cerr << "aswell: stty: '" << who << "': " << std::strerror(errno) << "\n";
        if (owned_fd >= 0) close(owned_fd);
        return 1;
    }

    struct winsize ws{};
    int rows = 0;
    int cols = 0;
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }

    bool show_all = false;
    bool show_save = false;
    bool show_size = false;
    bool show_speed = false;
    bool changed = false;
    bool winsize_changed = false;
    int rc = 0;

    for (size_t i = 0; i < operands.size() && rc == 0; ++i) {
        const std::string& op = operands[i];
        auto need_value = [&](const char* what, std::string& out) {
            if (i + 1 >= operands.size()) {
                std::cerr << "aswell: stty: '" << op << "': option requires an argument (" << what
                          << ")\n";
                rc = 1;
                return false;
            }
            out = operands[++i];
            return true;
        };

        if (op == "-a") {
            show_all = true;
        } else if (op == "-g") {
            show_save = true;
        } else if (op == "size") {
            show_size = true;
        } else if (op == "speed" && i + 1 >= operands.size()) {
            // Bare trailing "speed" displays; otherwise it sets (below).
            show_speed = true;
        } else if (op == "sane") {
            apply_sane(tio);
            changed = true;
        } else if (op == "raw") {
            apply_raw(tio);
            changed = true;
        } else if (op == "cooked" || op == "-raw") {
            tio.c_lflag |= (ICANON | ISIG);
            tio.c_iflag |= (BRKINT | ICRNL);
            tio.c_oflag |= OPOST;
            changed = true;
        } else if (op == "rows") {
            std::string v;
            long n = 0;
            if (!need_value("rows", v) || !parse_int_operand(v, n) || n < 0 || n > 10000) {
                if (rc == 0) {
                    std::cerr << "aswell: stty: invalid rows '" << v << "'\n";
                    rc = 1;
                }
            } else {
                ws.ws_row = static_cast<unsigned short>(n);
                rows = static_cast<int>(n);
                winsize_changed = true;
            }
        } else if (op == "cols" || op == "columns") {
            std::string v;
            long n = 0;
            if (!need_value("columns", v) || !parse_int_operand(v, n) || n < 0 || n > 10000) {
                if (rc == 0) {
                    std::cerr << "aswell: stty: invalid columns '" << v << "'\n";
                    rc = 1;
                }
            } else {
                ws.ws_col = static_cast<unsigned short>(n);
                cols = static_cast<int>(n);
                winsize_changed = true;
            }
        } else if (op == "speed" || op == "ispeed" || op == "ospeed" || op == "baud") {
            std::string v;
            long n = 0;
            if (!need_value("speed", v) || !parse_int_operand(v, n) || n < 0 || n > 10000000) {
                if (rc == 0) {
                    std::cerr << "aswell: stty: invalid speed '" << v << "'\n";
                    rc = 1;
                }
            } else {
                bool ok = false;
                speed_t sp = baud_to_speed(static_cast<int>(n), ok);
                if (!ok) {
                    std::cerr << "aswell: stty: unsupported speed '" << v << "'\n";
                    rc = 1;
                } else {
                    if (op == "ospeed") {
                        cfsetospeed(&tio, sp);
                    } else if (op == "ispeed") {
                        cfsetispeed(&tio, sp);
                    } else {
                        cfsetispeed(&tio, sp);
                        cfsetospeed(&tio, sp);
                    }
                    changed = true;
                }
            }
        } else if (!op.empty() && op[0] != '-') {
            // Bare words: a set-flag, a control-char name, min/time, or a
            // save-restore blob (display verbs were handled above).
            const FlagEntry* fe = find_flag(op);
            const CcEntry* cc = (fe == nullptr) ? find_cc(op) : nullptr;
            if (fe != nullptr) {
                *field_ptr(tio, fe->field) |= fe->bit;
                changed = true;
            } else if (cc != nullptr) {
                std::string v;
                cc_t val = 0;
                if (!need_value(op.c_str(), v) || !parse_cc_value(v, val)) {
                    if (rc == 0) {
                        std::cerr << "aswell: stty: invalid " << op << " value '" << v << "'\n";
                        rc = 1;
                    }
                } else {
                    tio.c_cc[cc->index] = val;
                    changed = true;
                }
            } else if (op == "min" || op == "time") {
                std::string v;
                long n = 0;
                if (!need_value(op.c_str(), v) || !parse_int_operand(v, n) || n < 0 || n > 255) {
                    if (rc == 0) {
                        std::cerr << "aswell: stty: invalid " << op << " value '" << v << "'\n";
                        rc = 1;
                    }
                } else {
                    tio.c_cc[op == "min" ? VMIN : VTIME] = static_cast<cc_t>(n);
                    changed = true;
                }
            } else if (op.compare(0, kSavePrefix.size(), kSavePrefix) == 0) {
                int r = rows;
                int c = cols;
                if (!restore_settings(op, tio, r, c)) {
                    std::cerr << "aswell: stty: invalid saved settings\n";
                    rc = 1;
                } else {
                    rows = r;
                    cols = c;
                    ws.ws_row = static_cast<unsigned short>(r > 0 ? r : 0);
                    ws.ws_col = static_cast<unsigned short>(c > 0 ? c : 0);
                    changed = true;
                    winsize_changed = (r > 0 && c > 0);
                }
            } else {
                std::cerr << "aswell: stty: invalid argument '" << op << "'\n";
                rc = 1;
            }
        } else if (op == "--") {
            // End-of-options marker: nothing to do.
        } else {
            // -flag or unknown dash option.
            std::string name = op.substr(1);
            const FlagEntry* e = find_flag(name);
            if (e == nullptr) {
                std::cerr << "aswell: stty: invalid argument '" << op << "'\n";
                rc = 1;
            } else {
                *field_ptr(tio, e->field) &= static_cast<tcflag_t>(~e->bit);
                changed = true;
            }
        }
    }

    if (rc == 0 && (changed || winsize_changed)) {
        if (changed && tcsetattr(fd, TCSADRAIN, &tio) != 0) {
            std::cerr << "aswell: stty: tcsetattr: " << std::strerror(errno) << "\n";
            rc = 1;
        } else if (winsize_changed && ioctl(fd, TIOCSWINSZ, &ws) != 0) {
            std::cerr << "aswell: stty: TIOCSWINSZ: " << std::strerror(errno) << "\n";
            rc = 1;
        } else if (tcgetattr(fd, &tio) == 0) {
            if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
                rows = ws.ws_row;
                cols = ws.ws_col;
            }
        }
    }

    if (rc == 0) {
        if (show_save) {
            std::cout << save_settings(tio, rows, cols) << "\n";
        } else if (show_all) {
            print_all_status(tio, rows, cols);
        } else if (show_size) {
            std::cout << rows << " " << cols << "\n";
        } else if (show_speed) {
            int ibaud = speed_to_baud(cfgetispeed(&tio));
            std::cout << (ibaud >= 0 ? ibaud : 0) << "\n";
        } else if (operands.empty() || changed || winsize_changed) {
            print_short_status(tio, rows, cols);
        }
    }

    if (owned_fd >= 0) close(owned_fd);
    return rc;
}

} // namespace aswell
