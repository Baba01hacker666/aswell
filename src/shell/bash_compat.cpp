#include "aswell/shell/bash_compat.hpp"
#include "aswell/shell/executor.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <poll.h>
#include <sys/stat.h>

namespace aswell {

namespace {
int64_t bash_compat_now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

std::string BashCompat::default_bashrc_path() {
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/root";
    return base + "/.bashrc";
}

bool BashCompat::bash_available() {
    return access("/bin/bash", X_OK) == 0 || access("/usr/bin/bash", X_OK) == 0;
}

bool BashCompat::should_skip_var(const std::string& name) {
    // Bash internals, prompt machinery, completion state, history tuning.
    // Everything else (EDITOR, LANG, PATH, user exports like OPENCODE_API_KEY)
    // is imported.
    static const char* exact[] = {
        "_", "SHLVL", "OLDPWD", "PPID", "BASHPID", "BASH_SUBSHELL",
        "EUID", "UID", "GROUPS", "SHELLOPTS", "BASHOPTS", "SHELLOPTS",
        "PS1", "PS2", "PS4", "PROMPT_COMMAND", "PROMPT_DIRTRIM",
        "HISTCONTROL", "HISTFILE", "HISTSIZE", "HISTFILESIZE", "HISTIGNORE",
        "COMP_WORDBREAKS", "COMPREPLY", "COMP_LINE", "COMP_POINT", "COMP_WORDS",
        "COMP_CWORD", "COPROC", "DIRSTACK", "FUNCNAME", "PIPESTATUS",
        "TERM_PROGRAM", nullptr,
    };
    for (const char** p = exact; *p; ++p) {
        if (name == *p) return true;
    }
    if (name.compare(0, 5, "BASH_") == 0) return true;
    if (name.compare(0, 5, "COMP_") == 0) return true;
    if (name.compare(0, 5, "HIST") == 0) return true;
    return false;
}

bool BashCompat::should_skip_function(const std::string& name, const std::string& body) {
    if (name.empty()) return true;
    if (name[0] == '_') return true; // completion helpers
    if (body.find("COMPREPLY") != std::string::npos) return true;
    if (body.find("compgen") != std::string::npos) return true;
    if (body.find("_completion") != std::string::npos) return true;
    if (body.size() > 8192) return true; // sanity cap
    return false;
}

// Decode bash quoting for alias/export values.
std::string BashCompat::bash_unquote(const std::string& raw) {
    std::string s = str_util::trim(raw);
    if (s.size() >= 3 && s[0] == '$' && s[1] == '\'') {
        // $'ansi-c' quoting
        std::string out;
        for (size_t i = 2; i + 1 < s.size();) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size() - 1) {
                char n = s[i + 1];
                switch (n) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'e': out += '\033'; break;
                    case '\\': out += '\\'; break;
                    case '\'': out += '\''; break;
                    case '"': out += '"'; break;
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        // octal escape \ooo
                        int val = 0;
                        size_t k = i + 1;
                        for (int d = 0; d < 3 && k < s.size() - 1 && s[k] >= '0' && s[k] <= '7'; ++d, ++k) {
                            val = val * 8 + (s[k] - '0');
                        }
                        out += static_cast<char>(val);
                        i = k;
                        continue;
                    }
                    default: out += n; break;
                }
                i += 2;
            } else {
                out += c;
                ++i;
            }
        }
        return out;
    }
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
        // 'single' with '\'' escape sequences
        std::string inner = s.substr(1, s.size() - 2);
        std::string out;
        for (size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '\'' && i + 3 < inner.size() + 1 && inner.compare(i, 4, "'\\''") == 0) {
                out += '\'';
                i += 3;
            } else {
                out += inner[i];
            }
        }
        return out;
    }
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        std::string inner = s.substr(1, s.size() - 2);
        std::string out;
        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '\\' && i + 1 < inner.size()) {
                char n = inner[++i];
                switch (n) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case '$': out += '$'; break;
                    case '`': out += '`'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    default: out += n; break;
                }
            } else {
                out += c;
            }
        }
        return out;
    }
    return s;
}

