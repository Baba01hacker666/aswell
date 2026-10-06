// `parallel` — run shell commands concurrently, safely, with readable output.
//
// Why this is a builtin: every job is executed by the shell itself, so a job can
// use pipelines, redirections, aliases, functions, globbing and variables
// exactly as typed at the prompt. External `xargs -P` can only exec argv.
//
// Grammar (documented in docs/CONFIGURATION.md and `aswell help`):
//   parallel [opts] 'cmd1' 'cmd2' ...   each argument is one job
//   parallel [opts] TEMPLATE ::: a b c  one job per item, {} replaced
//   parallel [opts] TEMPLATE            jobs read from stdin, one per line
#include "aswell/shell/builtins.hpp"
#include "aswell/shell/parallel.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/signals.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>

namespace aswell {
namespace {

struct ParallelOptions {
    size_t jobs = 0;             // 0 = auto (cpu count)
    bool keep_order = false;
    bool tag = false;
    bool dry_run = false;
    bool halt_on_error = false;
    bool verbose = false;
    bool null_delimited = false;
    long long timeout_ms = 0;    // 0 = no per-job timeout
    bool show_help = false;
};

struct ParallelTask {
    std::string label;           // command line as the user wrote it
    std::string command;         // command line handed to the shell
    pid_t pid = -1;
    int fd = -1;                 // read end of the job's output pipe
    std::string output;
    int status = 0;
    bool finished = false;
    bool failed = false;
    bool timed_out = false;
    struct timeval started {};
};

void usage() {
    std::cout <<
        "usage: parallel [options] <command>... \n"
        "       parallel [options] <command-template> ::: item...\n"
        "       <list> | parallel [options] <command-template>\n"
        "\n"
        "Runs shell commands concurrently. Each job is a full shell command line,\n"
        "so pipes, redirections, aliases and functions all work inside it.\n"
        "\n"
        "options:\n"
        "  -j, --jobs N        run at most N jobs at a time (default: all CPUs)\n"
        "  -k, --keep-order    print output in submission order instead of as it lands\n"
        "  -t, --tag           prefix every output line with [job] so it stays attributable\n"
        "  -e, --halt-on-error stop launching after the first failure\n"
        "  -T, --timeout SEC   kill a job that runs longer than SEC seconds (0 = never)\n"
        "  -0, --null          split stdin on NUL bytes instead of newlines\n"
        "  -n, --dry-run       print the commands, run none of them\n"
        "  -v, --verbose       report each job as it finishes\n"
        "\n"
        "examples:\n"
        "  parallel -j4 'make -C app' 'make -C docs' 'cargo test'   # three builds at once\n"
        "  parallel 'curl -sO {}' ::: *.tar.gz                       # download in parallel\n"
        "  ls -d */ | parallel 'git -C {} pull --ff-only'             # update every repo\n"
        "  parallel --dry-run 'ping -c1 {}' ::: host1 host2          # see what would run\n";
}

long long elapsed_ms(const struct timeval& from) {
    struct timeval now {};
    gettimeofday(&now, nullptr);
    return static_cast<long long>(now.tv_sec - from.tv_sec) * 1000 +
           static_cast<long long>(now.tv_usec - from.tv_usec) / 1000;
}

} // namespace

// `{}` (and `{.}` for the extension-less stem) substitution, shell-quoted.
std::string parallel_apply_template(const std::string& tmpl, const std::string& item) {
    std::string quoted = str_util::escape_shell(item);
    std::string stem = item;
    size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);

    if (tmpl.find("{}") == std::string::npos && tmpl.find("{.}") == std::string::npos) {
        // No placeholder: append the item as an extra argument (xargs behaviour).
        std::string out = tmpl;
        if (!out.empty() && out.back() != ' ') out += ' ';
        out += quoted;
        return out;
    }

    std::string out;
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}') {
            out += quoted;
            ++i;
            continue;
        }
        if (tmpl[i] == '{' && i + 2 < tmpl.size() && tmpl[i + 1] == '.' && tmpl[i + 2] == '}') {
            out += str_util::escape_shell(stem);
            i += 2;
            continue;
        }
        if (tmpl[i] == '\\' && i + 1 < tmpl.size() && tmpl[i + 1] == '{') {
            out += '{';   // \{} escapes a literal brace
            ++i;
            continue;
        }
        out += tmpl[i];
    }
    return out;
}

