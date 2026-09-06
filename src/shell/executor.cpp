#include "aswell/shell/executor.hpp"
#include "aswell/shell/lexer.hpp"
#include "aswell/shell/parser.hpp"
#include "aswell/shell/signals.hpp"
#include <dirent.h>
#include <array>

namespace aswell {

Executor::Executor(Environment& env, JobManager& jobs)
    : env_(env), jobs_(jobs), expansion_(env, [this](const std::string& script) {
        return this->evaluate_command_substitution(script);
    }) {}

int Executor::execute_string(const std::string& line) {
    if (str_util::trim(line).empty()) {
        return 0;
    }

    Lexer lexer(line);
    Parser parser(lexer);
    auto ast = parser.parse_program();

    if (parser.has_error()) {
        std::cerr << "aswell: " << parser.error_message() << "\n";
        env_.last_exit_status = 2;
        return 2;
    }

    if (!ast) {
        return 0;
    }

    auto start_time = std::chrono::steady_clock::now();
    ControlFlow flow;
    int status = execute_command_list(*ast, flow);
    auto end_time = std::chrono::steady_clock::now();
    last_command_duration_ms_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    env_.last_exit_status = status;

    // Check traps
    SignalManager::handle_pending_traps(env_, [this](const std::string& code) {
        this->execute_string(code);
    });

    if (flow.type == ControlFlow::Type::EXIT) {
        exit(flow.status);
    }

    return status;
}

int Executor::execute_script(const std::string& script) {
    return execute_string(script);
}

std::string Executor::evaluate_command_substitution(const std::string& script) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return "";
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        SignalManager::reset_signals_for_child();

        Executor sub_exec(env_, jobs_);
        int ret = sub_exec.execute_string(script);
        _exit(ret);
    }

    // Parent
    close(pipefd[1]);
    std::string output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    return output;
}

bool Executor::apply_redirections(const std::vector<Redirection>& redirs, std::vector<SavedRedirection>& saved) {
    for (const auto& redir : redirs) {
        int default_fd = 0;
        int flags = 0;
        mode_t mode = 0644;

        switch (redir.type) {
            case RedirType::LESS:
                default_fd = 0;
                flags = O_RDONLY;
                break;
            case RedirType::GREAT:
                default_fd = 1;
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                break;
            case RedirType::DGREAT:
                default_fd = 1;
                flags = O_WRONLY | O_CREAT | O_APPEND;
                break;
            case RedirType::CLOBBER:
                default_fd = 1;
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                break;
            case RedirType::LESSGREAT:
                default_fd = 0;
                flags = O_RDWR | O_CREAT;
                break;
            case RedirType::LESSAND:
            case RedirType::GREATAND: {
                int target_fd = (redir.io_number >= 0) ? redir.io_number : (redir.type == RedirType::LESSAND ? 0 : 1);
                std::string target_word = expansion_.expand_word_single(redir.target);
                if (target_word == "-") {
                    // Close fd
                    int backup = dup(target_fd);
                    if (backup >= 0) {
                        saved.push_back({target_fd, backup});
                    }
                    close(target_fd);
                } else {
                    int src_fd = 0;
                    try { src_fd = std::stoi(target_word); } catch (...) { src_fd = -1; }
                    if (src_fd >= 0) {
                        int backup = dup(target_fd);
                        if (backup >= 0) {
                            saved.push_back({target_fd, backup});
                        }
                        dup2(src_fd, target_fd);
                    }
                }
                continue;
            }
            case RedirType::DLESS:
            case RedirType::DLESSDASH: {
                int target_fd = (redir.io_number >= 0) ? redir.io_number : 0;
                std::string content = redir.heredoc_content;
                if (redir.heredoc_expand) {
                    content = expansion_.expand_word_single(content);
                }

                int p[2];
                if (pipe(p) == 0) {
                    ssize_t w = write(p[1], content.data(), content.size());
                    (void)w;
                    close(p[1]);
                    int backup = dup(target_fd);
                    if (backup >= 0) {
                        saved.push_back({target_fd, backup});
                    }
                    dup2(p[0], target_fd);
                    close(p[0]);
                }
                continue;
            }
            case RedirType::HERESTRING: {
                int target_fd = (redir.io_number >= 0) ? redir.io_number : 0;
                std::string content = expansion_.expand_word_single(redir.target) + "\n";
                int p[2];
                if (pipe(p) == 0) {
                    ssize_t w = write(p[1], content.data(), content.size());
                    (void)w;
                    close(p[1]);
                    int backup = dup(target_fd);
                    if (backup >= 0) {
                        saved.push_back({target_fd, backup});
                    }
                    dup2(p[0], target_fd);
                    close(p[0]);
                }
                continue;
            }
        }

        int target_fd = (redir.io_number >= 0) ? redir.io_number : default_fd;
        std::string filename = expansion_.expand_word_single(redir.target);

        int fd = open(filename.c_str(), flags, mode);
        if (fd < 0) {
            std::cerr << "aswell: " << filename << ": " << std::strerror(errno) << "\n";
            return false;
        }

        int backup = dup(target_fd);
        if (backup >= 0) {
            saved.push_back({target_fd, backup});
        }
        dup2(fd, target_fd);
        close(fd);
    }
    return true;
}

