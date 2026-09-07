#include "aswell/shell/builtins.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>

namespace aswell {

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
            for (size_t j = 0; j < str.size(); ++j) {
                if (str[j] == '\\' && j + 1 < str.size()) {
                    j++;
                    switch (str[j]) {
                        case 'n': std::cout << '\n'; break;
                        case 't': std::cout << '\t'; break;
                        case 'r': std::cout << '\r'; break;
                        case 'a': std::cout << '\a'; break;
                        case 'b': std::cout << '\b'; break;
                        case 'e': std::cout << '\033'; break;
                        case '\\': std::cout << '\\'; break;
                        default: std::cout << '\\' << str[j]; break;
                    }
                } else {
                    std::cout << str[j];
                }
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
            i++;
            switch (fmt[i]) {
                case 'n': std::cout << '\n'; break;
                case 't': std::cout << '\t'; break;
                case 'r': std::cout << '\r'; break;
                case '\\': std::cout << '\\'; break;
                default: std::cout << fmt[i]; break;
            }
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
                case 'x': {
                    long long val = 0;
                    try { if (!arg.empty()) val = std::stoll(arg); } catch (...) {}
                    std::cout << std::hex << val << std::dec;
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
              << "  echo [args...]     Write arguments to standard output\n"
              << "  printf fmt [args]  Formatted output\n"
              << "  test [expr] / [ ]  Evaluate conditional expression\n"
              << "  exit [n]           Exit shell with status n\n"
              << "  set [-e -u -x -v]  Set or display shell options and variables\n"
              << "  export [name[=val]] Set export attribute for variables\n"
              << "  alias [name=val]   Define or display aliases\n"
              << "  source / . file    Execute commands from a file in current shell\n"
              << "  jobs / fg / bg     Job control commands\n"
              << "  aswell theme       Switch or list visual themes\n"
              << "  aswell config      Open interactive configuration TUI\n";
    return 0;
}

} // namespace aswell
