#include "aswell/shell/builtins.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cctype>

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

namespace {

bool is_valid_varname(const std::string& name) {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') return false;
    for (size_t i = 1; i < name.size(); ++i) {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

long read_optind(Environment& env) {
    const std::string& s = env.get_var("OPTIND");
    if (s.empty()) return 1;
    try {
        size_t pos = 0;
        long v = std::stol(s, &pos);
        if (pos != s.size() || v < 1) return 1;
        return v;
    } catch (...) {
        return 1;
    }
}

} // namespace

int Builtins::builtin_getopts(const std::vector<std::string>& args, Environment& env) {
    if (args.size() < 3) {
        std::cerr << "aswell: getopts: usage: getopts optstring name [arg ...]\n";
        return 1;
    }
    const std::string& optstring = args[1];
    const std::string& varname = args[2];
    if (!is_valid_varname(varname)) {
        std::cerr << "aswell: getopts: '" << varname << "': not a valid identifier\n";
        return 1;
    }

    std::vector<std::string> operands;
    if (args.size() == 3) {
        operands = env.get_positional_params();
    } else {
        for (size_t i = 3; i < args.size(); ++i) operands.push_back(args[i]);
    }

    const bool silent = !optstring.empty() && optstring[0] == ':';
    const size_t opt_start = silent ? 1 : 0;
    const bool noisy = !silent && env.get_var("OPTERR") != "0";
    const size_t count = operands.size();

    long optind = read_optind(env);
    size_t nextchar = env.get_getopts_nextchar();
    // A user reassignment of OPTIND (or first use) resets cluster tracking.
    if (optind != env.get_getopts_last_ind()) {
        nextchar = 0;
    }

    auto finish = [&](const std::string& name_val, bool set_optarg, const std::string& optarg_val,
                      long new_ind, size_t new_nextchar, int ret) {
        env.set_var(varname, name_val);
        if (set_optarg) {
            env.set_var("OPTARG", optarg_val);
        } else {
            env.unset_var("OPTARG");
        }
        env.set_var("OPTIND", std::to_string(new_ind));
        env.set_getopts_nextchar(new_nextchar);
        env.set_getopts_last_ind(new_ind);
        return ret;
    };
    // End of options: point OPTIND at the stopper, name to '?'.
    auto at_end = [&](long new_ind) {
        return finish("?", false, "", new_ind, 0, 1);
    };

    if (static_cast<size_t>(optind) > count || optind < 1) {
        return at_end(optind < 1 ? 1 : optind);
    }

    std::string word = operands[static_cast<size_t>(optind) - 1];
    if (nextchar == 0) {
        // Fetch a new word: it must look like an option.
        if (word.size() < 2 || word[0] != '-' || word == "-") {
            return at_end(optind);
        }
        if (word == "--") {
            return at_end(optind + 1);
        }
        nextchar = 1;
        word = operands[static_cast<size_t>(optind) - 1];
    } else if (nextchar >= word.size()) {
        // Stale cluster position (should not happen): resync.
        nextchar = 0;
        return at_end(optind);
    }

    char opt = word[nextchar];
    size_t decl = optstring.find(opt, opt_start);
    bool takes_arg = (decl != std::string::npos && decl + 1 < optstring.size() &&
                      optstring[decl + 1] == ':');

    if (decl == std::string::npos) {
        // Unknown option: consume the whole word.
        if (noisy) {
            std::cerr << "aswell: getopts: illegal option -- " << opt << "\n";
        }
        if (silent) {
            return finish("?", true, std::string(1, opt), optind + 1, 0, 0);
        }
        return finish("?", false, "", optind + 1, 0, 0);
    }

    if (!takes_arg) {
        // Plain flag: advance within the cluster or to the next word.
        if (nextchar + 1 < word.size()) {
            return finish(std::string(1, opt), false, "", optind, nextchar + 1, 0);
        }
        return finish(std::string(1, opt), false, "", optind + 1, 0, 0);
    }

    // Option takes an argument: rest of the word, else the next word.
    if (nextchar + 1 < word.size()) {
        return finish(std::string(1, opt), true, word.substr(nextchar + 1), optind + 1, 0, 0);
    }
    if (static_cast<size_t>(optind) < count) {
        std::string optarg = operands[static_cast<size_t>(optind)];
        return finish(std::string(1, opt), true, optarg, optind + 2, 0, 0);
    }
    if (noisy) {
        std::cerr << "aswell: getopts: option requires an argument -- " << opt << "\n";
    }
    if (silent) {
        return finish(":", true, std::string(1, opt), optind + 1, 0, 0);
    }
    return finish("?", false, "", optind + 1, 0, 0);
}

} // namespace aswell