std::string BashCompat::expand_simple(const std::string& val, Environment& env) {
    // Expand $HOME, $USER, ${HOME}, ~ prefixes and $PATH references using the
    // current environment. Full POSIX expansion happens at execution time;
    // this just resolves the common cases found in bashrc PATH lines.
    std::string out;
    out.reserve(val.size());
    const char* home = std::getenv("HOME");
    std::string home_s = home ? home : env.get_var("HOME");
    for (size_t i = 0; i < val.size();) {
        if (val[i] == '~' && (i == 0 || val[i - 1] == ':') &&
            (i + 1 >= val.size() || val[i + 1] == '/' || val[i + 1] == ':')) {
            out += home_s;
            ++i;
        } else if (val[i] == '$' && i + 1 < val.size()) {
            if (val[i + 1] == '{') {
                size_t end = val.find('}', i + 2);
                if (end != std::string::npos) {
                    std::string var = val.substr(i + 2, end - (i + 2));
                    const char* ev = std::getenv(var.c_str());
                    out += ev ? ev : env.get_var(var);
                    i = end + 1;
                    continue;
                }
                out += val[i++];
            } else if (std::isalpha(static_cast<unsigned char>(val[i + 1])) || val[i + 1] == '_') {
                size_t j = i + 1;
                while (j < val.size() && (std::isalnum(static_cast<unsigned char>(val[j])) || val[j] == '_')) ++j;
                std::string var = val.substr(i + 1, j - (i + 1));
                const char* ev = std::getenv(var.c_str());
                out += ev ? ev : env.get_var(var);
                i = j;
            } else {
                out += val[i++];
            }
        } else {
            out += val[i++];
        }
    }
    return out;
}

void BashCompat::merge_path(Environment& env, const std::string& imported_path) {
    if (imported_path.empty()) return;
    const char* home = std::getenv("HOME");
    std::string home_s = home ? home : "/root";
    std::string custom1 = home_s + "/.config/aswell/commands";
    std::string custom2 = home_s + "/.config/aswell/bin";

    // Preserve aswell custom dirs at the front, then the imported PATH with
    // duplicates removed.
    std::vector<std::string> parts = str_util::split(imported_path, ':');
    std::vector<std::string> merged;
    merged.push_back(custom1);
    merged.push_back(custom2);
    std::set<std::string> seen{custom1, custom2};
    auto push_unique = [&](const std::string& p) {
        if (p.empty() || seen.count(p)) return;
        seen.insert(p);
        merged.push_back(p);
    };
    for (const auto& p : parts) {
        if (p == custom1 || p == custom2) continue;
        push_unique(expand_simple(p, env));
    }
    // Keep any entries from the pre-import PATH that bash dropped (safety).
    for (const auto& p : str_util::split(env.get_var("PATH"), ':')) {
        push_unique(p);
    }
    std::string joined;
    for (size_t i = 0; i < merged.size(); ++i) {
        if (i) joined += ':';
        joined += merged[i];
    }
    env.set_var("PATH", joined, true);
}

