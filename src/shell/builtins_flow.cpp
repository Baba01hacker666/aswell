#include "aswell/shell/builtins.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/signals.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace aswell {

int Builtins::builtin_exit(const std::vector<std::string>& args, Environment& env, ControlFlow& flow) {
    int code = env.last_exit_status;
    if (args.size() > 1) {
        try {
            code = std::stoi(args[1]);
        } catch (...) {
            code = 2;
        }
    }
    flow.type = ControlFlow::Type::EXIT;
    flow.status = code;
    return code;
}

int Builtins::builtin_break(const std::vector<std::string>& args, ControlFlow& flow) {
    int count = 1;
    if (args.size() > 1) {
        try { count = std::stoi(args[1]); } catch (...) { count = 1; }
    }
    flow.type = ControlFlow::Type::BREAK;
    flow.count = count;
    return 0;
}

int Builtins::builtin_continue(const std::vector<std::string>& args, ControlFlow& flow) {
    int count = 1;
    if (args.size() > 1) {
        try { count = std::stoi(args[1]); } catch (...) { count = 1; }
    }
    flow.type = ControlFlow::Type::CONTINUE;
    flow.count = count;
    return 0;
}

int Builtins::builtin_return(const std::vector<std::string>& args, Environment& env, ControlFlow& flow) {
    int code = env.last_exit_status;
    if (args.size() > 1) {
        try { code = std::stoi(args[1]); } catch (...) { code = 0; }
    }
    flow.type = ControlFlow::Type::RETURN;
    flow.status = code;
    return code;
}

int Builtins::builtin_eval(const std::vector<std::string>& args, Environment& /*env*/, Executor& executor) {
    std::string line;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) line += ' ';
        line += args[i];
    }
    return executor.execute_string(line);
}

int Builtins::builtin_exec(const std::vector<std::string>& args, Environment& env) {
    if (args.size() <= 1) return 0;

    std::string cmd = args[1];
    std::string full_path = env.find_in_path(cmd);
    if (full_path.empty()) {
        std::cerr << "aswell: exec: " << cmd << ": not found\n";
        return 127;
    }

    std::vector<char*> argv;
    for (size_t i = 1; i < args.size(); ++i) {
        argv.push_back(const_cast<char*>(args[i].c_str()));
    }
    argv.push_back(nullptr);

    execve(full_path.c_str(), argv.data(), env.get_envp().data());
    std::cerr << "aswell: exec: " << std::strerror(errno) << "\n";
    return 126;
}

int Builtins::builtin_source(const std::vector<std::string>& args, Environment& env, Executor& executor) {
    if (args.size() <= 1) {
        std::cerr << "aswell: " << args[0] << ": filename argument required\n";
        return 2;
    }

    std::string filename = args[1];
    std::ifstream file(filename);
    if (!file) {
        std::string found = env.find_in_path(filename);
        if (!found.empty()) {
            file.open(found);
            filename = found;
        }
    }

    if (!file) {
        std::cerr << "aswell: " << args[0] << ": " << filename << ": No such file or directory\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (args.size() > 2) {
        std::vector<std::string> sub_params(args.begin() + 2, args.end());
        env.push_positional_params(sub_params);
    }

    int ret = executor.execute_script(buffer.str());

    if (args.size() > 2) {
        env.pop_positional_params();
    }

    return ret;
}

int Builtins::builtin_trap(const std::vector<std::string>& args, Environment& env) {
    if (args.size() <= 1) {
        for (const auto& [sig, action] : env.get_traps()) {
            std::cout << "trap -- " << str_util::escape_shell(action) << " " << SignalManager::signum_to_name(sig) << "\n";
        }
        return 0;
    }

    if (args[1] == "-l") {
        for (int i = 1; i < 32; ++i) {
            std::cout << i << ") SIG" << SignalManager::signum_to_name(i) << "\t";
            if (i % 4 == 0) std::cout << "\n";
        }
        std::cout << "\n";
        return 0;
    }

    std::string action = args[1];
    for (size_t i = 2; i < args.size(); ++i) {
        int sig = SignalManager::signame_to_num(args[i]);
        if (sig >= 0) {
            if (action == "-") {
                env.remove_trap(sig);
            } else {
                env.set_trap(sig, action);
            }
        }
    }
    return 0;
}

