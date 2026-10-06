#include "aswell/editor/completion.hpp"
#include "aswell/shell/builtins.hpp"
#include "aswell/config/alias_library.hpp"
#include "aswell/config/settings.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/config/config.hpp"
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace aswell {

CompletionEngine::CompletionEngine(Environment& env) : env_(env) {}

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Ranking key: raw match class dominates, source bonus only breaks ties
// within the same class. key = raw * 100 + (100 + bonus), so an exact
// match (raw 0) can never lose to a fuzzier builtin/alias candidate.
inline int rank_key(int raw_score, int bonus = 0) {
    return raw_score * 100 + (100 + bonus);
}
inline int raw_score(int key) {
    return key / 100;
}

bool CompletionEngine::subsequence_match(std::string_view pattern, std::string_view target) {
    if (pattern.empty()) return true;
    std::string lp = str_util::to_lower(pattern);
    std::string lt = str_util::to_lower(target);
    size_t p_idx = 0;
    for (char c : lt) {
        if (c == lp[p_idx]) {
            ++p_idx;
            if (p_idx == lp.size()) return true;
        }
    }
    return false;
}

int CompletionEngine::match_score(const std::string& pattern, const std::string& target) {
    if (pattern.empty()) return 1;
    if (pattern == target) return 0;
    // Case-sensitive prefix: best class for Tab completion.
    if (target.size() >= pattern.size() &&
        target.compare(0, pattern.size(), pattern) == 0) {
        return 1;
    }
    std::string lp = str_util::to_lower(pattern);
    std::string lt = str_util::to_lower(target);
    if (lt.size() >= lp.size() && lt.compare(0, lp.size(), lp) == 0) return 2;
    if (lt.find(lp) != std::string::npos) return 3;
    if (subsequence_match(pattern, target)) return 4;
    // Tiny typo tolerance for short command names (fast path only).
    if (pattern.size() >= 3 && target.size() >= 3 &&
        pattern.size() <= 12 && target.size() <= 24) {
        int d = str_util::levenshtein_distance(lp, lt);
        if (d <= 1) return 5;
        if (d == 2 && pattern.size() >= 5) return 6;
    }
    return 100;
}

void CompletionEngine::rank_and_truncate(
    std::vector<std::pair<int, CompletionCandidate>>& scored,
    std::vector<CompletionCandidate>& out, const std::string& prefix, size_t limit) {
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                  if (a.first != b.first) return a.first < b.first;
                  if (a.second.text.size() != b.second.text.size())
                      return a.second.text.size() < b.second.text.size();
                  return a.second.text < b.second.text;
              });
    // Prefer candidates that extend the typed prefix literally: if any
    // prefix matches exist, drop fuzzy-only results to stay predictable.
    bool has_strong = false;
    for (const auto& [key, c] : scored) {
        (void)c;
        if (raw_score(key) <= 2) { has_strong = true; break; }
    }
    for (const auto& [key, cand] : scored) {
        if (out.size() >= limit) break;
        if (has_strong && raw_score(key) > 3) break;
        if (!prefix.empty() && raw_score(key) >= 100) continue;
        out.push_back(cand);
    }
}

void CompletionEngine::refresh_path_cache() {
    std::string path_var = env_.get_var("PATH");
    int64_t now = now_ms();
    if (path_cache_valid_ && path_var == path_cache_key_ &&
        (now - path_cache_time_ms_) < 5000) {
        return; // hot path: no filesystem I/O at all
    }
    path_cache_.clear();
    std::set<std::string> seen;
    auto paths = str_util::split(path_var, ':');
    const char* home_env = std::getenv("HOME");
    std::string custom_cmd_dir = (home_env ? std::string(home_env) : "/root") + "/.config/aswell/commands";
    for (const auto& p : paths) {
        if (p.empty()) continue;
        DIR* dir = opendir(p.c_str());
        if (!dir) continue;
        bool is_custom = (p == custom_cmd_dir);
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            std::string fname = ent->d_name;
            std::string full = p + "/" + fname;
            // Executable check is the bottleneck: only stat when needed.
            // dent d_type shortcut first (DT_UNKNOWN falls back to access).
            bool executable = false;
            if (ent->d_type == DT_REG || ent->d_type == DT_LNK) {
                executable = (access(full.c_str(), X_OK) == 0);
            } else if (ent->d_type == DT_UNKNOWN) {
                executable = (access(full.c_str(), X_OK) == 0);
            } else {
                continue; // skip subdirs and special files in PATH
            }
            if (!executable && !is_custom) continue;
            if (is_custom && !executable) {
                if (access(full.c_str(), R_OK) != 0) continue;
            }
            if (seen.insert(fname).second) {
                path_cache_.emplace_back(fname, p);
                if (is_custom && str_util::ends_with(fname, ".sh")) {
                    std::string stripped = fname.substr(0, fname.size() - 3);
                    if (seen.insert(stripped).second) {
                        path_cache_.emplace_back(stripped, p);
                    }
                }
            }
        }
        closedir(dir);
    }
    path_cache_key_ = path_var;
    path_cache_time_ms_ = now;
    path_cache_valid_ = true;
}

