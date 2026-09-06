#include "aswell/shell/builtins.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/signals.hpp"
#include <fstream>
#include <iomanip>

namespace aswell {

bool Builtins::is_builtin(const std::string& name) {
    static const std::set<std::string> builtins = {
        "cd", "pwd", "echo", "printf", "test", "[", "exit",
        "set", "unset", "export", "readonly", "alias", "unalias",
        "eval", "exec", "read", "source", ".", "shift", "trap",
        "type", "wait", "jobs", "fg", "bg", "kill", "hash",
        "umask", "local", "break", "continue", "return",
        "true", "false", ":", "help", "history", "aswell"
    };
    return builtins.find(name) != builtins.end();
}

int Builtins::execute(const std::string& name,
                      const std::vector<std::string>& args,
                      Environment& env,
                      JobManager& jobs,
                      Executor& executor,
                      ControlFlow& flow) {
    if (name == "cd") return builtin_cd(args, env);
    if (name == "pwd") return builtin_pwd(args, env);
    if (name == "echo") return builtin_echo(args, env);
    if (name == "printf") return builtin_printf(args, env);
    if (name == "test" || name == "[") return builtin_test(args, env);
    if (name == "exit") return builtin_exit(args, env, flow);
    if (name == "set") return builtin_set(args, env);
    if (name == "unset") return builtin_unset(args, env);
    if (name == "export") return builtin_export(args, env);
    if (name == "readonly") return builtin_readonly(args, env);
    if (name == "alias") return builtin_alias(args, env);
    if (name == "unalias") return builtin_unalias(args, env);
    if (name == "eval") return builtin_eval(args, env, executor);
    if (name == "exec") return builtin_exec(args, env);
    if (name == "read") return builtin_read(args, env);
    if (name == "source" || name == ".") return builtin_source(args, env, executor);
    if (name == "shift") return builtin_shift(args, env);
    if (name == "trap") return builtin_trap(args, env);
    if (name == "type") return builtin_type(args, env);
    if (name == "wait") return builtin_wait(args, env, jobs);
    if (name == "jobs") return builtin_jobs(args, jobs);
    if (name == "fg") return builtin_fg(args, jobs);
    if (name == "bg") return builtin_bg(args, jobs);
    if (name == "kill") return builtin_kill(args, env);
    if (name == "hash") return builtin_hash(args, env);
    if (name == "umask") return builtin_umask(args, env);
    if (name == "local") return builtin_local(args, env);
    if (name == "break") return builtin_break(args, flow);
    if (name == "continue") return builtin_continue(args, flow);
    if (name == "return") return builtin_return(args, env, flow);
    if (name == "true" || name == ":") return 0;
    if (name == "false") return 1;
    if (name == "help") return builtin_help(args);
    if (name == "history") return builtin_history(args);
    if (name == "aswell") return builtin_aswell(args, env, executor);

    return 1;
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

int Builtins::builtin_test(const std::vector<std::string>& raw_args, Environment& /*env*/) {
    std::vector<std::string> args = raw_args;
    if (!args.empty() && args[0] == "[") {
        if (args.back() == "]") {
            args.pop_back();
        } else {
            std::cerr << "aswell: [: missing ']'\n";
            return 2;
        }
    }

    if (args.size() <= 1) return 1; // Empty test is false

    bool invert = false;
    size_t idx = 1;
    if (args[idx] == "!") {
        invert = true;
        idx++;
    }

    size_t remaining = args.size() - idx;
    bool result = false;

    if (remaining == 1) {
        result = !args[idx].empty();
    } else if (remaining == 2) {
        const std::string& op = args[idx];
        const std::string& target = args[idx + 1];
        struct stat st;

        if (op == "-f") result = (stat(target.c_str(), &st) == 0 && S_ISREG(st.st_mode));
        else if (op == "-d") result = (stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        else if (op == "-e") result = (stat(target.c_str(), &st) == 0);
        else if (op == "-r") result = (access(target.c_str(), R_OK) == 0);
        else if (op == "-w") result = (access(target.c_str(), W_OK) == 0);
        else if (op == "-x") result = (access(target.c_str(), X_OK) == 0);
        else if (op == "-s") result = (stat(target.c_str(), &st) == 0 && st.st_size > 0);
        else if (op == "-z") result = target.empty();
        else if (op == "-n") result = !target.empty();
        else if (op == "-L" || op == "-h") result = (lstat(target.c_str(), &st) == 0 && S_ISLNK(st.st_mode));
        else if (op == "-t") {
            int fd = 0;
            try { fd = std::stoi(target); } catch (...) { fd = -1; }
            result = (fd >= 0 && isatty(fd));
        }
    } else if (remaining == 3) {
        const std::string& left = args[idx];
        const std::string& op = args[idx + 1];
        const std::string& right = args[idx + 2];

        if (op == "=" || op == "==") result = (left == right);
        else if (op == "!=") result = (left != right);
        else {
            long long l = 0, r = 0;
            try { l = std::stoll(left); } catch (...) {}
            try { r = std::stoll(right); } catch (...) {}

            if (op == "-eq") result = (l == r);
            else if (op == "-ne") result = (l != r);
            else if (op == "-lt") result = (l < r);
            else if (op == "-le") result = (l <= r);
            else if (op == "-gt") result = (l > r);
            else if (op == "-ge") result = (l >= r);
        }
    }

    if (invert) result = !result;
    return result ? 0 : 1;
}

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

int Builtins::builtin_set(const std::vector<std::string>& args, Environment& env) {
    if (args.size() == 1) {
        auto vars = env.get_all_vars();
        for (const auto& [k, v] : vars) {
            std::cout << k << "=" << str_util::escape_shell(v.value) << "\n";
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-e") env.opt_errexit = true;
        else if (a == "+e") env.opt_errexit = false;
        else if (a == "-u") env.opt_nounset = true;
        else if (a == "+u") env.opt_nounset = false;
        else if (a == "-x") env.opt_xtrace = true;
        else if (a == "+x") env.opt_xtrace = false;
        else if (a == "-v") env.opt_verbose = true;
        else if (a == "+v") env.opt_verbose = false;
        else if (a == "-o" && i + 1 < args.size()) {
            i++;
            if (args[i] == "pipefail") env.opt_pipefail = true;
            else if (args[i] == "vi") env.opt_vi_mode = true;
            else if (args[i] == "emacs") env.opt_vi_mode = false;
        } else if (a == "--") {
            std::vector<std::string> params(args.begin() + i + 1, args.end());
            env.set_positional_params(params);
            break;
        }
    }
    return 0;
}

int Builtins::builtin_unset(const std::vector<std::string>& args, Environment& env) {
    bool unset_func = false;
    size_t idx = 1;
    if (idx < args.size() && args[idx] == "-f") {
        unset_func = true;
        idx++;
    } else if (idx < args.size() && args[idx] == "-v") {
        idx++;
    }

    for (size_t i = idx; i < args.size(); ++i) {
        if (unset_func) {
            env.remove_function(args[i]);
        } else {
            env.unset_var(args[i]);
        }
    }
    return 0;
}

int Builtins::builtin_export(const std::vector<std::string>& args, Environment& env) {
    if (args.size() == 1 || (args.size() == 2 && args[1] == "-p")) {
        auto vars = env.get_all_vars();
        for (const auto& [k, v] : vars) {
            if (v.is_exported) {
                std::cout << "export " << k << "=" << str_util::escape_shell(v.value) << "\n";
            }
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        size_t eq = a.find('=');
        if (eq != std::string::npos) {
            env.set_var(a.substr(0, eq), a.substr(eq + 1), true);
        } else {
            env.export_var(a);
        }
    }
    return 0;
}

int Builtins::builtin_readonly(const std::vector<std::string>& args, Environment& env) {
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        size_t eq = a.find('=');
        if (eq != std::string::npos) {
            env.set_var(a.substr(0, eq), a.substr(eq + 1), false);
            env.set_readonly(a.substr(0, eq));
        } else {
            env.set_readonly(a);
        }
    }
    return 0;
}

int Builtins::builtin_alias(const std::vector<std::string>& args, Environment& env) {
    if (args.size() == 1) {
        for (const auto& [k, v] : env.get_aliases()) {
            std::cout << "alias " << k << "=" << str_util::escape_shell(v) << "\n";
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        size_t eq = a.find('=');
        if (eq != std::string::npos) {
            env.set_alias(a.substr(0, eq), a.substr(eq + 1));
        } else {
            std::string val;
            if (env.get_alias(a, val)) {
                std::cout << "alias " << a << "=" << str_util::escape_shell(val) << "\n";
            } else {
                std::cerr << "aswell: alias: " << a << ": not found\n";
                return 1;
            }
        }
    }
    return 0;
}

int Builtins::builtin_unalias(const std::vector<std::string>& args, Environment& env) {
    if (args.size() > 1 && args[1] == "-a") {
        env.clear_aliases();
        return 0;
    }
    for (size_t i = 1; i < args.size(); ++i) {
        env.remove_alias(args[i]);
    }
    return 0;
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

int Builtins::builtin_shift(const std::vector<std::string>& args, Environment& env) {
    size_t n = 1;
    if (args.size() > 1) {
        try { n = std::stoul(args[1]); } catch (...) { return 1; }
    }
    if (!env.shift_positional_params(n)) {
        std::cerr << "aswell: shift: " << n << ": shift count out of range\n";
        return 1;
    }
    return 0;
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

int Builtins::builtin_wait(const std::vector<std::string>& args, Environment& /*env*/, JobManager& jobs) {
    if (args.size() <= 1) {
        for (const auto& [id, job] : jobs.get_jobs()) {
            jobs.wait_for_job(id);
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        int id = 0;
        try { id = std::stoi(args[i]); } catch (...) { continue; }
        jobs.wait_for_job(id);
    }
    return 0;
}

int Builtins::builtin_jobs(const std::vector<std::string>& args, JobManager& jobs) {
    bool verbose = (args.size() > 1 && args[1] == "-l");
    jobs.list_jobs(verbose);
    return 0;
}

int Builtins::builtin_fg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 1;
    if (args.size() > 1) {
        std::string s = args[1];
        if (!s.empty() && s[0] == '%') s = s.substr(1);
        try { id = std::stoi(s); } catch (...) { return 1; }
    }
    jobs.put_job_in_foreground(id, true);
    return 0;
}

int Builtins::builtin_bg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 1;
    if (args.size() > 1) {
        std::string s = args[1];
        if (!s.empty() && s[0] == '%') s = s.substr(1);
        try { id = std::stoi(s); } catch (...) { return 1; }
    }
    jobs.put_job_in_background(id, true);
    return 0;
}

int Builtins::builtin_kill(const std::vector<std::string>& args, Environment& /*env*/) {
    if (args.size() <= 1) {
        std::cerr << "aswell: kill: usage: kill [-sig] pid...\n";
        return 1;
    }

    int sig = SIGTERM;
    size_t idx = 1;
    if (args[idx].size() > 1 && args[idx][0] == '-') {
        std::string signame = args[idx].substr(1);
        int parsed = SignalManager::signame_to_num(signame);
        if (parsed >= 0) {
            sig = parsed;
            idx++;
        }
    }

    for (size_t i = idx; i < args.size(); ++i) {
        pid_t pid = 0;
        try { pid = std::stoi(args[i]); } catch (...) { continue; }
        kill(pid, sig);
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

int Builtins::builtin_local(const std::vector<std::string>& args, Environment& env) {
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        size_t eq = a.find('=');
        if (eq != std::string::npos) {
            env.set_local_var(a.substr(0, eq), a.substr(eq + 1));
        } else {
            env.set_local_var(a, "");
        }
    }
    return 0;
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

int Builtins::builtin_history(const std::vector<std::string>& /*args*/) {
    std::string hist_file = std::string(getenv("HOME") ? getenv("HOME") : "") + "/.aswell_history";
    std::ifstream file(hist_file);
    if (!file) return 0;

    std::string line;
    int idx = 1;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            std::cout << std::setw(5) << idx++ << "  " << line << "\n";
        }
    }
    return 0;
}

int Builtins::builtin_aswell(const std::vector<std::string>& args, Environment& env, Executor& /*executor*/) {
    if (args.size() <= 1) {
        std::cout << SHELL_BANNER << "\n"
                  << "Type 'aswell help' for commands or 'aswell theme' to configure themes.\n";
        return 0;
    }

    const std::string& sub = args[1];
    if (sub == "version" || sub == "--version") {
        std::cout << "aswell version " << SHELL_VERSION << " (POSIX Compatible Shell)\n";
        return 0;
    }
    if (sub == "help" || sub == "--help") {
        builtin_help(args);
        return 0;
    }
    if (sub == "theme") {
        if (args.size() == 2 || (args.size() == 3 && args[2] == "list")) {
            std::cout << "\033[1;34mAvailable Aswell Themes:\033[0m\n"
                      << "  * \033[1;32mmodern\033[0m     - Clean, elegant unicode boxes, branch glyphs, subtle colors\n"
                      << "  * \033[1;35mcyberpunk\033[0m  - Neon cyan/magenta glowing accents, sharp brackets\n"
                      << "  * \033[1;36mnord\033[0m       - Arctic blue, frost, muted Scandinavian aesthetic\n"
                      << "  * \033[1;37mminimal\033[0m    - Monochrome single-character prompt, blazing speed\n"
                      << "  * \033[1;31mdracula\033[0m    - Vampire dark theme with purple and emerald highlights\n"
                      << "  * \033[1;33mpowerline\033[0m  - Segmented status arrows with contrasting backgrounds\n"
                      << "Use 'aswell theme set <name>' to apply.\n";
            return 0;
        }
        if (args.size() >= 4 && args[2] == "set") {
            env.set_var("ASWELL_THEME", args[3], true);
            std::cout << "Aswell theme changed to: " << args[3] << "\n";
            return 0;
        }
    }

    std::cout << "Unknown aswell command: " << sub << ". Run 'aswell help' for options.\n";
    return 1;
}

} // namespace aswell
