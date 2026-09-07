#include "aswell/shell/environment.hpp"

namespace aswell {

Environment::Environment() {
    shell_pid = getpid();

    // Import from process environ
    if (environ) {
        for (char** env = environ; *env != nullptr; ++env) {
            std::string entry = *env;
            size_t eq = entry.find('=');
            if (eq != std::string::npos) {
                std::string name = entry.substr(0, eq);
                std::string val = entry.substr(eq + 1);
                global_vars_[name] = Variable{val, true, false};
            }
        }
    }

    // Default POSIX environment variables if missing
    char cwd_buf[4096];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        set_var("PWD", cwd_buf, true);
    }
    if (!has_var("SHLVL")) {
        set_var("SHLVL", "1", true);
    } else {
        try {
            int lvl = std::stoi(get_var("SHLVL"));
            set_var("SHLVL", std::to_string(lvl + 1), true);
        } catch (...) {
            set_var("SHLVL", "1", true);
        }
    }

    if (!has_var("IFS")) {
        set_var("IFS", " \t\n", false);
    }
    
    const char* home_env = std::getenv("HOME");
    std::string home_str = home_env ? home_env : "/root";
    std::string custom_cmd_dir = home_str + "/.config/aswell/commands";
    std::string custom_bin_dir = home_str + "/.config/aswell/bin";

    std::string cur_path = has_var("PATH") ? get_var("PATH") : "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    if (cur_path.find(custom_cmd_dir) == std::string::npos) {
        cur_path = custom_cmd_dir + ":" + custom_bin_dir + ":" + cur_path;
    }
    set_var("PATH", cur_path, true);

    set_var("SHELL", "aswell", true);

    if (!has_var("COLORTERM")) {
        set_var("COLORTERM", "truecolor", true);
    }
    if (!has_var("CLICOLOR")) {
        set_var("CLICOLOR", "1", true);
    }
}

void Environment::set_var(const std::string& name, const std::string& value, bool export_var) {
    // Check if in local scopes
    for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
        auto var_it = it->find(name);
        if (var_it != it->end()) {
            if (var_it->second.is_readonly) {
                std::cerr << "aswell: " << name << ": readonly variable\n";
                return;
            }
            var_it->second.value = value;
            if (export_var) var_it->second.is_exported = true;
            return;
        }
    }

    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) {
        if (it->second.is_readonly) {
            std::cerr << "aswell: " << name << ": readonly variable\n";
            return;
        }
        it->second.value = value;
        if (export_var) it->second.is_exported = true;
    } else {
        global_vars_[name] = Variable{value, export_var, false};
    }
}

void Environment::set_local_var(const std::string& name, const std::string& value) {
    if (!local_scopes_.empty()) {
        local_scopes_.back()[name] = Variable{value, false, false};
    } else {
        set_var(name, value, false);
    }
}

std::string Environment::get_var(const std::string& name) const {
    if (name == "?") {
        return std::to_string(last_exit_status);
    }
    if (name == "$") {
        return std::to_string(shell_pid);
    }
    if (name == "!") {
        return last_bg_pid > 0 ? std::to_string(last_bg_pid) : "";
    }
    if (name == "#") {
        return std::to_string(current_positional_.size());
    }
    if (name == "-") {
        return get_options_flags();
    }
    if (name == "0") {
        return shell_name;
    }
    if (name == "*") {
        std::string res;
        for (size_t i = 0; i < current_positional_.size(); ++i) {
            if (i > 0) res += " ";
            res += current_positional_[i];
        }
        return res;
    }
    if (name == "@") {
        std::string res;
        for (size_t i = 0; i < current_positional_.size(); ++i) {
            if (i > 0) res += " ";
            res += current_positional_[i];
        }
        return res;
    }
    // Positional digits 1..9
    if (name.size() == 1 && std::isdigit(static_cast<unsigned char>(name[0]))) {
        size_t idx = static_cast<size_t>(name[0] - '0');
        if (idx > 0 && idx <= current_positional_.size()) {
            return current_positional_[idx - 1];
        }
        return "";
    }

    // Check local scopes from innermost out
    for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
        auto var_it = it->find(name);
        if (var_it != it->end()) {
            return var_it->second.value;
        }
    }

    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) {
        return it->second.value;
    }
    return "";
}

bool Environment::has_var(const std::string& name) const {
    for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
        if (it->find(name) != it->end()) return true;
    }
    return global_vars_.find(name) != global_vars_.end();
}

bool Environment::is_exported(const std::string& name) const {
    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) return it->second.is_exported;
    return false;
}

bool Environment::is_readonly(const std::string& name) const {
    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) return it->second.is_readonly;
    return false;
}

void Environment::unset_var(const std::string& name) {
    for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
        auto var_it = it->find(name);
        if (var_it != it->end()) {
            if (var_it->second.is_readonly) {
                std::cerr << "aswell: " << name << ": readonly variable\n";
                return;
            }
            it->erase(var_it);
            return;
        }
    }

    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) {
        if (it->second.is_readonly) {
            std::cerr << "aswell: " << name << ": readonly variable\n";
            return;
        }
        global_vars_.erase(it);
    }
}

void Environment::export_var(const std::string& name) {
    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) {
        it->second.is_exported = true;
    } else {
        global_vars_[name] = Variable{"", true, false};
    }
}

void Environment::set_readonly(const std::string& name) {
    auto it = global_vars_.find(name);
    if (it != global_vars_.end()) {
        it->second.is_readonly = true;
    } else {
        global_vars_[name] = Variable{"", false, true};
    }
}