std::vector<CompletionCandidate> CompletionEngine::complete_command(const std::string& prefix) {
    std::vector<std::pair<int, CompletionCandidate>> scored;
    std::set<std::string> seen;

    auto add = [&](const std::string& name, const std::string& desc, int bonus = 0) {
        if (name.empty() || !seen.insert(name).second) return;
        int s = match_score(prefix, name);
        if (s >= 100 && !prefix.empty()) return;
        // Builtins/aliases/functions outrank PATH binaries for identical scores.
        scored.emplace_back(rank_key(s, bonus), CompletionCandidate{name, name, desc, false});
    };

    static const std::vector<std::pair<std::string, std::string>> b_list = {
        {"cd", "builtin: change directory"}, {"pwd", "builtin: print working directory"},
        {"echo", "builtin: write arguments"}, {"printf", "builtin: format and print data"},
        {"test", "builtin: check file types"}, {"exit", "builtin: exit shell"},
        {"set", "builtin: set shell options"}, {"unset", "builtin: unset variables"},
        {"export", "builtin: export variables"}, {"readonly", "builtin: mark readonly"},
        {"alias", "builtin: define alias"}, {"unalias", "builtin: remove alias"},
        {"eval", "builtin: evaluate code"}, {"exec", "builtin: replace process"},
        {"getopts", "builtin: parse command options"},
        {"stty", "builtin: terminal line settings"},
        {"read", "builtin: read line"}, {"source", "builtin: execute script file"},
        {".", "builtin: execute script file"}, {"shift", "builtin: shift arguments"},
        {"trap", "builtin: signal handler"}, {"type", "builtin: describe command"},
        {"wait", "builtin: wait for jobs"}, {"jobs", "builtin: list jobs"},
        {"fg", "builtin: foreground job"}, {"bg", "builtin: background job"},
        {"kill", "builtin: send signal"}, {"history", "builtin: command history"},
        {"parallel", "builtin: run jobs concurrently"}, {"retry", "builtin: rerun until success"},
        {"timeout", "builtin: kill a slow command"}, {"local", "builtin: function-local variable"},
        {"ulimit", "builtin: resource limits"}, {"umask", "builtin: file mode mask"},
        {"aswell", "builtin: configuration"}, {"color", "builtin: colored TrueColor text"},
        {"dirs", "builtin: directory stack"}, {"pushd", "builtin: push directory"},
        {"popd", "builtin: pop directory"}, {"command", "builtin: bypass functions"},
        {"hash", "builtin: command hash"}, {"umask", "builtin: file mask"},
        {"local", "builtin: local variable"}, {"true", "builtin: always succeed"},
        {"false", "builtin: always fail"},
    };
    for (const auto& [b, desc] : b_list) add(b, desc, -10);
    for (const auto& [f, unused] : env_.get_all_functions()) {
        (void)unused;
        add(f, "function", -8);
    }
    for (const auto& [a, unused] : env_.get_aliases()) {
        (void)unused;
        add(a, "alias", -8);
    }

    refresh_path_cache();
    for (const auto& [fname, dir] : path_cache_) {
        if (seen.count(fname)) continue;
        int s = match_score(prefix, fname);
        if (s >= 100 && !prefix.empty()) continue;
        std::string desc = (dir.find(".config/aswell/commands") != std::string::npos)
                               ? "custom command"
                               : "command";
        scored.emplace_back(rank_key(s), CompletionCandidate{fname, fname, desc, false});
        seen.insert(fname);
        if (scored.size() > 4000) break; // pathological PATH guard
    }

    std::vector<CompletionCandidate> out;
    rank_and_truncate(scored, out, prefix, 50);
    return out;
}

