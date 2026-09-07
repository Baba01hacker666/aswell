#include "aswell/shell/builtins.hpp"
#include <set>

namespace aswell {

bool Builtins::is_builtin(const std::string& name) {
    static const std::set<std::string> builtins = {
        "cd", "pwd", "echo", "printf", "test", "[", "exit",
        "set", "unset", "export", "readonly", "alias", "unalias",
        "eval", "exec", "read", "source", ".", "shift", "trap",
        "type", "wait", "jobs", "fg", "bg", "kill", "hash",
        "umask", "local", "break", "continue", "return",
        "true", "false", ":", "help", "history", "aswell", "color"
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
    if (name == "color") return builtin_color(args, env);

    return 1;
}

} // namespace aswell