void Executor::restore_redirections(std::vector<SavedRedirection>& saved) {
    for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
        dup2(it->backup_fd, it->original_fd);
        close(it->backup_fd);
    }
    saved.clear();
}

int Executor::execute_command_list(CommandListNode& list, ControlFlow& flow) {
    int last_status = 0;

    for (const auto& item : list.items) {
        if (flow.type != ControlFlow::Type::NONE) {
            break;
        }

        if (item.async) {
            // Asynchronous command (&)
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                SignalManager::reset_signals_for_child();
                ControlFlow child_flow;
                int st = execute_and_or(*item.and_or, child_flow);
                _exit(st);
            } else if (pid > 0) {
                env_.last_bg_pid = pid;
                jobs_.add_job(pid, "&", {pid});
                std::cout << "[" << pid << "]\n";
                last_status = 0;
            }
        } else {
            last_status = execute_and_or(*item.and_or, flow);
            env_.last_exit_status = last_status;
        }

        if (env_.opt_errexit && last_status != 0 && flow.type == ControlFlow::Type::NONE) {
            flow.type = ControlFlow::Type::EXIT;
            flow.status = last_status;
            break;
        }
    }

    return last_status;
}

int Executor::execute_and_or(AndOrNode& and_or, ControlFlow& flow) {
    int status = 0;

    for (size_t i = 0; i < and_or.pipelines.size(); ++i) {
        if (flow.type != ControlFlow::Type::NONE) break;

        auto [op, pipe] = and_or.pipelines[i];

        if (op == AndOrNode::Op::AND && status != 0) {
            continue; // Skip because && condition failed
        }
        if (op == AndOrNode::Op::OR && status == 0) {
            continue; // Skip because || condition succeeded
        }

        status = execute_pipeline(*pipe, false, flow);
    }

    return status;
}

