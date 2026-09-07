#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/ast.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/expansion.hpp"
#include "aswell/shell/jobs.hpp"
#include "aswell/shell/builtins.hpp"
#include <chrono>

namespace drills {
}

namespace aswell {

struct SavedRedirection {
    int original_fd;
    int backup_fd;
};

class Executor {
public:
    Executor(Environment& env, JobManager& jobs);

    int execute_string(const std::string& line);
    int execute_script(const std::string& script);

    int execute_command_list(CommandListNode& list, ControlFlow& flow);
    int execute_and_or(AndOrNode& and_or, ControlFlow& flow);
    int execute_pipeline(PipelineNode& pipeline, bool async, ControlFlow& flow);
    int execute_command(CommandNode& cmd, ControlFlow& flow);

    int execute_simple_command(SimpleCommandNode& cmd, ControlFlow& flow);
    int execute_subshell(SubshellNode& cmd, ControlFlow& flow);
    int execute_grouping(GroupingNode& cmd, ControlFlow& flow);
    int execute_if(IfNode& cmd, ControlFlow& flow);
    int execute_for(ForNode& cmd, ControlFlow& flow);
    int execute_while(WhileNode& cmd, ControlFlow& flow);
    int execute_case(CaseNode& cmd, ControlFlow& flow);
    int execute_function_def(FunctionDefNode& cmd, ControlFlow& flow);
    int execute_function_call(FunctionDefNode& func, const std::vector<std::string>& args, ControlFlow& flow);
    int execute_function(const std::string& name, const std::vector<std::string>& args);
    int execute_external(const std::string& executable_path, const std::vector<std::string>& args);

    std::string evaluate_command_substitution(const std::string& script);

    // Redirection application and restoration
    bool apply_redirections(const std::vector<Redirection>& redirs, std::vector<SavedRedirection>& saved);
    void restore_redirections(std::vector<SavedRedirection>& saved);

    Environment& get_env() { return env_; }
    JobManager& get_jobs() { return jobs_; }
    Expansion& get_expansion() { return expansion_; }

    double get_last_command_duration_ms() const { return last_command_duration_ms_; }

    // Suggest commands for error message
    std::vector<std::string> find_similar_commands(const std::string& target) const;

private:
    Environment& env_;
    JobManager& jobs_;
    Expansion expansion_;
    double last_command_duration_ms_ = 0.0;
};

} // namespace aswell
