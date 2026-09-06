#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/jobs.hpp"

namespace aswell {

struct ControlFlow {
    enum class Type { NONE, BREAK, CONTINUE, RETURN, EXIT };
    Type type = Type::NONE;
    int count = 1;
    int status = 0;
};

class Executor; // Forward declaration

class Builtins {
public:
    static bool is_builtin(const std::string& name);

    static int execute(const std::string& name,
                       const std::vector<std::string>& args,
                       Environment& env,
                       JobManager& jobs,
                       Executor& executor,
                       ControlFlow& flow);

private:
    static int builtin_cd(const std::vector<std::string>& args, Environment& env);
    static int builtin_pwd(const std::vector<std::string>& args, Environment& env);
    static int builtin_echo(const std::vector<std::string>& args, Environment& env);
    static int builtin_printf(const std::vector<std::string>& args, Environment& env);
    static int builtin_test(const std::vector<std::string>& args, Environment& env);
    static int builtin_exit(const std::vector<std::string>& args, Environment& env, ControlFlow& flow);
    static int builtin_set(const std::vector<std::string>& args, Environment& env);
    static int builtin_unset(const std::vector<std::string>& args, Environment& env);
    static int builtin_export(const std::vector<std::string>& args, Environment& env);
    static int builtin_readonly(const std::vector<std::string>& args, Environment& env);
    static int builtin_alias(const std::vector<std::string>& args, Environment& env);
    static int builtin_unalias(const std::vector<std::string>& args, Environment& env);
    static int builtin_eval(const std::vector<std::string>& args, Environment& env, Executor& executor);
    static int builtin_exec(const std::vector<std::string>& args, Environment& env);
    static int builtin_read(const std::vector<std::string>& args, Environment& env);
    static int builtin_source(const std::vector<std::string>& args, Environment& env, Executor& executor);
    static int builtin_shift(const std::vector<std::string>& args, Environment& env);
    static int builtin_trap(const std::vector<std::string>& args, Environment& env);
    static int builtin_type(const std::vector<std::string>& args, Environment& env);
    static int builtin_wait(const std::vector<std::string>& args, Environment& env, JobManager& jobs);
    static int builtin_jobs(const std::vector<std::string>& args, JobManager& jobs);
    static int builtin_fg(const std::vector<std::string>& args, JobManager& jobs);
    static int builtin_bg(const std::vector<std::string>& args, JobManager& jobs);
    static int builtin_kill(const std::vector<std::string>& args, Environment& env);
    static int builtin_hash(const std::vector<std::string>& args, Environment& env);
    static int builtin_umask(const std::vector<std::string>& args, Environment& env);
    static int builtin_local(const std::vector<std::string>& args, Environment& env);
    static int builtin_break(const std::vector<std::string>& args, ControlFlow& flow);
    static int builtin_continue(const std::vector<std::string>& args, ControlFlow& flow);
    static int builtin_return(const std::vector<std::string>& args, Environment& env, ControlFlow& flow);
    static int builtin_help(const std::vector<std::string>& args);
    static int builtin_history(const std::vector<std::string>& args);
    static int builtin_aswell(const std::vector<std::string>& args, Environment& env, Executor& executor);
};

} // namespace aswell
