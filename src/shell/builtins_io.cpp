#include "aswell/shell/builtins.hpp"
#include "aswell/ui/color.hpp"
#include "aswell/ui/template_engine.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <iomanip>
#include <cctype>

namespace aswell {

static bool parse_octal_escape(const std::string& str, size_t& idx, char& out_char) {
    int val = 0;
    int count = 0;
    while (idx < str.size() && count < 3 && str[idx] >= '0' && str[idx] <= '7') {
        val = val * 8 + (str[idx] - '0');
        idx++;
        count++;
    }
    if (count > 0) {
        out_char = static_cast<char>(val & 0xFF);
        return true;
    }
    return false;
}

static bool parse_hex_escape(const std::string& str, size_t& idx, char& out_char) {
    int val = 0;
    int count = 0;
    while (idx < str.size() && count < 2 && std::isxdigit(static_cast<unsigned char>(str[idx]))) {
        char c = str[idx];
        int d = 0;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        val = val * 16 + d;
        idx++;
        count++;
    }
    if (count > 0) {
        out_char = static_cast<char>(val & 0xFF);
        return true;
    }
    return false;
}

static std::string expand_escapes(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t j = 0; j < str.size(); ++j) {
        if (str[j] == '\\' && j + 1 < str.size()) {
            char next = str[j + 1];
            if (next == 'n') { result += '\n'; j++; }
            else if (next == 't') { result += '\t'; j++; }
            else if (next == 'r') { result += '\r'; j++; }
            else if (next == 'a') { result += '\a'; j++; }
            else if (next == 'b') { result += '\b'; j++; }
            else if (next == 'f') { result += '\f'; j++; }
            else if (next == 'v') { result += '\v'; j++; }
            else if (next == 'e' || next == 'E') { result += '\033'; j++; }
            else if (next == '\\') { result += '\\'; j++; }
            else if (next == 'c') { break; }
            else if (next == '0' || (next >= '1' && next <= '7')) {
                size_t p = j + 1;
                char out_c = 0;
                if (parse_octal_escape(str, p, out_c)) {
                    result += out_c;
                    j = p - 1;
                } else {
                    result += next;
                    j++;
                }
            } else if (next == 'x' && j + 2 < str.size()) {
                size_t p = j + 2;
                char out_c = 0;
                if (parse_hex_escape(str, p, out_c)) {
                    result += out_c;
                    j = p - 1;
                } else {
                    result += "\\x";
                    j++;
                }
            } else {
                result += next;
                j++;
            }
        } else {
            result += str[j];
        }
    }
    return result;
}

static void print_rainbow(const std::string& text, bool newline) {
    static const Color rainbow_colors[] = {
        Color(255, 85, 85),    // Red
        Color(255, 184, 108),  // Orange
        Color(241, 250, 140),  // Yellow
        Color(80, 250, 123),   // Green
        Color(139, 233, 253),  // Cyan
        Color(98, 114, 164),   // Blue
        Color(189, 147, 249),  // Purple
        Color(255, 121, 198)   // Pink
    };
    size_t num_colors = sizeof(rainbow_colors) / sizeof(rainbow_colors[0]);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == ' ' || text[i] == '\t') {
            std::cout << text[i];
        } else {
            const Color& c = rainbow_colors[i % num_colors];
            std::cout << c.to_fg_ansi(true) << text[i];
        }
    }
    std::cout << "\033[0m";
    if (newline) std::cout << '\n';
    std::cout.flush();
}

static void print_gradient(const std::string& text, const Color& c1, const Color& c2, bool newline) {
    size_t len = text.size();
    for (size_t i = 0; i < len; ++i) {
        if (text[i] == ' ' || text[i] == '\t') {
            std::cout << text[i];
        } else {
            float t = (len > 1) ? static_cast<float>(i) / static_cast<float>(len - 1) : 0.0f;
            Color c = c1.blend(c2, t);
            std::cout << c.to_fg_ansi(true) << text[i];
        }
    }
    std::cout << "\033[0m";
    if (newline) std::cout << '\n';
    std::cout.flush();
}