std::vector<CompletionCandidate> CompletionEngine::complete_path(const std::string& prefix,
                                                                 bool dirs_only) {
    std::vector<std::pair<int, CompletionCandidate>> scored;

    std::string expanded_prefix = prefix;
    std::string home = env_.get_var("HOME");
    if (home.empty()) {
        const char* h = std::getenv("HOME");
        home = h ? h : "/root";
    }
    if (str_util::starts_with(expanded_prefix, "~")) {
        expanded_prefix = home + expanded_prefix.substr(1);
    }

    std::string dir_path = ".";
    std::string file_prefix = expanded_prefix;
    std::string display_base;
    size_t last_slash = expanded_prefix.find_last_of('/');
    if (last_slash != std::string::npos) {
        dir_path = (last_slash == 0) ? "/" : expanded_prefix.substr(0, last_slash);
        if (dir_path.empty()) dir_path = ".";
        file_prefix = expanded_prefix.substr(last_slash + 1);
        display_base = prefix.substr(0, prefix.find_last_of('/') + 1);
    }

    DIR* dir = opendir(dir_path.empty() ? "." : dir_path.c_str());
    if (!dir) return {};
    struct dirent* ent;
    size_t scanned = 0;
    while ((ent = readdir(dir)) != nullptr) {
        if (++scanned > 2000) break; // cap huge directories for speed
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        if (name[0] == '.' && (file_prefix.empty() || file_prefix[0] != '.')) continue;

        bool is_dir = (ent->d_type == DT_DIR);
        std::string full_path =
            (dir_path == ".") ? name : (dir_path == "/" ? "/" + name : dir_path + "/" + name);
        if (ent->d_type == DT_UNKNOWN || ent->d_type == DT_LNK) {
            struct stat st{};
            if (stat(full_path.c_str(), &st) == 0) is_dir = S_ISDIR(st.st_mode);
        }
        if (dirs_only && !is_dir) continue;

        int s = match_score(file_prefix, name);
        if (s >= 100 && !file_prefix.empty()) continue;
        std::string result_text = display_base + name + (is_dir ? "/" : "");
        scored.emplace_back(rank_key(s), CompletionCandidate{result_text, name + (is_dir ? "/" : ""),
                                                   is_dir ? "directory" : "file", is_dir});
    }
    closedir(dir);

    std::vector<CompletionCandidate> out;
    rank_and_truncate(scored, out, file_prefix, 50);
    return out;
}

std::vector<CompletionCandidate> CompletionEngine::complete_variable(const std::string& prefix,
                                                                     bool bare) {
    std::vector<std::pair<int, CompletionCandidate>> scored;
    std::string var_prefix = prefix;
    bool braced = false;
    if (!var_prefix.empty() && var_prefix[0] == '$') var_prefix = var_prefix.substr(1);
    if (!var_prefix.empty() && var_prefix[0] == '{') {
        braced = true;
        var_prefix = var_prefix.substr(1);
        size_t end = var_prefix.find('}');
        if (end != std::string::npos) var_prefix = var_prefix.substr(0, end);
    }
    for (const auto& [k, v] : env_.get_all_vars()) {
        int s = match_score(var_prefix, k);
        if (s >= 100 && !var_prefix.empty()) continue;
        std::string text = bare ? k : (braced ? "${" + k + "}" : "$" + k);
        std::string desc = v.value.size() > 24 ? v.value.substr(0, 24) + "..." : v.value;
        scored.emplace_back(rank_key(s), CompletionCandidate{text, (bare ? k : "$" + k), desc, false});
    }
    std::vector<CompletionCandidate> out;
    rank_and_truncate(scored, out, var_prefix, 30);
    return out;
}

std::vector<CompletionCandidate> CompletionEngine::complete_git(const std::string& prefix) {
    std::vector<std::pair<int, CompletionCandidate>> scored;
    // Local branches
    DIR* dir = opendir(".git/refs/heads");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            std::string branch = ent->d_name;
            int s = match_score(prefix, branch);
            if (s >= 100 && !prefix.empty()) continue;
            scored.emplace_back(rank_key(s), CompletionCandidate{branch, branch, "branch", false});
        }
        closedir(dir);
    }
    // Remote branches are intentionally skipped here: a single-level
    // .git/refs/remotes scan is rarely worth the I/O on every Tab press.
    // Local branches + subcommands already cover the common cases.
    std::vector<CompletionCandidate> out;
    rank_and_truncate(scored, out, prefix, 30);
    return out;
}