std::string BashCompat::run_bash_dump() {
    // Never attempt to execute bash when it is not installed: callers and
    // this guard together guarantee no exec happens on bash-less systems
    // (minimal containers, embedded, some BSDs).
    if (!bash_available()) return {};
    // -i makes bash treat the shell as interactive so ~/.bashrc's
    // "case $- in *i*)" guard passes. +m disables job control inside the
    // dump shell so it can never stop on SIGTTOU/SIGTTIN against our tty.
    // NOTE: the inner script must not contain single quotes: it is wrapped in
    // `bash -i +m -c '...'` so any inner ' would terminate the -c string early.
    const char* inner =
        "source ~/.bashrc 2>/dev/null; "
        "echo __ASWELL_ALIAS_BEGIN__; alias -p 2>/dev/null; "
        "echo __ASWELL_EXPORT_BEGIN__; export -p 2>/dev/null; "
        "echo __ASWELL_FUNC_BEGIN__; "
        "for __w in $(declare -F 2>/dev/null); do "
        "case \"$__w\" in declare|-f|__w) continue;; _*) continue;; esac; "
        "declare -f \"$__w\" 2>/dev/null; done; "
        "echo __ASWELL_FUNC_END__";
    std::string cmd = std::string("PS1=x exec bash -i +m -c '") + inner + "'";

    // fork/pipe instead of popen so the parent can enforce a hard deadline:
    // a stopped or wedged child is SIGKILLed (SIGKILL works even on stopped
    // processes, unlike SIGTERM), so startup can never hang here.
    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) != 0) return {};
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return {};
    }
    if (pid == 0) {
        // Child: detach from the controlling terminal (no tty-stop signals
        // possible), wire stdout to the pipe, silence stdin/stderr.
        close(pipefd[0]);
        if (pipefd[1] != STDOUT_FILENO) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }
        int devnull_r = open("/dev/null", O_RDONLY);
        if (devnull_r >= 0) {
            dup2(devnull_r, STDIN_FILENO);
            if (devnull_r != STDIN_FILENO) close(devnull_r);
        }
        int devnull_w = open("/dev/null", O_WRONLY);
        if (devnull_w >= 0) {
            dup2(devnull_w, STDERR_FILENO);
            if (devnull_w != STDERR_FILENO) close(devnull_w);
        }
        setsid();
        signal(SIGTTOU, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        execl("/bin/bash", "bash", "-c", cmd.c_str(), static_cast<char*>(nullptr));
        execl("/usr/bin/bash", "bash", "-c", cmd.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    // Parent: bounded read with an 8s deadline, 1MB cap.
    close(pipefd[1]);
    std::string out;
    char buf[4096];
    const int64_t deadline_ms = bash_compat_now_ms() + 8000;
    bool timed_out = false;
    while (out.size() <= 1024 * 1024) {
        int64_t remain = deadline_ms - bash_compat_now_ms();
        if (remain <= 0) {
            timed_out = true;
            break;
        }
        struct pollfd pfd{};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN | POLLHUP | POLLERR;
        int pr = poll(&pfd, 1, static_cast<int>(remain));
        if (pr <= 0) {
            if (pr == 0) timed_out = true;
            break; // deadline hit or poll error
        }
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break; // EOF or error
        out.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);
    if (timed_out) kill(pid, SIGKILL);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (timed_out) return {};
    return out;
}

BashImportStats BashCompat::import_dump(const std::string& dump, Environment& env,
                                        Executor* exec, bool verbose) {
    BashImportStats stats;
    enum class Section { NONE, ALIAS, EXPORT, FUNC };
    Section sec = Section::NONE;
    std::string pending_func;

    auto flush_func = [&]() {
        if (pending_func.empty()) return;
        // Function header is "<name> ()" on the first line.
        std::string first_line = pending_func.substr(0, pending_func.find('\n'));
        std::string fname = str_util::trim(first_line);
        size_t sp = fname.find(' ');
        if (sp != std::string::npos) fname = fname.substr(0, sp);
        size_t par = fname.find('(');
        if (par != std::string::npos) fname = fname.substr(0, par);
        fname = str_util::trim(fname);
        if (!fname.empty() && !should_skip_function(fname, pending_func)) {
            std::string existing_check;
            bool has_alias = env.get_alias(fname, existing_check);
            bool has_func = env.has_function(fname);
            // Define via aswell's own parser so the function becomes a real
            // typed AST node (POSIX-compatible bodies work as-is).
            if (!has_alias && !has_func && exec) {
                int rc = exec->execute_string(pending_func);
                if (rc == 0 && env.has_function(fname)) {
                    ++stats.functions;
                } else if (verbose && rc != 0) {
                    std::cerr << "aswell: bashrc: skipping function '" << fname
                              << "' (not POSIX-compatible)\n";
                }
            } else if (!has_alias && !has_func && !exec) {
                // No executor (unit tests): count as importable.
                ++stats.functions;
            }
        }
        pending_func.clear();
    };

    std::istringstream iss(dump);
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "__ASWELL_ALIAS_BEGIN__") { sec = Section::ALIAS; continue; }
        if (line == "__ASWELL_EXPORT_BEGIN__") { sec = Section::EXPORT; continue; }
        if (line == "__ASWELL_FUNC_BEGIN__") { sec = Section::FUNC; continue; }
        if (line == "__ASWELL_FUNC_END__") { flush_func(); sec = Section::NONE; continue; }
        if (sec == Section::NONE) continue;

        if (sec == Section::ALIAS) {
            std::string t = str_util::trim(line);
            if (t.compare(0, 6, "alias ") != 0) continue;
            std::string rest = t.substr(6);
            size_t eq = rest.find('=');
            if (eq == std::string::npos) continue;
            std::string name = str_util::trim(rest.substr(0, eq));
            std::string value = bash_unquote(rest.substr(eq + 1));
            if (name.empty()) continue;
            std::string existing;
            if (!env.get_alias(name, existing)) {
                env.set_alias(name, value);
                ++stats.aliases;
            }
        } else if (sec == Section::EXPORT) {
            std::string t = str_util::trim(line);
            if (t.compare(0, 11, "declare -x ") != 0) continue;
            std::string rest = t.substr(11);
            size_t eq = rest.find('=');
            std::string name = (eq == std::string::npos) ? str_util::trim(rest)
                                                         : str_util::trim(rest.substr(0, eq));
            if (name.empty() || should_skip_var(name)) continue;
            if (name == "PATH") continue; // handled separately after full parse
            if (eq == std::string::npos) {
                if (!env.has_var(name)) env.export_var(name);
                continue;
            }
            std::string value = bash_unquote(rest.substr(eq + 1));
            if (!env.has_var(name)) {
                env.set_var(name, value, true);
                ++stats.variables;
            }
        } else { // FUNC
            if (line == "}") {
                pending_func += line + "\n";
                flush_func();
                continue;
            }
            // New function header looks like "name ()" (bash 5: "name () " ).
            bool is_header = pending_func.empty() && line.find("()") != std::string::npos &&
                             !line.empty() && line[0] != ' ' && line[0] != '\t' && line[0] != '}';
            if (is_header) {
                flush_func();
                pending_func = line + "\n";
            } else if (!pending_func.empty()) {
                pending_func += line + "\n";
            }
        }
    }
    flush_func();

    // PATH needs merge semantics (keep aswell custom dirs first).
    size_t path_pos = dump.find("__ASWELL_EXPORT_BEGIN__");
    if (path_pos != std::string::npos) {
        std::istringstream pss(dump.substr(path_pos));
        std::string pline;
        while (std::getline(pss, pline)) {
            if (pline == "__ASWELL_FUNC_BEGIN__") break;
            std::string t = str_util::trim(pline);
            if (t.compare(0, 11, "declare -x ") != 0) continue;
            std::string rest = t.substr(11);
            if (rest.compare(0, 5, "PATH=") == 0) {
                std::string pval = bash_unquote(rest.substr(5));
                std::string before = env.get_var("PATH");
                merge_path(env, pval);
                if (env.get_var("PATH") != before) ++stats.variables;
                break;
            }
        }
    }
    stats.used_bash_dump = true;
    return stats;
}

