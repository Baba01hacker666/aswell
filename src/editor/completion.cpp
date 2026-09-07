#include "aswell/editor/completion.hpp"
#include "aswell/shell/builtins.hpp"
#include <dirent.h>

namespace aswell {

CompletionEngine::CompletionEngine(Environment& env) : env_(env) {}

static bool subsequence_match(std::string_view pattern, std::string_view target) {
    if (pattern.empty()) return true;
    size_t p_idx = 0;
    std::string lp = str_util::to_lower(pattern);
    std::string lt = str_util::to_lower(target);

    for (char c : lt) {
        if (c == lp[p_idx]) {
            p_idx++;
            if (p_idx == lp.size()) return true;
        }
    }
    return false;
}

std::vector<CompletionCandidate> CompletionEngine::complete_command(const std::string& prefix) {
    std::vector<CompletionCandidate> results;
    std::set<std::string> seen;

    auto add = [&](const std::string& name, const std::string& desc) {
        if (seen.find(name) == seen.end() && subsequence_match(prefix, name)) {
            seen.insert(name);
            results.push_back({name, name, desc, false});
        }
    };

    // 1. Builtins
    static const std::vector<std::pair<std::string, std::string>> b_list = {
        {"cd", "Change directory"}, {"pwd", "Print working directory"},
        {"echo", "Write arguments"}, {"printf", "Format and print data"},
        {"test", "Check file types and compare values"}, {"exit", "Exit shell"},
        {"set", "Set shell options"}, {"unset", "Unset variables"},
        {"export", "Export variables"}, {"readonly", "Mark variables readonly"},
        {"alias", "Define alias"}, {"unalias", "Remove alias"},
        {"eval", "Evaluate arguments as shell code"}, {"exec", "Replace shell process"},
        {"read", "Read line from stdin"}, {"source", "Execute script file"},
        {"shift", "Shift positional arguments"}, {"trap", "Signal handler"},
        {"type", "Describe command type"}, {"wait", "Wait for jobs"},
        {"jobs", "List active jobs"}, {"fg", "Bring job to foreground"},
        {"bg", "Run job in background"}, {"kill", "Send signal to process"},
        {"history", "Show command history"}, {"aswell", "Aswell configuration"},
        {"color", "Output colored text with TrueColor styling"},
        {"dirs", "Display or modify directory stack"},
        {"pushd", "Push directory to stack and cd"},
        {"popd", "Pop directory from stack and cd"},
        {"command", "Execute simple command bypassing functions"}
    };
    for (const auto& [b, desc] : b_list) {
        add(b, desc);
    }

    // 2. Functions & Aliases
    for (const auto& [f, _] : env_.get_all_functions()) {
        add(f, "function");
    }
    for (const auto& [a, _] : env_.get_aliases()) {
        add(a, "alias");
    }

    // 3. Custom commands in ~/.config/aswell/commands
    const char* home_env = std::getenv("HOME");
    std::string custom_cmd_dir = (home_env ? std::string(home_env) : "/root") + "/.config/aswell/commands";
    DIR* cdir = opendir(custom_cmd_dir.c_str());
    if (cdir) {
        struct dirent* ent;
        while ((ent = readdir(cdir)) != nullptr) {
            if (ent->d_name[0] != '.') {
                std::string fname = ent->d_name;
                if (str_util::ends_with(fname, ".sh")) {
                    add(fname.substr(0, fname.size() - 3), "custom command");
                }
                add(fname, "custom command");
            }
        }
        closedir(cdir);
    }

    // 4. Executables in PATH
    auto paths = str_util::split(env_.get_var("PATH"), ':');
    for (const auto& p : paths) {
        DIR* dir = opendir(p.c_str());
        if (!dir) continue;
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] != '.') {
                std::string fname = ent->d_name;
                add(fname, "command");
            }
        }
        closedir(dir);
    }

    return results;
}