int Executor::execute_pipeline(PipelineNode& pipeline, bool /*async*/, ControlFlow& flow) {
    if (pipeline.commands.empty()) return 0;

    // Single command execution without piping
    if (pipeline.commands.size() == 1) {
        int status = execute_command(*pipeline.commands[0], flow);
        if (pipeline.negated) {
            status = (status == 0) ? 1 : 0;
        }
        return status;
    }

    // Multiple commands in pipeline: cmd1 | cmd2 | cmd3
    size_t num_cmds = pipeline.commands.size();
    std::vector<std::array<int, 2>> pipes(num_cmds - 1);
    for (size_t i = 0; i < num_cmds - 1; ++i) {
        if (pipe(pipes[i].data()) != 0) {
            std::cerr << "aswell: pipe creation failed\n";
            return 1;
        }
    }

    std::vector<pid_t> pids(num_cmds);

    for (size_t i = 0; i < num_cmds; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "aswell: fork failed\n";
            return 1;
        }

        if (pid == 0) {
            // Child
            SignalManager::reset_signals_for_child();

            // Connect input pipe
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // Connect output pipe
            if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipe descriptors in child
            for (size_t j = 0; j < num_cmds - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            ControlFlow child_flow;
            int ret = execute_command(*pipeline.commands[i], child_flow);
            _exit(ret);
        }

        pids[i] = pid;
    }

    // Parent closes all pipe fds
    for (size_t i = 0; i < num_cmds - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children in pipeline
    int last_status = 0;
    int first_fail_status = 0;

    for (size_t i = 0; i < num_cmds; ++i) {
        int st = 0;
        waitpid(pids[i], &st, 0);
        int exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : (128 + WTERMSIG(st));
        if (i == num_cmds - 1) {
            last_status = exit_code;
        }
        if (exit_code != 0 && first_fail_status == 0) {
            first_fail_status = exit_code;
        }
    }

    int ret_status = env_.opt_pipefail && first_fail_status != 0 ? first_fail_status : last_status;
    if (pipeline.negated) {
        ret_status = (ret_status == 0) ? 1 : 0;
    }
    return ret_status;
}

int Executor::execute_command(CommandNode& cmd, ControlFlow& flow) {
    switch (cmd.get_type()) {
        case ASTNodeType::SIMPLE_COMMAND:
            return execute_simple_command(static_cast<SimpleCommandNode&>(cmd), flow);
        case ASTNodeType::SUBSHELL:
            return execute_subshell(static_cast<SubshellNode&>(cmd), flow);
        case ASTNodeType::GROUPING:
            return execute_grouping(static_cast<GroupingNode&>(cmd), flow);
        case ASTNodeType::IF:
            return execute_if(static_cast<IfNode&>(cmd), flow);
        case ASTNodeType::FOR:
            return execute_for(static_cast<ForNode&>(cmd), flow);
        case ASTNodeType::WHILE:
            return execute_while(static_cast<WhileNode&>(cmd), flow);
        case ASTNodeType::CASE:
            return execute_case(static_cast<CaseNode&>(cmd), flow);
        case ASTNodeType::FUNCTION_DEF:
            return execute_function_def(static_cast<FunctionDefNode&>(cmd), flow);
        default:
            return 0;
    }
}

int Executor::execute_simple_command(SimpleCommandNode& cmd, ControlFlow& flow) {
    // 1. Variable assignments with no command words
    if (cmd.words.empty()) {
        std::vector<SavedRedirection> saved;
        if (!apply_redirections(cmd.redirections, saved)) {
            restore_redirections(saved);
            return 1;
        }

        for (const auto& [k, v] : cmd.assignments) {
            std::string exp_val = expansion_.expand_word_single(v);
            env_.set_var(k, exp_val);
        }

        restore_redirections(saved);
        return 0;
    }

    // 2. Expand words
    std::vector<std::string> expanded_words = expansion_.expand_words(cmd.words);
    if (expanded_words.empty()) {
        return 0;
    }

    std::string cmd_name = expanded_words[0];

    // Check aliases
    std::string alias_val;
    if (env_.get_alias(cmd_name, alias_val)) {
        // Expand alias
        auto alias_parts = str_util::split(alias_val, ' ');
        if (!alias_parts.empty()) {
            std::vector<std::string> new_words;
            for (const auto& p : alias_parts) if (!p.empty()) new_words.push_back(p);
            for (size_t i = 1; i < expanded_words.size(); ++i) new_words.push_back(expanded_words[i]);
            expanded_words = new_words;
            cmd_name = expanded_words[0];
        }
    }

    // Check if builtin
    if (Builtins::is_builtin(cmd_name)) {
        std::vector<SavedRedirection> saved;
        if (!apply_redirections(cmd.redirections, saved)) {
            restore_redirections(saved);
            return 1;
        }

        // Apply temporary prefix assignments
        env_.push_scope();
        for (const auto& [k, v] : cmd.assignments) {
            env_.set_var(k, expansion_.expand_word_single(v));
        }

        int status = Builtins::execute(cmd_name, expanded_words, env_, jobs_, *this, flow);

        env_.pop_scope();
        restore_redirections(saved);
        return status;
    }

    // Check if shell function
    if (env_.has_function(cmd_name)) {
        auto func = env_.get_function(cmd_name);
        std::vector<SavedRedirection> saved;
        if (!apply_redirections(cmd.redirections, saved)) {
            restore_redirections(saved);
            return 1;
        }

        // Apply temporary prefix assignments
        env_.push_scope();
        for (const auto& [k, v] : cmd.assignments) {
            env_.set_var(k, expansion_.expand_word_single(v));
        }

        std::vector<std::string> func_args(expanded_words.begin() + 1, expanded_words.end());
        int status = execute_function_call(*func, func_args, flow);

        env_.pop_scope();
        restore_redirections(saved);
        return status;
    }

    // External command lookup
    std::string executable_path = env_.find_in_path(cmd_name);
    if (executable_path.empty()) {
        // Enhanced error or POSIX message
        auto similar = find_similar_commands(cmd_name);
        if (env_.opt_interactive && !similar.empty() && !env_.opt_no_theme) {
            std::cerr << "\033[1;31m╭─ Aswell Error\033[0m\n"
                      << "\033[1;31m│\033[0m Command not found: \033[1;37m" << cmd_name << "\033[0m\n"
                      << "\033[1;31m│\033[0m Did you mean:\n";
            for (const auto& s : similar) {
                std::cerr << "\033[1;31m│\033[0m   \033[1;36m" << s << "\033[0m\n";
            }
            std::cerr << "\033[1;31m╰────────────────\033[0m\n";
        } else {
            std::cerr << "aswell: " << cmd_name << ": command not found\n";
        }
        return 127;
    }

    // Fork and exec external command
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "aswell: fork failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    if (pid == 0) {
        // Child process
        SignalManager::reset_signals_for_child();

        std::vector<SavedRedirection> saved;
        if (!apply_redirections(cmd.redirections, saved)) {
            _exit(1);
        }

        // Set temporary prefix assignments
        for (const auto& [k, v] : cmd.assignments) {
            env_.set_var(k, expansion_.expand_word_single(v), true);
        }

        std::vector<char*> argv;
        for (const auto& w : expanded_words) {
            argv.push_back(const_cast<char*>(w.c_str()));
        }
        argv.push_back(nullptr);

        execve(executable_path.c_str(), argv.data(), env_.get_envp().data());
        std::cerr << "aswell: " << executable_path << ": " << std::strerror(errno) << "\n";
        _exit(126);
    }

    // Parent
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : (128 + WTERMSIG(status));
}