int Builtins::builtin_hash(const std::vector<std::string>& args, Environment& env) {
    bool opt_t = false;   // print paths only
    bool opt_d = false;   // forget entries
    std::string forced_path;
    std::vector<std::string> names;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--") {
            for (++i; i < args.size(); ++i) names.push_back(args[i]);
            break;
        } else if (a == "-r") {
            env.clear_command_hash();
            return 0;
        } else if (a == "-d") {
            opt_d = true;
        } else if (a == "-t") {
            opt_t = true;
        } else if (a == "-p" && i + 1 < args.size()) {
            forced_path = args[++i];
        } else if (a.size() > 1 && a[0] == '-') {
            std::cerr << "aswell: hash: -" << a[1] << ": invalid option\n";
            return 1;
        } else {
            names.push_back(a);
        }
    }

    // `hash -p /custom/path name`: seed the table directly.
    if (!forced_path.empty()) {
        if (names.empty()) {
            std::cerr << "aswell: hash: -p: option requires a name argument\n";
            return 1;
        }
        if (access(forced_path.c_str(), X_OK) != 0) {
            std::cerr << "aswell: hash: " << forced_path << ": " << std::strerror(errno) << "\n";
            return 1;
        }
        for (const auto& n : names) env.remember_command(n, forced_path);
        return 0;
    }

    if (opt_d) {
        int ret = 0;
        auto table = env.get_command_hash();
        for (const auto& n : names) {
            if (table.find(n) == table.end()) {
                std::cerr << "aswell: hash: " << n << ": not found\n";
                ret = 1;
            } else {
                env.forget_command(n);
            }
        }
        if (names.empty()) {
            std::cerr << "aswell: hash: -d: option requires an argument\n";
            return 1;
        }
        return ret;
    }

    if (names.empty()) {
        auto table = env.get_command_hash();
        if (table.empty()) {
            std::cout << "hash: hash table empty\n";
            return 0;
        }
        if (!opt_t) std::cout << "hits\tcommand\n";
        for (const auto& [name, entry] : table) {
            if (opt_t) {
                std::cout << entry.first << "\n";
            } else {
                std::cout << std::setw(4) << entry.second << "\t" << entry.first << "\n";
            }
        }
        return 0;
    }

    int ret = 0;
    for (const auto& n : names) {
        if (opt_t) {
            std::string path;
            if (env.get_remembered_command(n, path)) {
                std::cout << path << "\n";
            } else {
                std::cerr << "aswell: hash: " << n << ": not found\n";
                ret = 1;
            }
            continue;
        }
        // Resolving populates the shared table via find_in_path().
        std::string path = env.find_in_path(n);
        if (path.empty()) {
            std::cerr << "aswell: hash: " << n << ": not found\n";
            ret = 1;
        }
    }
    return ret;
}

namespace {
mode_t current_umask_value() {
    mode_t old_mask = umask(0);
    umask(old_mask);
    return old_mask;
}

std::string format_symbolic_umask(mode_t mask) {
    std::string out;
    const char* classes = "ugo";
    for (int c = 0; c < 3; ++c) {
        if (c > 0) out += ',';
        out += classes[static_cast<size_t>(c)];
        out += '=';
        int bits = (static_cast<int>(mask) >> ((2 - c) * 3)) & 7;
        int perms = (~bits) & 7;
        if (perms & 4) out += 'r';
        if (perms & 2) out += 'w';
        if (perms & 1) out += 'x';
    }
    return out;
}

// Parse symbolic modes like "u=rwx,g=rx,o=", "a+rx", "go-w".
// Returns false on any invalid clause.
bool parse_symbolic_umask(const std::string& spec, mode_t current, mode_t& out_mask) {
    int m = static_cast<int>(current);
    for (const std::string& clause : str_util::split(spec, ',')) {
        if (clause.empty()) return false;
        size_t i = 0;
        int who = 0; // bit 0=u, 1=g, 2=o
        while (i < clause.size()) {
            char c = clause[i];
            if (c == 'u') who |= 1;
            else if (c == 'g') who |= 2;
            else if (c == 'o') who |= 4;
            else if (c == 'a') who |= 7;
            else break;
            ++i;
        }
        if (who == 0) who = 7; // empty who-list means "all"
        if (i >= clause.size()) return false;
        char op = clause[i++];
        if (op != '+' && op != '-' && op != '=') return false;
        int perms = 0;
        while (i < clause.size()) {
            char c = clause[i++];
            if (c == 'r') perms |= 4;
            else if (c == 'w') perms |= 2;
            else if (c == 'x') perms |= 1;
            else return false;
        }
        for (int c = 0; c < 3; ++c) {
            if (!(who & (1 << c))) continue;
            int shift = (2 - c) * 3;
            int bits = (perms << shift) & 0777;
            int cmask = 7 << shift;
            if (op == '=') m = (m & ~cmask) | ((~bits) & cmask);
            else if (op == '+') m &= ~bits; // adding permission clears mask bits
            else m |= bits;                 // removing permission sets mask bits
        }
    }
    out_mask = static_cast<mode_t>(m & 0777);
    return true;
}

bool is_octal_umask(const std::string& s) {
    if (s.empty() || s.size() > 4) return false;
    for (char c : s) {
        if (c < '0' || c > '7') return false;
    }
    return true;
}
} // namespace