BashImportStats BashCompat::import_file(const std::string& path, Environment& env, Executor* exec) {
    BashImportStats stats;
    std::ifstream f(path);
    if (!f) return stats;
    std::string line;
    std::string pending_func;
    auto flush_pending = [&]() {
        if (!pending_func.empty() && exec) {
            std::string first = pending_func.substr(0, pending_func.find('\n'));
            std::string fname = str_util::trim(first);
            size_t sp = fname.find_first_of(" (");
            if (sp != std::string::npos) fname = fname.substr(0, sp);
            if (!fname.empty() && !should_skip_function(fname, pending_func)) {
                std::string av;
                if (!env.get_alias(fname, av) && !env.has_function(fname)) {
                    if (exec->execute_string(pending_func) == 0 && env.has_function(fname)) {
                        ++stats.functions;
                    }
                }
            }
            pending_func.clear();
        } else {
            pending_func.clear();
        }
    };
    while (std::getline(f, line)) {
        std::string t = str_util::trim(line);
        if (t.empty() || t[0] == '#') continue;
        // Strip trailing comments outside quotes (best effort).
        // alias lines
        if (t.compare(0, 6, "alias ") == 0 || t.compare(0, 6, "alias\t") == 0) {
            std::string rest = str_util::trim(t.substr(5));
            size_t eq = rest.find('=');
            if (eq == std::string::npos) continue;
            std::string name = str_util::trim(rest.substr(0, eq));
            std::string value = bash_unquote(rest.substr(eq + 1));
            // Strip inline trailing comment outside quotes (simple scan).
            if (name.empty() || name.find(' ') != std::string::npos) continue;
            std::string existing;
            if (!env.get_alias(name, existing)) {
                env.set_alias(name, value);
                ++stats.aliases;
            }
            continue;
        }
        // export VAR=val / export VAR
        if (t.compare(0, 7, "export ") == 0 || t == "export") {
            std::string rest = t.size() > 7 ? str_util::trim(t.substr(7)) : "";
            if (rest.empty()) continue;
            // handle `export A=1 B=2` minimally: take first assignment only if single
            size_t eq = rest.find('=');
            if (eq == std::string::npos) {
                std::string name = str_util::trim(rest);
                size_t sp = name.find_first_of(" \t");
                if (sp != std::string::npos) name = name.substr(0, sp);
                if (!name.empty() && !should_skip_var(name) && !env.has_var(name)) {
                    env.export_var(name);
                }
                continue;
            }
            std::string name = str_util::trim(rest.substr(0, eq));
            size_t sp = name.find_last_of(" \t");
            if (sp != std::string::npos) name = name.substr(sp + 1);
            if (name.empty() || should_skip_var(name)) continue;
            std::string value = expand_simple(bash_unquote(str_util::trim(rest.substr(eq + 1))), env);
            if (name == "PATH") {
                std::string before = env.get_var("PATH");
                merge_path(env, value);
                if (env.get_var("PATH") != before) ++stats.variables;
                continue;
            }
            if (!env.has_var(name)) {
                env.set_var(name, value, true);
                ++stats.variables;
            }
            continue;
        }
        // VAR=val (bare assignment, no spaces around =)
        size_t eq = t.find('=');
        if (eq != std::string::npos && t.find(' ') == std::string::npos && t.find('\t') == std::string::npos) {
            std::string name = t.substr(0, eq);
            bool valid = !name.empty() && (std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_');
            for (size_t i = 1; valid && i < name.size(); ++i) {
                if (!std::isalnum(static_cast<unsigned char>(name[i])) && name[i] != '_') valid = false;
            }
            if (valid && !should_skip_var(name) && !env.has_var(name)) {
                env.set_var(name, expand_simple(bash_unquote(t.substr(eq + 1)), env), false);
                ++stats.variables;
            }
            continue;
        }
        // function start: "name() {", "name () {", "function name {"
        bool is_func = false;
        if (t.compare(0, 9, "function ") == 0) is_func = true;
        else if (t.find("()") != std::string::npos && t.find('=') == std::string::npos) is_func = true;
        if (is_func) {
            flush_pending();
            pending_func = line + "\n";
            if (t.find('{') != std::string::npos && t.find('}') != std::string::npos) {
                flush_pending(); // one-liner
            }
            continue;
        }
        if (!pending_func.empty()) {
            pending_func += line + "\n";
            std::string st = str_util::trim(line);
            if (st == "}") flush_pending();
        }
    }
    flush_pending();

    // Also pick up ~/.bash_aliases when importing ~/.bashrc directly.
    if (path.size() >= 7 && path.compare(path.size() - 7, 7, ".bashrc") == 0) {
        const char* home = std::getenv("HOME");
        std::string aliases = (home ? std::string(home) : "/root") + "/.bash_aliases";
        struct stat st{};
        if (stat(aliases.c_str(), &st) == 0) {
            BashImportStats sub = import_file(aliases, env, exec);
            stats.aliases += sub.aliases;
            stats.variables += sub.variables;
            stats.functions += sub.functions;
        }
    }
    return stats;
}

BashImportStats BashCompat::import_bashrc(Environment& env, Executor* exec, bool verbose) {
    // Opt-outs: explicit env var or missing file.
    const char* no_bash = std::getenv("ASWELL_NO_BASHRC");
    if (no_bash && std::string(no_bash) == "1") {
        BashImportStats s;
        s.skipped = true;
        return s;
    }
    std::string rc = default_bashrc_path();
    struct stat st{};
    bool has_rc = (stat(rc.c_str(), &st) == 0);

    if (bash_available()) {
        std::string dump = run_bash_dump();
        if (dump.find("__ASWELL_ALIAS_BEGIN__") != std::string::npos) {
            BashImportStats stats = import_dump(dump, env, exec, verbose);
            // Fallback supplement: direct parse of ~/.bash_aliases in case the
            // interactive dump missed it (e.g. guarded sourcing).
            const char* home = std::getenv("HOME");
            std::string aliases = (home ? std::string(home) : "/root") + "/.bash_aliases";
            struct stat ast{};
            if (stat(aliases.c_str(), &ast) == 0) {
                BashImportStats sub = import_file(aliases, env, exec);
                stats.aliases += sub.aliases;
            }
            return stats;
        }
    }
    if (!has_rc) {
        BashImportStats s;
        s.skipped = true;
        return s;
    }
    BashImportStats stats = import_file(rc, env, exec);
    if (verbose) {
        std::cerr << "aswell: imported " << stats.aliases << " aliases, " << stats.variables
                  << " variables, " << stats.functions << " functions from ~/.bashrc (direct parse)\n";
    }
    return stats;
}

} // namespace aswell