std::map<std::string, Variable> Environment::get_all_vars() const {
    std::map<std::string, Variable> result;
    for (const auto& [k, v] : global_vars_) {
        result[k] = v;
    }
    for (const auto& scope : local_scopes_) {
        for (const auto& [k, v] : scope) {
            result[k] = v;
        }
    }
    return result;
}

std::vector<std::string> Environment::get_env_strings() const {
    std::vector<std::string> envs;
    for (const auto& [k, v] : global_vars_) {
        if (v.is_exported) {
            envs.push_back(k + "=" + v.value);
        }
    }
    return envs;
}

std::vector<char*> Environment::get_envp() const {
    static std::vector<std::string> storage;
    storage = get_env_strings();
    std::vector<char*> res;
    res.reserve(storage.size() + 1);
    for (auto& s : storage) {
        res.push_back(s.data());
    }
    res.push_back(nullptr);
    return res;
}

void Environment::push_scope() {
    local_scopes_.emplace_back();
}

void Environment::pop_scope() {
    if (!local_scopes_.empty()) {
        local_scopes_.pop_back();
    }
}

void Environment::set_positional_params(const std::vector<std::string>& params) {
    current_positional_ = params;
}

void Environment::push_positional_params(const std::vector<std::string>& params) {
    positional_stack_.push_back(current_positional_);
    current_positional_ = params;
}

void Environment::pop_positional_params() {
    if (!positional_stack_.empty()) {
        current_positional_ = positional_stack_.back();
        positional_stack_.pop_back();
    }
}

const std::vector<std::string>& Environment::get_positional_params() const {
    return current_positional_;
}

std::string Environment::get_positional_param(size_t index) const {
    if (index > 0 && index <= current_positional_.size()) {
        return current_positional_[index - 1];
    }
    return "";
}

size_t Environment::get_positional_param_count() const {
    return current_positional_.size();
}

bool Environment::shift_positional_params(size_t n) {
    if (n > current_positional_.size()) {
        return false;
    }
    current_positional_.erase(current_positional_.begin(), current_positional_.begin() + static_cast<std::ptrdiff_t>(n));
    return true;
}

std::string Environment::get_options_flags() const {
    std::string flags;
    if (opt_errexit) flags += 'e';
    if (opt_nounset) flags += 'u';
    if (opt_xtrace) flags += 'x';
    if (opt_verbose) flags += 'v';
    if (opt_interactive) flags += 'i';
    return flags;
}

void Environment::set_alias(const std::string& name, const std::string& value) {
    aliases_[name] = value;
}

bool Environment::get_alias(const std::string& name, std::string& out_value) const {
    auto it = aliases_.find(name);
    if (it != aliases_.end()) {
        out_value = it->second;
        return true;
    }
    return false;
}

void Environment::remove_alias(const std::string& name) {
    aliases_.erase(name);
}

void Environment::clear_aliases() {
    aliases_.clear();
}

void Environment::init_color_aliases() {
    set_alias("ls", "ls --color=auto");
    set_alias("grep", "grep --color=auto");
    set_alias("egrep", "egrep --color=auto");
    set_alias("fgrep", "fgrep --color=auto");
    set_alias("diff", "diff --color=auto");
    set_alias("ip", "ip --color=auto");
}

void Environment::remove_color_aliases() {
    remove_alias("ls");
    remove_alias("grep");
    remove_alias("egrep");
    remove_alias("fgrep");
    remove_alias("diff");
    remove_alias("ip");
}

void Environment::set_function(const std::string& name, std::shared_ptr<FunctionDefNode> func) {
    functions_[name] = func;
}

std::shared_ptr<FunctionDefNode> Environment::get_function(const std::string& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return it->second;
    }
    return nullptr;
}

bool Environment::has_function(const std::string& name) const {
    return functions_.find(name) != functions_.end();
}

void Environment::remove_function(const std::string& name) {
    functions_.erase(name);
}

void Environment::set_trap(int signum, const std::string& action) {
    traps_[signum] = action;
}

std::string Environment::get_trap(int signum) const {
    auto it = traps_.find(signum);
    if (it != traps_.end()) return it->second;
    return "";
}

bool Environment::has_trap(int signum) const {
    return traps_.find(signum) != traps_.end();
}

void Environment::remove_trap(int signum) {
    traps_.erase(signum);
}

std::string Environment::find_in_path(const std::string& cmd) const {
    if (cmd.find('/') != std::string::npos) {
        if (access(cmd.c_str(), X_OK) == 0) {
            return cmd;
        }
        return "";
    }

    std::string path_var = get_var("PATH");
    auto paths = str_util::split(path_var, ':');
    for (const auto& p : paths) {
        std::string full_path = p.empty() ? cmd : (p + "/" + cmd);
        if (access(full_path.c_str(), X_OK) == 0) {
            return full_path;
        }
        if (access((full_path + ".sh").c_str(), X_OK) == 0) {
            return full_path + ".sh";
        }
        // If in user custom commands folder, allow readable scripts
        if (p.find("/.config/aswell/commands") != std::string::npos || p.find("/.config/aswell/bin") != std::string::npos) {
            if (access(full_path.c_str(), R_OK) == 0) {
                return full_path;
            }
            if (access((full_path + ".sh").c_str(), R_OK) == 0) {
                return full_path + ".sh";
            }
        }
    }
    return "";
}

} // namespace aswell