static void print_color_list() {
    std::cout << "\033[1;36m╭─────────────────────────────────────────────────────────────╮\033[0m\n"
              << "\033[1;36m│\033[0m                 \033[1;37mASWELL COLOR ENGINE PALETTES\033[0m                \033[1;36m│\033[0m\n"
              << "\033[1;36m╰─────────────────────────────────────────────────────────────╯\033[0m\n\n";

    auto show_badge = [](const std::string& name, const Color& c) {
        std::cout << "  " << c.to_fg_ansi(true) << "● " << std::left << std::setw(16) << name << "\033[0m";
    };

    std::cout << "\033[1;33mStandard Colors:\033[0m\n";
    std::vector<std::string> std_names = {"red", "green", "yellow", "blue", "magenta", "cyan", "white", "gray", "orange", "pink"};
    for (size_t i = 0; i < std_names.size(); ++i) {
        show_badge(std_names[i], Color::from_name(std_names[i]));
        if ((i + 1) % 4 == 0 || i == std_names.size() - 1) std::cout << "\n";
    }

    std::cout << "\n\033[1;33mBright ANSI Colors:\033[0m\n";
    std::vector<std::string> bright_names = {"bright-red", "bright-green", "bright-yellow", "bright-blue", "bright-magenta", "bright-cyan", "bright-white"};
    for (size_t i = 0; i < bright_names.size(); ++i) {
        show_badge(bright_names[i], Color::from_name(bright_names[i]));
        if ((i + 1) % 4 == 0 || i == bright_names.size() - 1) std::cout << "\n";
    }

    std::cout << "\n\033[1;33mNord Palette:\033[0m\n";
    for (int i = 0; i <= 15; ++i) {
        std::string n = "nord" + std::to_string(i);
        show_badge(n, Color::from_name(n));
        if ((i + 1) % 4 == 0) std::cout << "\n";
    }

    std::cout << "\n\033[1;33mDracula Palette:\033[0m\n";
    std::vector<std::string> drac_names = {"dracula-red", "dracula-green", "dracula-yellow", "dracula-cyan", "dracula-purple", "dracula-orange", "dracula-pink"};
    for (size_t i = 0; i < drac_names.size(); ++i) {
        show_badge(drac_names[i], Color::from_name(drac_names[i]));
        if ((i + 1) % 4 == 0 || i == drac_names.size() - 1) std::cout << "\n";
    }

    std::cout << "\n\033[1;33mCyberpunk Palette:\033[0m\n";
    std::vector<std::string> cyber_names = {"neon-cyan", "neon-yellow", "neon-pink", "cyber-blue"};
    for (const auto& n : cyber_names) {
        show_badge(n, Color::from_name(n));
    }
    std::cout << "\n\n\033[1;33mHex, RGB & 256-Color Codes:\033[0m\n"
              << "  Any 24-bit TrueColor hex: \033[38;2;0;240;255m#00f0ff\033[0m, \033[38;2;255;0;128m#ff0080\033[0m, \033[38;2;80;250;123m#50fa7b\033[0m\n"
              << "  Any RGB code: rgb(255, 80, 100)\n"
              << "  Any numeric 256-color code: 0 .. 255 (e.g. 196, 46, 226)\n";
}

int Builtins::builtin_cd(const std::vector<std::string>& args, Environment& env) {
    std::string target;
    if (args.size() <= 1) {
        target = env.get_var("HOME");
        if (target.empty()) {
            std::cerr << "aswell: cd: HOME not set\n";
            return 1;
        }
    } else if (args[1] == "-") {
        target = env.get_var("OLDPWD");
        if (target.empty()) {
            std::cerr << "aswell: cd: OLDPWD not set\n";
            return 1;
        }
        std::cout << target << "\n";
    } else {
        target = args[1];
    }

    char prev_buf[4096];
    if (getcwd(prev_buf, sizeof(prev_buf))) {
        env.set_var("OLDPWD", prev_buf, true);
    }

    if (chdir(target.c_str()) != 0) {
        std::cerr << "aswell: cd: " << target << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    char new_buf[4096];
    if (getcwd(new_buf, sizeof(new_buf))) {
        env.set_var("PWD", new_buf, true);
    }

    return 0;
}

int Builtins::builtin_pwd(const std::vector<std::string>& args, Environment& env) {
    bool physical = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-P") physical = true;
        else if (args[i] == "-L") physical = false;
    }

    if (!physical) {
        std::string pwd = env.get_var("PWD");
        if (!pwd.empty()) {
            std::cout << pwd << "\n";
            return 0;
        }
    }

    char buf[4096];
    if (getcwd(buf, sizeof(buf))) {
        std::cout << buf << "\n";
        return 0;
    }
    return 1;
}