// Static flag tables for instant --flag completion (no subprocess).
std::vector<CompletionCandidate> CompletionEngine::complete_option_flags(
    const std::string& cmd, const std::string& prefix) {
    static const std::unordered_map<std::string, std::vector<std::string>> kFlags = {
        {"ls", {"-l", "-a", "-al", "-lh", "-la", "--color=auto", "-R", "-t", "-S", "-h", "-d"}},
        {"grep", {"-r", "-R", "-n", "-i", "-v", "-l", "-c", "--color=auto", "-E", "-F", "-A", "-B", "-C"}},
        {"git", {"--help", "--version", "-C", "-c"}},
        {"docker", {"--help", "--version", "-v", "--rm", "-it", "-d", "-p", "--name"}},
        {"cargo", {"--help", "--version", "--release", "--debug", "--all-features", "--manifest-path"}},
        {"npm", {"--help", "--version", "--save", "--save-dev", "--global", "-g", "--prefix"}},
        {"kill", {"-9", "-15", "-TERM", "-KILL", "-HUP", "-INT", "-l", "-s"}},
        {"ps", {"aux", "-ef", "-e", "-f", "--forest"}},
        {"find", {"-name", "-iname", "-type", "-exec", "-delete", "-maxdepth", "-mindepth"}},
        {"tar", {"-x", "-c", "-z", "-v", "-f", "-t", "--strip-components"}},
        {"ssh", {"-p", "-i", "-v", "-X", "-L", "-R", "-N", "-f"}},
        {"scp", {"-r", "-P", "-i", "-v", "-C"}},
        {"make", {"-j", "-k", "-n", "-C", "--dry-run"}},
        {"aswell", {"--help", "--version", "--no-theme", "--safe-mode", "--anon", "--config"}},
    };
    auto it = kFlags.find(cmd);
    if (it == kFlags.end()) return {};
    std::vector<CompletionCandidate> out;
    for (const auto& f : it->second) {
        if (match_score(prefix, f) >= 100 && !prefix.empty()) continue;
        out.push_back({f, f, "flag", false});
    }
    return out;
}

std::vector<CompletionCandidate> CompletionEngine::complete_git_args(
    const std::vector<std::string>& args, const std::string& prefix) {
    static const std::vector<std::string> kSub = {
        "add", "branch", "checkout", "switch", "restore", "clone", "commit", "diff",
        "fetch", "log", "merge", "pull", "push", "rebase", "reset", "status",
        "stash", "tag", "show", "remote", "submodule", "worktree", "cherry-pick",
    };
    // git <subcommand> <branch-ish contexts>
    std::string sub = args.size() >= 2 ? args[1] : "";
    if (args.size() <= 2 && (prefix.empty() || prefix[0] != '-')) {
        std::vector<std::pair<int, CompletionCandidate>> scored;
        for (const auto& s : kSub) {
            int sc = match_score(prefix, s);
            if (sc >= 100 && !prefix.empty()) continue;
            scored.emplace_back(rank_key(sc), CompletionCandidate{s, s, "subcommand", false});
        }
        std::vector<CompletionCandidate> out;
        rank_and_truncate(scored, out, prefix, 30);
        return out;
    }
    if (!prefix.empty() && prefix[0] == '-') {
        static const std::vector<std::string> kGitFlags = {
            "--help", "--quiet", "--verbose", "--force", "--all", "--branch",
            "--message", "-m", "-a", "-b", "-f", "--oneline", "--graph", "--decorate",
        };
        std::vector<CompletionCandidate> out;
        for (const auto& f : kGitFlags) {
            if (match_score(prefix, f) >= 100) continue;
            out.push_back({f, f, "flag", false});
        }
        return out;
    }
    if (sub == "checkout" || sub == "switch" || sub == "merge" || sub == "rebase" ||
        sub == "branch" || sub == "diff" || sub == "log" || sub == "show" ||
        sub == "reset" || sub == "cherry-pick") {
        auto branches = complete_git(prefix);
        if (!branches.empty()) return branches;
    }
    return {};
}