int Builtins::builtin_umask(const std::vector<std::string>& args, Environment& /*env*/) {
    bool symbolic = false;
    size_t idx = 1;
    if (idx < args.size() && args[idx] == "-S") {
        symbolic = true;
        ++idx;
    }

    if (idx >= args.size()) {
        mode_t mask = current_umask_value();
        if (symbolic) {
            std::cout << format_symbolic_umask(mask) << "\n";
        } else {
            std::cout << std::oct << std::setfill('0') << std::setw(4) << mask << std::dec << "\n";
        }
        return 0;
    }

    const std::string& spec = args[idx];
    mode_t new_mask = 0;
    if (is_octal_umask(spec)) {
        try {
            new_mask = static_cast<mode_t>(std::stoul(spec, nullptr, 8));
        } catch (...) {
            std::cerr << "aswell: umask: invalid octal number\n";
            return 1;
        }
    } else {
        if (!parse_symbolic_umask(spec, current_umask_value(), new_mask)) {
            std::cerr << "aswell: umask: '" << spec << "': invalid symbolic mode\n";
            return 1;
        }
    }
    umask(new_mask);
    if (symbolic) {
        std::cout << format_symbolic_umask(new_mask) << "\n";
    }
    return 0;
}

int Builtins::builtin_history(const std::vector<std::string>& args) {
    std::string hist_file = std::string(getenv("HOME") ? getenv("HOME") : "") + "/.aswell_history";

    if (args.size() > 1 && args[1] == "-c") {
        std::ofstream file(hist_file, std::ios::trunc);
        return 0;
    }

    if (args.size() > 1 && args[1] == "-d") {
        if (args.size() < 3) {
            std::cerr << "aswell: history: -d: option requires an argument\n";
            return 1;
        }
        int del_idx = 0;
        try {
            del_idx = std::stoi(args[2]);
        } catch (...) {
            std::cerr << "aswell: history: " << args[2] << ": history position out of range\n";
            return 1;
        }
        std::ifstream file(hist_file);
        if (!file) return 0;
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }
        if (del_idx <= 0 || static_cast<size_t>(del_idx) > lines.size()) {
            std::cerr << "aswell: history: " << del_idx << ": history position out of range\n";
            return 1;
        }
        lines.erase(lines.begin() + (del_idx - 1));
        std::ofstream out(hist_file, std::ios::trunc);
        for (const auto& l : lines) {
            out << l << "\n";
        }
        return 0;
    }

    std::ifstream file(hist_file);
    if (!file) return 0;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    size_t start = 0;
    if (args.size() > 1) {
        try {
            int n = std::stoi(args[1]);
            if (n > 0 && static_cast<size_t>(n) < lines.size()) {
                start = lines.size() - static_cast<size_t>(n);
            }
        } catch (...) {
            // Not a number, ignore
        }
    }

    for (size_t i = start; i < lines.size(); ++i) {
        std::cout << std::setw(5) << (i + 1) << "  " << lines[i] << "\n";
    }
    return 0;
}

} // namespace aswell