std::vector<CompletionCandidate> CompletionEngine::complete_path(const std::string& prefix) {
    std::vector<CompletionCandidate> results;

    std::string expanded_prefix = prefix;
    std::string home = env_.get_var("HOME");
    if (str_util::starts_with(expanded_prefix, "~")) {
        expanded_prefix = home + expanded_prefix.substr(1);
    }

    std::string dir_path = ".";
    std::string file_prefix = expanded_prefix;

    size_t last_slash = expanded_prefix.find_last_of('/');
    if (last_slash != std::string::npos) {
        dir_path = (last_slash == 0) ? "/" : expanded_prefix.substr(0, last_slash);
        file_prefix = expanded_prefix.substr(last_slash + 1);
    }

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return results;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;

        // Hidden files only if prefix starts with '.'
        if (name[0] == '.' && (file_prefix.empty() || file_prefix[0] != '.')) {
            continue;
        }

        if (subsequence_match(file_prefix, name)) {
            std::string full_path = (dir_path == ".") ? name : (dir_path == "/" ? "/" + name : dir_path + "/" + name);
            struct stat st;
            bool is_dir = (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));

            std::string result_text = name + (is_dir ? "/" : "");
            if (last_slash != std::string::npos) {
                std::string base_dir = prefix.substr(0, prefix.find_last_of('/') + 1);
                result_text = base_dir + name + (is_dir ? "/" : "");
            }

            results.push_back({result_text, name + (is_dir ? "/" : ""), is_dir ? "directory" : "file", is_dir});
        }
    }
    closedir(dir);

    return results;
}

std::vector<CompletionCandidate> CompletionEngine::complete_variable(const std::string& prefix) {
    std::vector<CompletionCandidate> results;
    std::string var_prefix = (prefix.size() > 1 && prefix[0] == '$') ? prefix.substr(1) : prefix;

    for (const auto& [k, v] : env_.get_all_vars()) {
        if (subsequence_match(var_prefix, k)) {
            results.push_back({"$" + k, "$" + k, v.value.substr(0, 20), false});
        }
    }
    return results;
}

std::vector<CompletionCandidate> CompletionEngine::complete_git(const std::string& prefix) {
    std::vector<CompletionCandidate> results;
    std::string git_heads = ".git/refs/heads";
    DIR* dir = opendir(git_heads.c_str());
    if (!dir) return results;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] != '.') {
            std::string branch = ent->d_name;
            if (subsequence_match(prefix, branch)) {
                results.push_back({branch, branch, "git branch", false});
            }
        }
    }
    closedir(dir);
    return results;
}

std::vector<CompletionCandidate> CompletionEngine::complete(const std::string& buffer, size_t cursor_pos) {
    std::string text_before_cursor = buffer.substr(0, cursor_pos);

    // Find the current token being typed
    size_t token_start = text_before_cursor.find_last_of(" \t\n;&|");
    std::string current_word;
    if (token_start == std::string::npos) {
        current_word = text_before_cursor;
        token_start = 0;
    } else {
        token_start++;
        current_word = text_before_cursor.substr(token_start);
    }

    // Determine context: is this command position?
    std::string before_token = str_util::trim(text_before_cursor.substr(0, token_start));
    bool is_command_position = before_token.empty() ||
                               before_token.back() == ';' ||
                               before_token.back() == '|' ||
                               before_token.back() == '&' ||
                               before_token.back() == '(';

    // 1. Variable completion if starts with $
    if (str_util::starts_with(current_word, "$")) {
        return complete_variable(current_word);
    }

    // 2. Git branch completion
    auto tokens = str_util::split(before_token, ' ');
    if (!tokens.empty()) {
        std::string prev = tokens.back();
        if (prev == "checkout" || prev == "switch" || prev == "merge" || prev == "rebase") {
            auto git_res = complete_git(current_word);
            if (!git_res.empty()) return git_res;
        }
    }

    // 3. Command completion if at command start and doesn't contain '/'
    if (is_command_position && current_word.find('/') == std::string::npos) {
        auto cmds = complete_command(current_word);
        if (!cmds.empty()) return cmds;
    }

    // 4. Default: path completion
    return complete_path(current_word);
}

} // namespace aswell