int Executor::execute_subshell(SubshellNode& cmd, ControlFlow& /*flow*/) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "aswell: fork failed\n";
        return 1;
    }

    if (pid == 0) {
        SignalManager::reset_signals_for_child();
        std::vector<SavedRedirection> saved;
        if (!apply_redirections(cmd.redirections, saved)) {
            _exit(1);
        }
        ControlFlow child_flow;
        int status = execute_command_list(*cmd.body, child_flow);
        _exit(status);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : (128 + WTERMSIG(status));
}

int Executor::execute_grouping(GroupingNode& cmd, ControlFlow& flow) {
    std::vector<SavedRedirection> saved;
    if (!apply_redirections(cmd.redirections, saved)) {
        restore_redirections(saved);
        return 1;
    }

    int status = execute_command_list(*cmd.body, flow);
    restore_redirections(saved);
    return status;
}

int Executor::execute_if(IfNode& cmd, ControlFlow& flow) {
    std::vector<SavedRedirection> saved;
    if (!apply_redirections(cmd.redirections, saved)) {
        restore_redirections(saved);
        return 1;
    }

    int status = 0;
    bool executed = false;

    for (const auto& branch : cmd.branches) {
        int cond_status = execute_command_list(*branch.condition, flow);
        if (flow.type != ControlFlow::Type::NONE) break;

        if (cond_status == 0) {
            status = execute_command_list(*branch.body, flow);
            executed = true;
            break;
        }
    }

    if (!executed && cmd.else_branch && flow.type == ControlFlow::Type::NONE) {
        status = execute_command_list(*cmd.else_branch, flow);
    }

    restore_redirections(saved);
    return status;
}

int Executor::execute_for(ForNode& cmd, ControlFlow& flow) {
    std::vector<SavedRedirection> saved;
    if (!apply_redirections(cmd.redirections, saved)) {
        restore_redirections(saved);
        return 1;
    }

    std::vector<std::string> words;
    if (cmd.explicit_words) {
        words = expansion_.expand_words(cmd.words);
    } else {
        words = env_.get_positional_params();
    }

    int last_status = 0;

    for (const auto& word : words) {
        env_.set_var(cmd.var_name, word);
        last_status = execute_command_list(*cmd.body, flow);

        if (flow.type == ControlFlow::Type::BREAK) {
            flow.count--;
            if (flow.count <= 0) flow.type = ControlFlow::Type::NONE;
            break;
        } else if (flow.type == ControlFlow::Type::CONTINUE) {
            flow.count--;
            if (flow.count <= 0) {
                flow.type = ControlFlow::Type::NONE;
                continue;
            } else {
                break;
            }
        } else if (flow.type != ControlFlow::Type::NONE) {
            break;
        }
    }

    restore_redirections(saved);
    return last_status;
}