std::vector<std::string> CompletionEngine::tokenize(const std::string& s) {
    // Quote-aware split on whitespace for command-position analysis.
    std::vector<std::string> toks;
    std::string cur;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quote) {
            cur += c;
            if (c == quote) quote = 0;
        } else if (c == '\'' || c == '"') {
            quote = c;
            cur += c;
        } else if (c == ' ' || c == '\t') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) toks.push_back(cur);
    return toks;
}

std::vector<CompletionCandidate> CompletionEngine::complete(const std::string& buffer,
                                                            size_t cursor_pos) {
    if (cursor_pos > buffer.size()) cursor_pos = buffer.size();
    std::string text_before_cursor = buffer.substr(0, cursor_pos);

    size_t token_start = text_before_cursor.find_last_of(" \t\n;&|()");
    std::string current_word;
    if (token_start == std::string::npos) {
        current_word = text_before_cursor;
        token_start = 0;
    } else {
        token_start++;
        current_word = text_before_cursor.substr(token_start);
    }
    // Strip surrounding quotes for matching; completion re-addsplain text.
    bool quoted = current_word.size() >= 1 &&
                  (current_word[0] == '\'' || current_word[0] == '"');
    std::string match_word = quoted ? current_word.substr(1) : current_word;

    std::string before_token = str_util::trim(text_before_cursor.substr(0, token_start));
    auto args = tokenize(before_token);
    std::string cmd;
    size_t arg_index = 0; // 0 == command position
    if (!args.empty()) {
        // Handle `sudo git ...`, `command ls`, `env VAR=x cmd` wrappers.
        size_t cmd_idx = 0;
        while (cmd_idx < args.size() &&
               (args[cmd_idx] == "sudo" || args[cmd_idx] == "command" ||
                args[cmd_idx] == "env" || args[cmd_idx] == "xargs" ||
                args[cmd_idx] == "watch" || args[cmd_idx] == "time" ||
                args[cmd_idx] == "nice" || args[cmd_idx].find('=') != std::string::npos)) {
            cmd_idx++;
        }
        if (cmd_idx < args.size()) {
            cmd = args[cmd_idx];
            arg_index = args.size() - cmd_idx; // 1 == first argument
        }
    }
    bool trailing_space = !text_before_cursor.empty() &&
                          (text_before_cursor.back() == ' ' || text_before_cursor.back() == '\t');
    if (trailing_space) {
        current_word.clear();
        match_word.clear();
        if (!args.empty()) arg_index = args.size() - (cmd.empty() ? 0 : 1) + 1;
        if (args.empty()) arg_index = 0;
    }

    bool is_command_position = args.empty() || before_token.empty() ||
                               before_token.back() == ';' || before_token.back() == '|' ||
                               before_token.back() == '&' || before_token.back() == '(';

    // 1. Variables: $VAR and ${VAR}
    if (str_util::starts_with(current_word, "$")) {
        return complete_variable(current_word, false);
    }

    // Effective command for context (strip trailing wrapper detection).
    std::string eff = cmd;
    if (!eff.empty() && eff[0] == '\'' && eff.size() > 1) eff = eff.substr(1);

    // 2. Flags: `--x` after a known command — instant, no FS scan.
    if (!match_word.empty() && match_word[0] == '-' && !eff.empty() && !is_command_position) {
        auto flags = complete_option_flags(eff, match_word);
        if (!flags.empty()) return flags;
        if (eff == "git") {
            auto g = complete_git_args(args, match_word);
            if (!g.empty()) return g;
        }
        // Unknown command flags: fall through to path completion.
    }

    // 3. Command-specific argument completion.
    if (!eff.empty() && !is_command_position) {
        if (eff == "cd" || eff == "pushd" || eff == "rmdir") {
            return complete_path(match_word, true);
        }
        if (eff == "export" || eff == "unset" || eff == "readonly" || eff == "local") {
            return complete_variable(match_word, true);
        }
        if (eff == "alias" || eff == "unalias" || eff == "type" || eff == "which" ||
            eff == "command") {
            if (match_word.find('/') == std::string::npos) {
                auto cmds = complete_command(match_word);
                if (!cmds.empty()) return cmds;
            }
        }
        if (eff == "kill") {
            if (!match_word.empty() && (match_word[0] == '-' || match_word[0] == '%')) {
                static const std::vector<std::string> kSigs = {
                    "-9", "-15", "-TERM", "-KILL", "-HUP", "-INT", "-QUIT", "-ABRT",
                };
                std::vector<CompletionCandidate> out;
                for (const auto& s : kSigs) {
                    if (match_score(match_word, s) >= 100) continue;
                    out.push_back({s, s, "signal", false});
                }
                if (!out.empty()) return out;
            }
        }
        if (eff == "git") {
            auto g = complete_git_args(args, match_word);
            if (!g.empty()) return g;
        }
        if (eff == "aswell") {
            // Context-aware completion for the customization hub: subcommands,
            // then setting names, then the allowed values of that setting. The
            // candidates come from SettingsRegistry, so anything documented is
            // completable without touching this file.
            static const std::vector<std::string> kSubs = {
                "config", "theme", "aliases", "color", "custom", "template", "hooks", "ui",
                "event", "bash", "doctor", "reload", "help", "version",
            };

            std::vector<std::string> candidates;
            std::string kind = "subcommand";
            if (arg_index == 1) {
                candidates = kSubs;
            } else if (args.size() >= 2 && args[1] == "config" && arg_index == 2) {
                candidates = {"list", "get", "set", "toggle", "unset", "reset", "show",
                              "export", "import", "help", "path", "edit", "theme",
                              "username", "hostname"};
                kind = "config command";
            } else if (args.size() >= 3 && args[1] == "theme" && arg_index == 2) {
                candidates = {"list", "set", "preview", "show", "new", "reset"};
                kind = "theme command";
            } else if (args.size() >= 3 && args[1] == "aliases" && arg_index == 2) {
                candidates = {"list", "show", "install", "preview", "remove", "uninstall", "help"};
                kind = "alias command";
            } else if (args.size() >= 4 && args[1] == "aliases" && arg_index >= 3 &&
                       (args[2] == "install" || args[2] == "preview" || args[2] == "remove")) {
                // Categories first; entry names are completed as file words by the
                // caller's fallback, which would be noise here.
                for (const auto& cat : AliasLibrary::categories()) candidates.push_back(cat);
                candidates.push_back("all");
                kind = "alias pack";
            } else if (args.size() >= 3 && args[1] == "config" && args[2] == "theme" &&
                       arg_index == 3) {
                // Legacy spelling: `aswell config theme <name>`.
                candidates = ThemeManager::theme_names(ConfigManager::get_config_dir());
                kind = "theme";
            } else if (args.size() >= 3 && args[1] == "config" && arg_index == 3 &&
                       (args[2] == "get" || args[2] == "toggle" || args[2] == "unset" ||
                        args[2] == "help" || args[2] == "set")) {
                for (const auto& def : SettingsRegistry::all()) candidates.push_back(def.key);
                kind = "setting";
            } else if (args.size() >= 3 && args[1] == "config" && arg_index == 4 && args[2] == "set") {
                const SettingDef* def = SettingsRegistry::find(args[3]);
                if (def) {
                    if (def->type == SettingType::BOOL) {
                        candidates = {"true", "false"};
                    } else {
                        candidates = SettingsRegistry::choices(*def);
                    }
                    kind = "value";
                }
            } else if (args.size() >= 3 && args[1] == "theme" && arg_index == 3 &&
                       (args[2] == "set" || args[2] == "preview" || args[2] == "show" ||
                        args[2] == "new")) {
                candidates = ThemeManager::theme_names(ConfigManager::get_config_dir());
                kind = "theme";
            }

            if (!candidates.empty()) {
                std::vector<std::pair<int, CompletionCandidate>> scored;
                for (const auto& c : candidates) {
                    int sc = match_score(match_word, c);
                    if (sc >= 100 && !match_word.empty()) continue;
                    scored.emplace_back(rank_key(sc), CompletionCandidate{c, c, kind, false});
                }
                std::vector<CompletionCandidate> out;
                rank_and_truncate(scored, out, match_word, 30);
                if (!out.empty()) return out;
            }
        }
    }

    // 4. Command position (and no '/' in word): ranked builtin/alias/PATH.
    if ((is_command_position || arg_index == 0) && match_word.find('/') == std::string::npos) {
        auto cmds = complete_command(match_word);
        if (!cmds.empty()) return cmds;
    }

    // 5. Default: path completion (prefix-ranked, capped).
    return complete_path(match_word, false);
}

} // namespace aswell