int Builtins::builtin_echo(const std::vector<std::string>& args, Environment& /*env*/) {
    bool newline = true;
    bool interpret_escapes = false;
    size_t idx = 1;

    while (idx < args.size()) {
        if (args[idx] == "-n") {
            newline = false;
            idx++;
        } else if (args[idx] == "-e") {
            interpret_escapes = true;
            idx++;
        } else if (args[idx] == "-E") {
            interpret_escapes = false;
            idx++;
        } else {
            break;
        }
    }

    for (size_t i = idx; i < args.size(); ++i) {
        if (i > idx) std::cout << ' ';
        const std::string& str = args[i];
        if (interpret_escapes) {
            bool stop_output = false;
            for (size_t j = 0; j < str.size(); ++j) {
                if (str[j] == '\\' && j + 1 < str.size()) {
                    char next = str[j + 1];
                    if (next == 'n') { std::cout << '\n'; j++; }
                    else if (next == 't') { std::cout << '\t'; j++; }
                    else if (next == 'r') { std::cout << '\r'; j++; }
                    else if (next == 'a') { std::cout << '\a'; j++; }
                    else if (next == 'b') { std::cout << '\b'; j++; }
                    else if (next == 'f') { std::cout << '\f'; j++; }
                    else if (next == 'v') { std::cout << '\v'; j++; }
                    else if (next == 'e' || next == 'E') { std::cout << '\033'; j++; }
                    else if (next == '\\') { std::cout << '\\'; j++; }
                    else if (next == 'c') { stop_output = true; newline = false; break; }
                    else if (next == '0' || (next >= '1' && next <= '7')) {
                        size_t p = j + 1;
                        char out_c = 0;
                        if (parse_octal_escape(str, p, out_c)) {
                            std::cout << out_c;
                            j = p - 1;
                        } else {
                            std::cout << '\\' << next;
                            j++;
                        }
                    } else if (next == 'x' && j + 2 < str.size()) {
                        size_t p = j + 2;
                        char out_c = 0;
                        if (parse_hex_escape(str, p, out_c)) {
                            std::cout << out_c;
                            j = p - 1;
                        } else {
                            std::cout << "\\x";
                            j++;
                        }
                    } else {
                        std::cout << '\\' << next;
                        j++;
                    }
                } else {
                    std::cout << str[j];
                }
            }
            if (stop_output) {
                std::cout.flush();
                return 0;
            }
        } else {
            std::cout << str;
        }
    }

    if (newline) {
        std::cout << '\n';
    }
    std::cout.flush();
    return 0;
}

