#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/ast.hpp"

extern char **environ;

namespace aswell {

struct Variable {
    std::string value;
    bool is_exported = false;
    bool is_readonly = false;
};

class Environment {
public:
    Environment();

    // Variable management
    void set_var(const std::string& name, const std::string& value, bool export_var = false);
    void set_local_var(const std::string& name, const std::string& value);
    std::string get_var(const std::string& name) const;
    bool has_var(const std::string& name) const;
    bool is_exported(const std::string& name) const;
    bool is_readonly(const std::string& name) const;
    void unset_var(const std::string& name);
    void export_var(const std::string& name);
    void set_readonly(const std::string& name);
    std::map<std::string, Variable> get_all_vars() const;

    // Environment vector for execve
    std::vector<std::string> get_env_strings() const;
    std::vector<char*> get_envp() const;

    // Scope management for functions
    void push_scope();
    void pop_scope();

    // Positional parameters
    void set_positional_params(const std::vector<std::string>& params);
    void push_positional_params(const std::vector<std::string>& params);
    void pop_positional_params();
    const std::vector<std::string>& get_positional_params() const;
    std::string get_positional_param(size_t index) const;
    size_t get_positional_param_count() const;
    bool shift_positional_params(size_t n = 1);

    // Shell state
    int last_exit_status = 0;
    pid_t shell_pid = 0;
    pid_t last_bg_pid = 0;
    std::string shell_name = "aswell";

    // Options
    bool opt_errexit = false;    // -e
    bool opt_nounset = false;    // -u
    bool opt_xtrace = false;     // -x
    bool opt_verbose = false;    // -v
    bool opt_pipefail = false;
    bool opt_interactive = false;
    bool opt_vi_mode = false;
    bool opt_no_theme = false;
    bool opt_safe_mode = false;

    std::string get_options_flags() const;

    // Aliases
    void set_alias(const std::string& name, const std::string& value);
    bool get_alias(const std::string& name, std::string& out_value) const;
    void remove_alias(const std::string& name);
    void clear_aliases();
    void init_color_aliases();
    void remove_color_aliases();
    const std::unordered_map<std::string, std::string>& get_aliases() const { return aliases_; }

    // Functions
    void set_function(const std::string& name, std::shared_ptr<FunctionDefNode> func);
    std::shared_ptr<FunctionDefNode> get_function(const std::string& name) const;
    bool has_function(const std::string& name) const;
    void remove_function(const std::string& name);
    std::unordered_map<std::string, std::shared_ptr<FunctionDefNode>> get_all_functions() const { return functions_; }

    // Traps
    void set_trap(int signum, const std::string& action);
    std::string get_trap(int signum) const;
    bool has_trap(int signum) const;
    void remove_trap(int signum);
    const std::unordered_map<int, std::string>& get_traps() const { return traps_; }

    // Directory stack
    const std::vector<std::string>& get_dir_stack() const { return dir_stack_; }
    std::vector<std::string>& get_dir_stack() { return dir_stack_; }
    void push_dir(const std::string& dir) { dir_stack_.insert(dir_stack_.begin(), dir); }
    bool pop_dir(std::string& out_dir) {
        if (dir_stack_.empty()) return false;
        out_dir = dir_stack_.front();
        dir_stack_.erase(dir_stack_.begin());
        return true;
    }
    void clear_dir_stack() { dir_stack_.clear(); }
    void set_dir_stack(const std::vector<std::string>& stack) { dir_stack_ = stack; }

    // Path resolution
    std::string find_in_path(const std::string& cmd, const std::string& override_path = "") const;

private:
    std::unordered_map<std::string, Variable> global_vars_;
    std::vector<std::unordered_map<std::string, Variable>> local_scopes_;

    std::vector<std::vector<std::string>> positional_stack_;
    std::vector<std::string> current_positional_;

    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<std::string, std::shared_ptr<FunctionDefNode>> functions_;
    std::unordered_map<int, std::string> traps_;
    std::vector<std::string> dir_stack_;
};

} // namespace aswell