std::vector<std::string> parallel_split_lines(const std::string& data, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : data) {
        if (c == delim) {
            if (!str_util::trim(cur).empty()) out.push_back(str_util::trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!str_util::trim(cur).empty()) out.push_back(str_util::trim(cur));
    return out;
}

size_t parallel_default_jobs(long long configured) {
    if (configured > 0) return static_cast<size_t>(configured);
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 1;
    if (cpus > 64) cpus = 64;      // one shell per job is not free
    return static_cast<size_t>(cpus);
}

namespace {

// Hands the terminal to a process group this builtin just created, and always
// takes it back. A job that reads the keyboard while sitting in a different
// process group gets SIGTTIN and stops, which used to leave `timeout` and
// `parallel` waiting forever on a job nobody can resume.
class TerminalLoan {
public:
    explicit TerminalLoan(pid_t pgid) : active_(pgid > 0 && isatty(STDIN_FILENO)) {
        if (active_) tcsetpgrp(STDIN_FILENO, pgid);
    }
    ~TerminalLoan() {
        if (active_ && isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    TerminalLoan(const TerminalLoan&) = delete;
    TerminalLoan& operator=(const TerminalLoan&) = delete;

private:
    bool active_;
};

// Forwards a terminal interrupt to every live job group, and marks jobs that
// blew their timeout. Installed only while `parallel` is running.
std::vector<pid_t>* g_parallel_groups = nullptr;
std::function<void(const char*)> g_parallel_notify;
volatile sig_atomic_t g_parallel_interrupt = 0;

void parallel_signal(int signum) {
    g_parallel_interrupt = signum;
    if (g_parallel_groups) {
        for (pid_t pgid : *g_parallel_groups) {
            if (pgid > 0) kill(-pgid, SIGTERM);
        }
    }
}

class SignalForwarder {
public:
    explicit SignalForwarder(std::vector<pid_t>& groups) {
        g_parallel_groups = &groups;
        for (int sig : {SIGINT, SIGTERM, SIGHUP}) {
            struct sigaction old {};
            struct sigaction act {};
            act.sa_handler = parallel_signal;
            sigemptyset(&act.sa_mask);
            sigaction(sig, &act, &old);
            saved_[sig] = old;
            installed_.push_back(sig);
        }
    }
    ~SignalForwarder() {
        for (int sig : installed_) sigaction(sig, &saved_[sig], nullptr);
        g_parallel_groups = nullptr;
    }

    bool triggered() const { return g_parallel_interrupt != 0; }
    void clear() { g_parallel_interrupt = 0; }

private:
    std::vector<int> installed_;
    std::map<int, struct sigaction> saved_;
};

// Reads everything currently pending. The fd is O_NONBLOCK (see below), so this
// returns instead of stalling until a slow job happens to write.
void drain(ParallelTask& task) {
    char buf[8192];
    for (;;) {
        ssize_t n = read(task.fd, buf, sizeof(buf));
        if (n > 0) {
            task.output.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        break;   // EAGAIN (nothing pending) or EOF (job closed the pipe)
    }
}

void emit(const ParallelTask& task, const ParallelOptions& opts, size_t index) {
    if (task.output.empty() && !opts.tag) return;

    if (!opts.tag) {
        std::cout << task.output;
        std::cout.flush();
        return;
    }

    std::string prefix = "[" + std::to_string(index + 1) + "] ";
    size_t start = 0;
    while (start < task.output.size()) {
        size_t nl = task.output.find('\n', start);
        if (nl == std::string::npos) nl = task.output.size();
        std::cout << prefix << task.output.substr(start, nl - start) << "\n";
        start = nl + 1;
    }
    std::cout.flush();
}

} // namespace

int Builtins::builtin_parallel(const std::vector<std::string>& args, Environment& env, Executor& executor) {
    (void)executor;
    ParallelOptions opts;
    std::vector<std::string> positional;
    std::vector<std::string> items;
    bool seen_colons = false;

    // Short options may be clustered and their value may be attached, so
    // `-j4`, `-j 4`, `-knt` and `--jobs=4` all work.
    size_t i = 1;
    while (i < args.size()) {
        const std::string a = args[i];
        if (a == "--") {
            for (++i; i < args.size(); ++i) {
                if (seen_colons) items.push_back(args[i]); else positional.push_back(args[i]);
            }
            break;
        }
        if (a == ":::") { seen_colons = true; ++i; continue; }

        const bool longopt = str_util::starts_with(a, "--");
        const bool flag = !a.empty() && a[0] == '-' && a.size() > 1 && !longopt;
        if (!longopt && !flag) {
            if (seen_colons) items.push_back(a); else positional.push_back(a);
            ++i;
            continue;
        }

        // Index of the option letter inside `a`, so an attached value can be
        // sliced off the right place in a clustered short-option group.
        size_t letter_at = 1;

        auto value_for = [&](std::string& out) -> bool {
            size_t eq = a.find('=');
            if (eq != std::string::npos) { out = a.substr(eq + 1); return true; }
            // `-j4`: an attached value follows the option letter immediately.
            // `letter_at` is the index of that letter, so `-kj4` reads "4" and not
            // the rest of the cluster.
            if (!longopt && a.size() > letter_at + 1) { out = a.substr(letter_at + 1); return true; }
            if (i + 1 < args.size()) { out = args[++i]; return true; }
            std::cerr << "aswell: parallel: " << a << " needs a value\n";
            return false;
        };

        std::string val;
        if (longopt) {
            std::string name = a.substr(2);
            size_t eq = name.find('=');
            if (eq != std::string::npos) name = name.substr(0, eq);

            if (name == "help") { usage(); return 0; }
            else if (name == "jobs") {
                if (!value_for(val)) return 2;
                opts.jobs = static_cast<size_t>(std::strtoul(val.c_str(), nullptr, 10));
            }
            else if (name == "keep-order") opts.keep_order = true;
            else if (name == "tag") opts.tag = true;
            else if (name == "halt-on-error") opts.halt_on_error = true;
            else if (name == "dry-run") opts.dry_run = true;
            else if (name == "verbose") opts.verbose = true;
            else if (name == "null") opts.null_delimited = true;
            else if (name == "timeout") {
                if (!value_for(val)) return 2;
                double secs = std::strtod(val.c_str(), nullptr);
                opts.timeout_ms = secs > 0 ? static_cast<long long>(secs * 1000.0) : 0;
            }
            else {
                std::cerr << "aswell: parallel: unknown option " << a << " (see: parallel --help)\n";
                return 2;
            }
            ++i;
            continue;
        }

        // Clustered short options: walk the letters after the '-'.
        for (size_t k = 1; k < a.size(); ++k) {
            letter_at = k;
            const char c = a[k];
            if (c == 'h') { usage(); return 0; }
            else if (c == 'j') {
                if (!value_for(val)) return 2;
                opts.jobs = static_cast<size_t>(std::strtoul(val.c_str(), nullptr, 10));
                break;   // the value consumed the rest of the cluster
            }
            else if (c == 'T') {
                if (!value_for(val)) return 2;
                double secs = std::strtod(val.c_str(), nullptr);
                opts.timeout_ms = secs > 0 ? static_cast<long long>(secs * 1000.0) : 0;
                break;
            }
            else if (c == 'k') opts.keep_order = true;
            else if (c == 't') opts.tag = true;
            else if (c == 'e') opts.halt_on_error = true;
            else if (c == 'n') opts.dry_run = true;
            else if (c == 'v') opts.verbose = true;
            else if (c == '0') opts.null_delimited = true;
            else {
                std::cerr << "aswell: parallel: unknown option -" << c << " (see: parallel --help)\n";
                return 2;
            }
        }
        ++i;
    }

    if (opts.jobs == 0) {
        std::string configured = str_util::trim(env.get_var("ASWELL_PARALLEL_JOBS"));
        long long want = configured.empty() ? 0 : std::strtoll(configured.c_str(), nullptr, 10);
        opts.jobs = parallel_default_jobs(want);
    }

    // Build the job list: explicit commands, or a template applied to items.
    std::vector<std::string> commands;
    if (seen_colons) {
        if (positional.empty()) {
            std::cerr << "aswell: parallel: ':::' needs a command template before it\n";
            return 2;
        }
        std::string tmpl;
        for (const auto& w : positional) {
            if (!tmpl.empty()) tmpl += ' ';
            tmpl += w;
        }
        if (items.empty()) {
            std::cerr << "aswell: parallel: ':::' was given no items\n";
            return 2;
        }
        for (const auto& item : items) commands.push_back(parallel_apply_template(tmpl, item));
    } else if (positional.empty() && isatty(STDIN_FILENO)) {
        // Nothing to run and nothing piped in: without this the stdin read below
        // would sit on the terminal and happily execute the command line the user
        // types next.
        usage();
        return 2;
    } else if (!positional.empty() && isatty(STDIN_FILENO)) {
        // Nothing piped in: each argument is a job in its own right.
        commands = positional;
    } else {
        // Job list from stdin (`find … | parallel …`). When arguments are present
        // too they are not jobs but the template each stdin line is poured into,
        // which is how GNU parallel behaves: `parallel 'echo got {}' < items`.
        std::string tmpl;
        if (!positional.empty()) {
            for (const auto& w : positional) {
                if (!tmpl.empty()) tmpl += ' ';
                tmpl += w;
            }
        }
        const char delim = opts.null_delimited ? '\0' : '\n';
        std::string data;
        char buf[4096];
        ssize_t n = 0;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
            data.append(buf, static_cast<size_t>(n));
        std::vector<std::string> stdin_items = parallel_split_lines(data, delim);
        if (stdin_items.empty() && !positional.empty()) {
            commands = positional;   // an empty stdin just means "these are the jobs"
        } else if (stdin_items.empty()) {
            std::cerr << "aswell: parallel: no commands given (pass them as arguments or on stdin)\n";
            return 2;
        } else {
            if (tmpl.empty()) {
                // Bare `parallel < file`: every line is a command.
                commands = stdin_items;
            } else {
                for (const auto& item : stdin_items) commands.push_back(parallel_apply_template(tmpl, item));
            }
        }
    }

    if (commands.empty()) {
        std::cerr << "aswell: parallel: nothing to run\n";
        return 2;
    }

    if (opts.dry_run) {
        for (const auto& cmd : commands) std::cout << cmd << "\n";
        std::cout << "aswell: parallel: " << commands.size() << " job(s), "
                  << opts.jobs << " at a time\n";
        return 0;
    }

    std::vector<ParallelTask> tasks(commands.size());
    for (size_t slot = 0; slot < commands.size(); ++slot) {
        tasks[slot].label = commands[slot];
        tasks[slot].command = commands[slot];
    }

    std::vector<pid_t> groups;
    SignalForwarder forwarder(groups);
    // Several concurrent jobs cannot share one keyboard, so the terminal is only
    // handed to a job when this is a single-job run.
    std::unique_ptr<TerminalLoan> terminal;

    size_t next = 0;
    size_t running = 0;
    size_t reaped = 0;
    size_t print_cursor = 0;
    size_t failed = 0;
    bool aborted = false;
    std::map<size_t, bool> printed;

    auto reap_ready = [&](bool blocking) {
        int wstatus = 0;
        for (;;) {
            // Reap by pid, never with waitpid(-1): a bare -1 competes with the
            // shell's own job table and whichever loses permanently forgets the
            // exit status of a tracked background job.
            pid_t done = -1;
            for (auto& task : tasks) {
                if (task.finished || task.pid <= 0) continue;
                pid_t r = waitpid(task.pid, &wstatus, blocking ? 0 : WNOHANG);
                if (r == task.pid) { done = r; break; }
                if (r < 0 && errno != EINTR) { task.finished = true; break; }
            }
            if (done <= 0) return;
            for (auto& task : tasks) {
                if (task.pid != done || task.finished) continue;
                task.finished = true;
                task.status = wstatus;
                if (WIFEXITED(wstatus)) task.failed = WEXITSTATUS(wstatus) != 0;
                else if (WIFSIGNALED(wstatus)) task.failed = true;
                if (task.failed) failed++;
                running--;
                reaped++;
                break;
            }
        }
    };

    // Kills every launched job group, closes every capture pipe and reaps the
    // children, so a failed spawn cannot leak fds or leave orphans behind in a
    // long-lived shell.
    auto abandon = [&](std::vector<ParallelTask>& live) {
        for (auto& task : live) {
            if (task.pid > 0) kill(-task.pid, SIGKILL);
        }
        for (auto& task : live) {
            if (task.fd >= 0) { close(task.fd); task.fd = -1; }
        }
        for (auto& task : live) {
            if (task.pid <= 0) continue;
            int st = 0;
            while (waitpid(task.pid, &st, 0) < 0 && errno == EINTR) { /* retry */ }
            task.finished = true;
        }
    };

    while (reaped < tasks.size()) {
        if (forwarder.triggered()) {
            forwarder.clear();
            aborted = true;
            for (auto& task : tasks) {
                if (task.finished || task.pid <= 0) continue;
                kill(-task.pid, SIGKILL);
            }
        }

        // Launch while there is room.
        while (next < tasks.size() && running < opts.jobs && !(opts.halt_on_error && failed > 0) && !aborted) {
            ParallelTask& task = tasks[next];
            int pipefd[2];
            if (pipe(pipefd) != 0) {
                std::cerr << "aswell: parallel: pipe failed\n";
                abandon(tasks);
                return 1;
            }

            pid_t pid = fork();
            if (pid < 0) {
                std::cerr << "aswell: parallel: fork failed\n";
                close(pipefd[0]);
                close(pipefd[1]);
                abandon(tasks);
                return 1;
            }

            if (pid == 0) {
                close(pipefd[0]);
                setpgid(0, 0);
                dup2(pipefd[1], STDOUT_FILENO);
                dup2(pipefd[1], STDERR_FILENO);
                close(pipefd[1]);
                SignalManager::reset_signals_for_child();

                JobManager local_jobs;
                Environment local_env = env;
                Executor sub_exec(local_env, local_jobs);
                int rc = sub_exec.execute_string(task.command);
                std::cout.flush();
                _exit(rc & 0xff);
            }

            close(pipefd[1]);
            fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
            if (!terminal && commands.size() == 1) terminal = std::make_unique<TerminalLoan>(pid);
            task.pid = pid;
            task.fd = pipefd[0];
            groups.push_back(pid);
            gettimeofday(&task.started, nullptr);
            running++;
            next++;
            setpgid(pid, pid);   // in case the child exec'd before its own call
        }

        // Pump output so a chatty job can never block on a full pipe.
        std::vector<struct pollfd> pfds;
        for (auto& task : tasks) {
            if (task.finished || task.fd < 0) continue;
            struct pollfd p;
            p.fd = task.fd;
            p.events = POLLIN;
            p.revents = 0;
            pfds.push_back(p);
        }
        if (!pfds.empty()) poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 20);
        else if (running > 0) usleep(20 * 1000);

        for (auto& task : tasks) {
            if (task.finished || task.fd < 0) continue;
            drain(task);
            if (opts.timeout_ms > 0 && elapsed_ms(task.started) > opts.timeout_ms) {
                task.timed_out = true;
                kill(-task.pid, SIGKILL);
            }
        }

        // Collect finished jobs, then show whatever is next in order.
        reap_ready(false);
        for (auto& task : tasks) {
            if (!task.finished || task.fd < 0) continue;
            drain(task);
            close(task.fd);
            task.fd = -1;
        }

        if (opts.keep_order) {
            while (print_cursor < tasks.size() && tasks[print_cursor].finished && !printed[print_cursor]) {
                emit(tasks[print_cursor], opts, print_cursor);
                if (opts.verbose) {
                    std::cerr << "[" << (print_cursor + 1) << "/" << tasks.size() << "] "
                              << (tasks[print_cursor].timed_out ? "timeout " : "")
                              << (tasks[print_cursor].failed ? "failed" : "ok") << "  "
                              << tasks[print_cursor].label << "\n";
                }
                printed[print_cursor] = true;
                print_cursor++;
            }
        } else {
            for (size_t slot = 0; slot < tasks.size(); ++slot) {
                if (!tasks[slot].finished || printed[slot]) continue;
                bool out_of_order = (slot != print_cursor);
                emit(tasks[slot], opts, slot);
                if (opts.verbose) {
                    std::cerr << "[" << (slot + 1) << "/" << tasks.size() << "] "
                              << (tasks[slot].timed_out ? "timeout " : "")
                              << (tasks[slot].failed ? "failed" : "ok") << "  " << tasks[slot].label << "\n";
                }
                printed[slot] = true;
                if (!out_of_order) print_cursor = slot + 1;
            }
        }

        // Everything launched died and nothing is left to read: done.
        if (running == 0 && (next >= tasks.size() || aborted || (opts.halt_on_error && failed > 0))) {
            // Print whatever is still pending so its output is not lost.
            for (size_t slot = 0; slot < tasks.size(); ++slot) {
                if (!tasks[slot].finished || printed[slot]) continue;
                emit(tasks[slot], opts, slot);
                printed[slot] = true;
            }
            break;
        }
    }

    // Drain stragglers (jobs that finished while we were printing).
    reap_ready(false);
    reap_ready(true);
    for (size_t slot = 0; slot < tasks.size(); ++slot) {
        ParallelTask& task = tasks[slot];
        if (task.fd < 0) continue;
        drain(task);
        if (!printed[slot]) emit(task, opts, slot);
        close(task.fd);
        task.fd = -1;
    }
    while (waitpid(-1, nullptr, WNOHANG) > 0) { /* nothing of ours left */ }

    if (aborted) {
        std::cerr << "aswell: parallel: interrupted — " << reaped << " of " << tasks.size()
                  << " job(s) completed\n";
        return 130;
    }
    if (opts.halt_on_error && failed > 0 && next < tasks.size()) {
        std::cerr << "aswell: parallel: stopped early after a failure (" << (tasks.size() - next)
                  << " job(s) not started)\n";
    }
    if (failed > 0) {
        std::cerr << "aswell: parallel: " << failed << " of " << tasks.size() << " job(s) failed\n";
        return 1;
    }
    if (opts.verbose) {
        std::cerr << "aswell: parallel: all " << tasks.size() << " job(s) succeeded\n";
    }
    (void)executor;
    return 0;
}

// ---------------------------------------------------------------------------
// `retry` — the other half of the story: keep trying a flaky command.
// ---------------------------------------------------------------------------

void retry_usage() {
    std::cout <<
        "usage: retry [options] -- <command>\n"
        "\n"
        "Runs a shell command until it succeeds, then returns its status. Each\n"
        "attempt runs in a subshell, so state changes inside it do not leak.\n"
        "\n"
        "options:\n"
        "  -n, --tries N       attempts before giving up (default 3, 0 = forever)\n"
        "  -d, --delay SEC     wait this long before the second attempt (default 0.5)\n"
        "  -x, --max-delay SEC upper bound for the backoff (default 30)\n"
        "  -b, --backoff F     multiply the delay by F after each failure (default 2)\n"
        "  -q, --quiet         only report the final result\n"
        "  -s, --status CODE   treat CODE (comma list) as success instead of 0\n"
        "\n"
        "examples:\n"
        "  retry -- curl -fsS https://example.com            # wait for a server\n"
        "  retry -n 0 -d 2 -- make deploy                    # keep going forever\n"
        "  retry -n 5 -d 0.2 -s 4,7 -- flaky-test            # accept 4 or 7 as ok\n";
}

std::vector<int> parse_status_list(const std::string& text) {
    std::vector<int> out;
    for (const auto& part : str_util::split(text, ',')) {
        std::string trimmed = str_util::trim(part);
        if (trimmed.empty()) continue;
        try {
            out.push_back(std::stoi(trimmed));
        } catch (...) {
            // Ignore junk rather than failing the whole command.
        }
    }
    return out;
}

int Builtins::builtin_retry(const std::vector<std::string>& args, Environment& env, Executor& executor) {
    (void)executor;   // jobs run in their own subshell, see below
    int tries = 3;
    double delay = 0.5;
    double max_delay = 30.0;
    double backoff = 2.0;
    bool quiet = false;
    std::vector<int> accepted = {0};
    std::string command;

    size_t i = 1;
    while (i < args.size()) {
        const std::string a = args[i];
        if (a == "--") { ++i; break; }
        if (!(a.size() > 1 && a[0] == '-')) break;   // the command starts here

        auto take_value = [&](std::string& out) -> bool {
            size_t eq = a.find('=');
            if (eq != std::string::npos) { out = a.substr(eq + 1); return true; }
            if (a.size() > 2 && a[1] != '-') { out = a.substr(2); return true; }  // -n5
            if (i + 1 < args.size()) { out = args[++i]; return true; }
            std::cerr << "aswell: retry: " << a << " needs a value\n";
            return false;
        };

        std::string val;
        bool ok = true;
        // `-n5` is the same option as `-n 5`: match on the letter, not the whole
        // word, so the attached-value form is not reported as unknown.
        const char letter = (a.size() > 1 && a[1] != '-') ? a[1] : '\0';
        if (a == "-h" || a == "--help") { retry_usage(); return 0; }
        else if (a == "-n" || letter == 'n' || str_util::starts_with(a, "--tries")) { ok = take_value(val); if (ok) tries = std::atoi(val.c_str()); }
        else if (a == "-d" || letter == 'd' || str_util::starts_with(a, "--delay")) { ok = take_value(val); if (ok) delay = std::atof(val.c_str()); }
        else if (a == "-x" || letter == 'x' || str_util::starts_with(a, "--max-delay")) { ok = take_value(val); if (ok) max_delay = std::atof(val.c_str()); }
        else if (a == "-b" || letter == 'b' || str_util::starts_with(a, "--backoff")) { ok = take_value(val); if (ok) backoff = std::atof(val.c_str()); }
        else if (a == "-s" || letter == 's' || str_util::starts_with(a, "--status")) {
            ok = take_value(val);
            if (ok) {
                std::vector<int> parsed = parse_status_list(val);
                if (!parsed.empty()) accepted = parsed;
            }
        }
        else if (a == "-q" || a == "--quiet") quiet = true;
        else {
            std::cerr << "aswell: retry: unknown option " << a << " (see: retry --help)\n";
            return 2;
        }
        if (!ok) return 2;
        ++i;
    }

    // Rebuild the command the same way `timeout` does: several words are argv and
    // must survive re-parsing (a script full of `;` and `&&` would otherwise be
    // split at the wrong level), a single word is shell code and runs verbatim.
    std::vector<std::string> words;
    for (size_t j = i; j < args.size(); ++j) {
        if (args[j] == "--" && words.empty()) continue;   // `retry -n 3 -- cmd`
        words.push_back(args[j]);
    }
    if (words.size() == 1) {
        command = words[0];
    } else {
        for (size_t j = 0; j < words.size(); ++j) {
            if (j != 0) command += ' ';
            command += str_util::escape_shell(words[j]);
        }
    }
    if (command.empty()) {
        std::cerr << "aswell: retry: no command given (try: retry -- <command>)\n";
        return 2;
    }
    if (tries < 0) tries = 0;
    if (delay < 0) delay = 0;
    if (max_delay < delay) max_delay = delay;
    if (backoff < 1.0) backoff = 1.0;

    int status = 0;
    double current_delay = delay;
    for (int attempt = 1; tries == 0 || attempt <= tries; ++attempt) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "aswell: retry: fork failed\n";
            return 1;
        }
        if (pid == 0) {
            SignalManager::reset_signals_for_child();
            setpgid(0, 0);
            JobManager local_jobs;
            Environment local_env = env;
            Executor sub_exec(local_env, local_jobs);
            int rc = sub_exec.execute_string(command);
            std::cout.flush();
            _exit(rc & 0xff);
        }

        setpgid(pid, pid);   // in case the child exec'd before its own call
        int raw = 0;
        pid_t reaped;
        while ((reaped = waitpid(pid, &raw, 0)) < 0 && errno == EINTR) { /* retry */ }
        if (reaped < 0) {
            status = 1;
        } else if (WIFEXITED(raw)) {
            status = WEXITSTATUS(raw);
        } else if (WIFSIGNALED(raw)) {
            status = 128 + WTERMSIG(raw);
            if (WTERMSIG(raw) == SIGINT) {
                std::cerr << "aswell: retry: interrupted\n";
                return 130;
            }
        } else {
            status = 1;
        }

        bool ok = false;
        for (int want : accepted) {
            if (status == want) ok = true;
        }
        if (ok) {
            if (!quiet && attempt > 1) {
                std::cerr << "aswell: retry: attempt " << attempt << " succeeded\n";
            }
            return 0;
        }

        bool last = (tries > 0 && attempt >= tries);
        if (!quiet) {
            std::cerr << "aswell: retry: attempt " << attempt << "/"
                      << (tries > 0 ? std::to_string(tries) : std::string("∞"))
                      << " failed (status " << status << ")"
                      << (last ? std::string() : " — retrying in " + std::to_string(current_delay).substr(0, 4) + "s")
                      << "\n";
        }
        if (last) break;

        struct timespec ts {};
        ts.tv_sec = static_cast<time_t>(current_delay);
        ts.tv_nsec = static_cast<long>((current_delay - static_cast<double>(ts.tv_sec)) * 1e9);
        nanosleep(&ts, nullptr);
        current_delay *= backoff;
        if (current_delay > max_delay) current_delay = max_delay;
    }

    return status == 0 ? 1 : status;
}


// ---------------------------------------------------------------------------
// timeout — give a command a deadline
// ---------------------------------------------------------------------------
namespace {

void timeout_usage() {
    std::cout << "usage: timeout [OPTS] SECONDS -- COMMAND...\n"
              << "       timeout [OPTS] SECONDS COMMAND...\n"
              << "\n"
              << "  Runs COMMAND, then sends it a signal if it is still alive after SECONDS.\n"
              << "  Several arguments are exec'ed directly (like coreutils); a single\n"
              << "  quoted argument is run as shell code, so aliases, functions, pipelines\n"
              << "  and redirections work: timeout 5 -- 'make -j8 | tail -1'.\n"
              << "  SECONDS accepts s/m/h/d suffixes.\n"
              << "\n"
              << "  -k, --kill-after SECS   SIGKILL if it ignores SIGTERM after this long\n"
              << "  -s, --signal SIG        signal to send first (default TERM)\n"
              << "  -p, --preserve-status   exit with the command's status, not 124\n"
              << "      --foreground        keep it in this shell's process group\n"
              << "  -v, --verbose           report what was sent and when\n"
              << "\n"
              << "Exit status (matching coreutils): 124 when the deadline was reached, 137\n"
              << "when SIGKILL was involved, 128+N if -s picked another signal, 125 for a\n"
              << "timeout problem, and the command's own status when it finished in time.\n"
              << "\n"
              << "Examples:\n"
              << "  timeout 30 -- make -j8              # never wait longer than half a minute\n"
              << "  timeout 5s -- sleep 60              # deadline reached -> status 124\n"
              << "  timeout -k 2 -s INT 10s -- ping host\n";
}

// "90s", "10m", "1.5", "2h" -> seconds. Negative / unparsable -> -1.
double parse_duration(const std::string& text) {
    if (text.empty()) return -1.0;
    char* end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (!end || value < 0.0) return -1.0;
    double scale = 1.0;
    while (*end == ' ') end++;
    switch (*end) {
        case '\0': break;
        case 's': case 'S': break;
        case 'm': case 'M': scale = 60.0; break;
        case 'h': case 'H': scale = 3600.0; break;
        case 'd': case 'D': scale = 86400.0; break;
        default: return -1.0;
    }
    return value * scale;
}

} // namespace

int Builtins::builtin_timeout(const std::vector<std::string>& args, Environment& env,
                              Executor& executor) {
    (void)executor;   // the job runs in a forked shell, so it inherits everything anyway
    double limit = -1.0;
    double kill_after = -1.0;
    int signal_to_send = SIGTERM;
    bool preserve = false;
    bool foreground = false;
    bool verbose = false;

    // Options may appear before or after the duration (`timeout -k 1 5 cmd` and
    // `timeout 5 -k 1 cmd` both work); the first token that is neither an option
    // nor the duration starts the command.
    size_t i = 1;
    size_t cmd_start = 0;
    bool have_limit = false;
    while (i < args.size()) {
        const std::string a = args[i];
        if (a == "--") {
            cmd_start = i + 1;
            break;
        }
        if (a.size() > 1 && a[0] == '-') {
            auto take = [&](std::string& out) -> bool {
                size_t eq = a.find('=');
                if (eq != std::string::npos) { out = a.substr(eq + 1); return true; }
                if (a.size() > 2 && a[1] != '-') { out = a.substr(2); return true; }   // -k1
                if (i + 1 < args.size()) { out = args[++i]; return true; }
                return false;
            };
            std::string val;
            const char letter = (a.size() > 1 && a[1] != '-') ? a[1] : '\0';
            if (a == "-h" || a == "--help") { timeout_usage(); return 0; }
            if (a == "-k" || letter == 'k' || str_util::starts_with(a, "--kill-after")) {
                if (!take(val) || (kill_after = parse_duration(val)) < 0.0) {
                    std::cerr << "aswell: timeout: " << a << " needs a duration\n";
                    return 125;
                }
                ++i;
                continue;
            }
            if (a == "-s" || letter == 's' || str_util::starts_with(a, "--signal")) {
                if (!take(val)) {
                    std::cerr << "aswell: timeout: " << a << " needs a signal name\n";
                    return 125;
                }
                signal_to_send = SignalManager::signame_to_num(val);
                if (signal_to_send <= 0) {
                    std::cerr << "aswell: timeout: unknown signal '" << val << "'\n";
                    return 125;
                }
                ++i;
                continue;
            }
            if (a == "-p" || a == "--preserve-status") { preserve = true; ++i; continue; }
            if (a == "--foreground") { foreground = true; ++i; continue; }
            if (a == "-v" || a == "--verbose") { verbose = true; ++i; continue; }
            std::cerr << "aswell: timeout: unknown option '" << a << "' (see: timeout --help)\n";
            return 125;
        }
        if (!have_limit) {
            limit = parse_duration(a);
            if (limit < 0.0) {
                std::cerr << "aswell: timeout: invalid duration '" << a << "'\n";
                return 125;
            }
            have_limit = true;
            ++i;
            continue;
        }
        cmd_start = i;
        break;
    }
    if (!have_limit) {
        timeout_usage();
        return 125;
    }
    if (cmd_start == 0 || cmd_start >= args.size()) {
        std::cerr << "aswell: timeout: no command given\n";
        return 125;
    }

    std::vector<std::string> words;
    for (size_t j = cmd_start; j < args.size(); ++j) words.push_back(args[j]);
    if (words.empty()) {
        std::cerr << "aswell: timeout: no command given\n";
        return 125;
    }

    // Fast path: a plain external command with no shell syntax is exec'ed
    // directly, exactly like coreutils `timeout`. That matters for signal
    // delivery — the deadline signal then hits the command itself instead of a
    // shell that is only standing between us and it.
    static const char* const kShellMeta = ";|&<>(){}*?$!`'\"\\";
    auto shell_meta = [](const std::string& text) {
        return text.find_first_of(kShellMeta) != std::string::npos;
    };
    // A single argument is shell code (`timeout 5 -- 'make -j8 | tail -1'`), so it
    // goes through the shell; several arguments are argv and can be exec'ed.
    bool direct = words.size() >= 2 && (!shell_meta(words[0]) || words[0].find('/') != std::string::npos);
    for (const auto& w : words) {
        if (shell_meta(w)) direct = false;
    }
    std::string program;
    if (direct) {
        std::string alias_target;
        if (Builtins::is_builtin(words[0]) || env.has_function(words[0]) ||
            env.get_alias(words[0], alias_target)) {
            direct = false;
        } else {
            program = env.find_in_path(words[0]);
            if (program.empty()) program = words[0];   // let execv report ENOENT
        }
    }

    // Shell path: re-quote the words so the shell splits them again, or run a
    // single argument verbatim as shell code.
    std::string command;
    if (!direct) {
        if (words.size() == 1) {
            command = words[0];
        } else {
            for (size_t j = 0; j < words.size(); ++j) {
                if (j != 0) command += ' ';
                command += str_util::escape_shell(words[j]);
            }
        }
    }

    const auto started = std::chrono::steady_clock::now();
    pid_t pgid = 0;

    fflush(nullptr);
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "aswell: timeout: fork failed: " << std::strerror(errno) << "\n";
        return 125;
    }
    if (pid == 0) {
        SignalManager::reset_signals_for_child();
        if (!foreground) {
            setpgid(0, 0);
            if (!direct) {
                // This helper only exists to host the command, so it must outlive
                // the group signal meant for the command. The ignores therefore go
                // *after* reset_signals_for_child(): the command this helper runs
                // gets the default dispositions back and stays killable.
                struct sigaction ignore {};
                ignore.sa_handler = SIG_IGN;
                sigemptyset(&ignore.sa_mask);
                for (int sig = 1; sig < NSIG; ++sig) sigaction(sig, &ignore, nullptr);
            }
        }
        if (direct) {
            std::vector<char*> argv;
            for (auto& w : words) argv.push_back(const_cast<char*>(w.c_str()));
            argv.push_back(nullptr);
            execv(program.c_str(), argv.data());
            _exit(127);
        }
        JobManager child_jobs;
        Environment child_env = env;
        Executor child(child_env, child_jobs);
        int rc = child.execute_string(command);
        _exit(rc & 0xff);
    }
    pgid = pid;
    if (!foreground) setpgid(pid, pid);   // also covers the child racing ahead
    TerminalLoan terminal(pgid);

    int status = 0;
    bool timed_out = false;
    bool killed_hard = false;
    for (;;) {
        pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) break;
        if (done < 0 && errno != EINTR) {
            std::cerr << "aswell: timeout: wait failed: " << std::strerror(errno) << "\n";
            return 125;
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (!timed_out && elapsed >= limit) {
            timed_out = true;
            if (verbose) {
                std::cerr << "aswell: timeout: sending SIG"
                          << SignalManager::signum_to_name(signal_to_send) << " to "
                          << (foreground ? std::to_string(pid) : "the job's process group") << "\n";
            }
            if (foreground) kill(pid, signal_to_send);
            else kill(-pgid, signal_to_send);
            continue;
        }
        if (timed_out && !killed_hard && kill_after >= 0.0 && elapsed - limit >= kill_after) {
            killed_hard = true;
            if (verbose) std::cerr << "aswell: timeout: sending SIGKILL\n";
            if (foreground) kill(pid, SIGKILL);
            else kill(-pgid, SIGKILL);
            continue;
        }
        struct pollfd pfd {};
        pfd.fd = -1;
        pfd.events = 0;
        poll(&pfd, 0, 20);
    }

    const int code = WIFSIGNALED(status) ? 128 + WTERMSIG(status) : WEXITSTATUS(status);
    if (timed_out && !preserve) {
        // Coreutils' contract: the deadline owns the exit status. 124 for a plain
        // timeout, 137 once SIGKILL was involved, 128+sig for any other signal the
        // user asked for with -s (a command that exits non-zero *because* it was
        // interrupted still reports the timeout, like `timeout` always has).
        std::cerr << "aswell: timeout: command exceeded " << limit << "s\n";
        if (killed_hard) return 137;
        if (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) return 128 + WTERMSIG(status);
        return 124;
    }
    return code;
}

} // namespace aswell