int Builtins::builtin_printf(const std::vector<std::string>& args, Environment& /*env*/) {
    if (args.size() < 2) {
        std::cerr << "aswell: printf: usage: printf format [arguments]\n";
        return 1;
    }

    const std::string& fmt = args[1];
    size_t arg_idx = 2;

    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '\\' && i + 1 < fmt.size()) {
            char next = fmt[i + 1];
            if (next == 'n') { std::cout << '\n'; i++; continue; }
            if (next == 't') { std::cout << '\t'; i++; continue; }
            if (next == 'r') { std::cout << '\r'; i++; continue; }
            if (next == 'a') { std::cout << '\a'; i++; continue; }
            if (next == 'b') { std::cout << '\b'; i++; continue; }
            if (next == 'f') { std::cout << '\f'; i++; continue; }
            if (next == 'v') { std::cout << '\v'; i++; continue; }
            if (next == 'e' || next == 'E') { std::cout << '\033'; i++; continue; }
            if (next == '\\') { std::cout << '\\'; i++; continue; }
            if (next == '0' || (next >= '1' && next <= '7')) {
                size_t p = i + 1;
                char out_c = 0;
                if (parse_octal_escape(fmt, p, out_c)) {
                    std::cout << out_c;
                    i = p - 1;
                    continue;
                }
            }
            if (next == 'x' && i + 2 < fmt.size()) {
                size_t p = i + 2;
                char out_c = 0;
                if (parse_hex_escape(fmt, p, out_c)) {
                    std::cout << out_c;
                    i = p - 1;
                    continue;
                }
            }
            std::cout << next;
            i++;
            continue;
        }

        if (fmt[i] == '%' && i + 1 < fmt.size()) {
            i++;
            if (fmt[i] == '%') {
                std::cout << '%';
                continue;
            }

            std::string arg = (arg_idx < args.size()) ? args[arg_idx++] : "";
            char spec = fmt[i];

            switch (spec) {
                case 'b':
                    std::cout << expand_escapes(arg);
                    break;
                case 's':
                    std::cout << arg;
                    break;
                case 'd':
                case 'i': {
                    long long val = 0;
                    try { if (!arg.empty()) val = std::stoll(arg); } catch (...) {}
                    std::cout << val;
                    break;
                }
                case 'u': {
                    unsigned long long val = 0;
                    try { if (!arg.empty()) val = std::stoull(arg); } catch (...) {}
                    std::cout << val;
                    break;
                }
                case 'x': {
                    long long val = 0;
                    try { if (!arg.empty()) val = std::stoll(arg); } catch (...) {}
                    std::cout << std::hex << val << std::dec;
                    break;
                }
                case 'X': {
                    long long val = 0;
                    try { if (!arg.empty()) val = std::stoll(arg); } catch (...) {}
                    std::cout << std::hex << std::uppercase << val << std::nouppercase << std::dec;
                    break;
                }
                case 'o': {
                    long long val = 0;
                    try { if (!arg.empty()) val = std::stoll(arg); } catch (...) {}
                    std::cout << std::oct << val << std::dec;
                    break;
                }
                case 'c':
                    if (!arg.empty()) std::cout << arg[0];
                    break;
                default:
                    std::cout << '%' << spec;
                    break;
            }
            continue;
        }

        std::cout << fmt[i];
    }
    std::cout.flush();
    return 0;
}

int Builtins::builtin_read(const std::vector<std::string>& args, Environment& env) {
    std::string prompt;
    bool raw = false;
    size_t idx = 1;

    while (idx < args.size()) {
        if (args[idx] == "-r") {
            raw = true;
            idx++;
        } else if (args[idx] == "-p" && idx + 1 < args.size()) {
            prompt = args[idx + 1];
            idx += 2;
        } else {
            break;
        }
    }

    if (!prompt.empty()) {
        std::cout << prompt;
        std::cout.flush();
    }

    std::string line;
    if (!std::getline(std::cin, line)) {
        return 1;
    }

    if (!raw && !line.empty() && line.back() == '\\') {
        // Can accumulate continuation lines
    }

    std::vector<std::string> var_names;
    for (size_t i = idx; i < args.size(); ++i) {
        var_names.push_back(args[i]);
    }
    if (var_names.empty()) {
        var_names.push_back("REPLY");
    }

    // Split line by IFS into var_names
    std::string ifs = env.get_var("IFS");
    if (ifs.empty()) ifs = " \t\n";

    size_t char_pos = 0;
    for (size_t v = 0; v < var_names.size(); ++v) {
        // Skip leading IFS whitespace
        while (char_pos < line.size() && ifs.find(line[char_pos]) != std::string::npos) {
            char_pos++;
        }
        if (char_pos >= line.size()) {
            env.set_var(var_names[v], "");
            continue;
        }

        if (v == var_names.size() - 1) {
            // Last variable gets the rest of the line, trimmed of trailing IFS
            std::string rest = line.substr(char_pos);
            while (!rest.empty() && ifs.find(rest.back()) != std::string::npos) {
                rest.pop_back();
            }
            env.set_var(var_names[v], rest);
            break;
        }

        size_t word_start = char_pos;
        while (char_pos < line.size() && ifs.find(line[char_pos]) == std::string::npos) {
            char_pos++;
        }
        std::string word = line.substr(word_start, char_pos - word_start);
        env.set_var(var_names[v], word);
    }

    return 0;
}

