#include "aswell/shell/builtins.hpp"
#include <iostream>
#include <vector>
#include <string>

namespace aswell {

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
            std::vector<std::string> params(args.begin() + static_cast<std::ptrdiff_t>(i + 1), args.end());
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

} // namespace aswell
