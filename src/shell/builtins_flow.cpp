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

int Builtins::builtin_hash(const std::vector<std::string>& /*args*/, Environment& /*env*/) {
    return 0;
}

int Builtins::builtin_umask(const std::vector<std::string>& args, Environment& /*env*/) {
    if (args.size() <= 1) {
        mode_t old_mask = umask(0);
        umask(old_mask);
        std::cout << std::oct << std::setfill('0') << std::setw(4) << old_mask << std::dec << "\n";
        return 0;
    }

    try {
        mode_t mask = static_cast<mode_t>(std::stoul(args[1], nullptr, 8));
        umask(mask);
    } catch (...) {
        std::cerr << "aswell: umask: invalid octal number\n";
        return 1;
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