int Builtins::builtin_type(const std::vector<std::string>& args, Environment& env) {
    if (args.size() <= 1) return 0;
    int ret = 0;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];
        std::string alias_val;
        if (env.get_alias(name, alias_val)) {
            std::cout << name << " is an alias for " << alias_val << "\n";
        } else if (is_builtin(name)) {
            std::cout << name << " is a shell builtin\n";
        } else if (env.has_function(name)) {
            std::cout << name << " is a function\n";
        } else {
            std::string path = env.find_in_path(name);
            if (!path.empty()) {
                std::cout << name << " is " << path << "\n";
            } else {
                std::cerr << "aswell: type: " << name << ": not found\n";
                ret = 1;
            }
        }
    }
    return ret;
}

int Builtins::builtin_help(const std::vector<std::string>& /*args*/) {
    std::cout << "\033[1;36mAswell Shell\033[0m — Builtin Commands:\n"
              << "  cd [dir]           Change the current directory\n"
              << "  pwd                Print current working directory\n"
              << "  echo [args...]     Write arguments to standard output (supports -e, \\033, \\x1b)\n"
              << "  printf fmt [args]  Formatted output with ANSI color escapes & %b\n"
              << "  color [opt] <col>  Output colored text with TrueColor styling\n"
              << "  test [expr] / [ ]  Evaluate conditional expression\n"
              << "  exit [n]           Exit shell with status n\n"
              << "  set [-e -u -x -v]  Set or display shell options and variables\n"
              << "  export [name[=val]] Set export attribute for variables\n"
              << "  alias [name=val]   Define or display aliases\n"
              << "  source / . file    Execute commands from a file in current shell\n"
              << "  jobs / fg / bg     Job control commands\n"
              << "  aswell color       Print colored text or inspect palettes\n"
              << "  aswell theme       Switch or list visual themes\n"
              << "  aswell config      Open interactive configuration TUI\n";
    return 0;
}