int Executor::execute_while(WhileNode& cmd, ControlFlow& flow) {
    std::vector<SavedRedirection> saved;
    if (!apply_redirections(cmd.redirections, saved)) {
        restore_redirections(saved);
        return 1;
    }

    int last_status = 0;

    while (true) {
        int cond_st = execute_command_list(*cmd.condition, flow);
        if (flow.type != ControlFlow::Type::NONE) break;

        bool cond = (cond_st == 0);
        if (cmd.is_until) cond = !cond;

        if (!cond) break;

        last_status = execute_command_list(*cmd.body, flow);

        if (flow.type == ControlFlow::Type::BREAK) {
            flow.count--;
            if (flow.count <= 0) flow.type = ControlFlow::Type::NONE;
            break;
        } else if (flow.type == ControlFlow::Type::CONTINUE) {
            flow.count--;
            if (flow.count <= 0) {
                flow.type = ControlFlow::Type::NONE;
                continue;
            } else {
                break;
            }
        } else if (flow.type != ControlFlow::Type::NONE) {
            break;
        }
    }

    restore_redirections(saved);
    return last_status;
}

int Executor::execute_case(CaseNode& cmd, ControlFlow& flow) {
    std::vector<SavedRedirection> saved;
    if (!apply_redirections(cmd.redirections, saved)) {
        restore_redirections(saved);
        return 1;
    }

    std::string word = expansion_.expand_word_single(cmd.word);
    int status = 0;

    for (const auto& item : cmd.items) {
        bool matched = false;
        for (const auto& pat : item.patterns) {
            std::string exp_pat = expansion_.expand_word_single(pat);
            if (Expansion::fnmatch_pattern(exp_pat, word)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            status = execute_command_list(*item.body, flow);
            break;
        }
    }

    restore_redirections(saved);
    return status;
}

int Executor::execute_function_def(FunctionDefNode& cmd, ControlFlow& /*flow*/) {
    env_.set_function(cmd.name, std::make_shared<FunctionDefNode>(cmd));
    return 0;
}

int Executor::execute_function_call(FunctionDefNode& func, const std::vector<std::string>& args, ControlFlow& flow) {
    env_.push_scope();
    env_.push_positional_params(args);

    std::vector<SavedRedirection> saved;
    if (!apply_redirections(func.redirections, saved)) {
        env_.pop_positional_params();
        env_.pop_scope();
        restore_redirections(saved);
        return 1;
    }

    int status = execute_command(*func.body, flow);

    if (flow.type == ControlFlow::Type::RETURN) {
        status = flow.status;
        flow.type = ControlFlow::Type::NONE;
    }

    restore_redirections(saved);
    env_.pop_positional_params();
    env_.pop_scope();
    return status;
}

std::vector<std::string> Executor::find_similar_commands(const std::string& target) const {
    std::vector<std::pair<int, std::string>> candidates;

    auto check_name = [&](const std::string& name) {
        int dist = str_util::levenshtein_distance(target, name);
        if (dist >= 0 && dist <= 2 && static_cast<size_t>(dist) < target.size()) {
            candidates.push_back({dist, name});
        }
    };

    // Check builtins
    static const std::vector<std::string> b_list = {
        "cd", "pwd", "echo", "printf", "test", "exit", "set", "unset", "export",
        "readonly", "alias", "unalias", "eval", "exec", "read", "source", "shift",
        "trap", "type", "wait", "jobs", "fg", "bg", "kill", "aswell"
    };
    for (const auto& b : b_list) check_name(b);

    // Check PATH directories
    auto paths = str_util::split(env_.get_var("PATH"), ':');
    for (const auto& p : paths) {
        DIR* dir = opendir(p.c_str());
        if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] != '.') {
                check_name(entry->d_name);
            }
        }
        closedir(dir);
    }

    std::sort(candidates.begin(), candidates.end());
    std::vector<std::string> result;
    for (const auto& [dist, name] : candidates) {
        if (std::find(result.begin(), result.end(), name) == result.end()) {
            result.push_back(name);
            if (result.size() >= 3) break;
        }
    }
    return result;
}

} // namespace aswell