int Builtins::builtin_color(const std::vector<std::string>& args, Environment& env) {
    (void)env;
    if (args.size() <= 1 || (args.size() == 2 && (args[1] == "--help" || args[1] == "-h" || args[1] == "help"))) {
        std::cout << "\033[1;36mUsage:\033[0m\n"
                  << "  color [OPTIONS] <COLOR> [TEXT...]\n"
                  << "  color rainbow <TEXT...>\n"
                  << "  color gradient <COLOR1> <COLOR2> <TEXT...>\n"
                  << "  color eval <MARKUP>\n"
                  << "  color list\n\n"
                  << "\033[1;33mOptions:\033[0m\n"
                  << "  -b, --bold         Bold text\n"
                  << "  -d, --dim          Dim / faint text\n"
                  << "  -i, --italic       Italic text\n"
                  << "  -u, --underline    Underline text\n"
                  << "  -r, --reverse      Invert foreground and background\n"
                  << "  --bg <COLOR>       Set background color\n"
                  << "  -n                 Do not append a trailing newline\n"
                  << "  -e                 Interpret backslash escapes in text\n\n"
                  << "\033[1;33mColors:\033[0m\n"
                  << "  Named:    red, green, blue, yellow, cyan, magenta, orange, pink, white, gray...\n"
                  << "  Nord:     nord0 .. nord15\n"
                  << "  Dracula:  dracula-red, dracula-green, dracula-cyan, dracula-purple...\n"
                  << "  Hex:      #00f0ff, #ff0055, #f00\n"
                  << "  RGB:      rgb(255, 80, 100)\n"
                  << "  256-code: 0 .. 255\n\n"
                  << "\033[1;33mExamples:\033[0m\n"
                  << "  color red \"Error: Connection refused\"\n"
                  << "  color green --bold \"Build successful\"\n"
                  << "  color --bg #222222 #00f0ff \"Cyberpunk neon message\"\n"
                  << "  color gradient cyan magenta \"Flowing gradient output\"\n"
                  << "  color rainbow \"Aswell Modern Shell\"\n"
                  << "  echo \"Pipeline data\" | color cyan\n"
                  << "  color list\n";
        return 0;
    }

    if (args.size() >= 2 && args[1] == "list") {
        print_color_list();
        return 0;
    }

    if (args.size() >= 2 && args[1] == "rainbow") {
        std::string text;
        if (args.size() > 2) {
            for (size_t i = 2; i < args.size(); ++i) {
                if (i > 2) text += " ";
                text += args[i];
            }
            print_rainbow(text, true);
        } else if (!isatty(STDIN_FILENO)) {
            std::string line;
            while (std::getline(std::cin, line)) {
                print_rainbow(line, true);
            }
        }
        return 0;
    }

    if (args.size() >= 4 && args[1] == "gradient") {
        Color c1 = Color::parse(args[2]);
        Color c2 = Color::parse(args[3]);
        if (c1.is_none) c1 = Color::from_name("cyan");
        if (c2.is_none) c2 = Color::from_name("magenta");

        std::string text;
        if (args.size() > 4) {
            for (size_t i = 4; i < args.size(); ++i) {
                if (i > 4) text += " ";
                text += args[i];
            }
            print_gradient(text, c1, c2, true);
        } else if (!isatty(STDIN_FILENO)) {
            std::string line;
            while (std::getline(std::cin, line)) {
                print_gradient(line, c1, c2, true);
            }
        }
        return 0;
    }

    if (args.size() >= 3 && args[1] == "eval") {
        std::string markup;
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2) markup += " ";
            markup += args[i];
        }
        TemplateContext ctx;
        std::cout << TemplateEngine::render(markup, ctx) << "\n";
        return 0;
    }

    // Parsing options & color
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
    bool newline = true;
    bool interpret_escapes = false;
    Color bg_color;
    std::string color_spec;
    std::vector<std::string> text_parts;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-b" || a == "--bold") bold = true;
        else if (a == "-d" || a == "--dim") dim = true;
        else if (a == "-i" || a == "--italic") italic = true;
        else if (a == "-u" || a == "--underline") underline = true;
        else if (a == "-r" || a == "--reverse") reverse = true;
        else if (a == "-n") newline = false;
        else if (a == "-e") interpret_escapes = true;
        else if (a == "--bg" && i + 1 < args.size()) {
            bg_color = Color::parse(args[++i]);
        } else if (color_spec.empty()) {
            color_spec = a;
        } else {
            text_parts.push_back(a);
        }
    }

    Color fg_color = Color::parse(color_spec);
    if (fg_color.is_none && !color_spec.empty()) {
        text_parts.insert(text_parts.begin(), color_spec);
        fg_color = Color(255, 255, 255);
    }

    std::string prefix;
    if (bold) prefix += "\033[1m";
    if (dim) prefix += "\033[2m";
    if (italic) prefix += "\033[3m";
    if (underline) prefix += "\033[4m";
    if (reverse) prefix += "\033[7m";
    if (!fg_color.is_none) prefix += fg_color.to_fg_ansi(true);
    if (!bg_color.is_none) prefix += bg_color.to_bg_ansi(true);

    if (!text_parts.empty()) {
        std::string text;
        for (size_t i = 0; i < text_parts.size(); ++i) {
            if (i > 0) text += " ";
            text += text_parts[i];
        }
        if (interpret_escapes) {
            text = expand_escapes(text);
        }
        std::cout << prefix << text << "\033[0m";
        if (newline) std::cout << '\n';
        std::cout.flush();
    } else if (!isatty(STDIN_FILENO)) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (interpret_escapes) line = expand_escapes(line);
            std::cout << prefix << line << "\033[0m\n";
        }
        std::cout.flush();
    } else {
        if (color_spec.empty()) {
            std::cerr << "aswell: color: missing color specification. Run 'color --help' for usage.\n";
            return 1;
        }
    }

    return 0;
}

} // namespace aswell
