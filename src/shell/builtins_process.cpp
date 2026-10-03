#include "aswell/shell/builtins.hpp"
#include "aswell/shell/signals.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <iomanip>
#include <cstring>
#include <cerrno>

namespace aswell {

// wait [-n] [job|pid...]
//   no args : wait for every tracked job, exit 0
//   -n      : wait for the next job to finish and return *its* status
//   ids     : wait for those jobs/pids; returns the last status seen
int Builtins::builtin_wait(const std::vector<std::string>& args, Environment& /*env*/, JobManager& jobs) {
    bool next_only = false;
    std::vector<std::string> targets;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--") {
            for (size_t j = i + 1; j < args.size(); ++j) targets.push_back(args[j]);
            break;
        }
        if (a == "-n" || a == "--one") {
            next_only = true;
            continue;
        }
        if (a == "-f" || a == "--all") continue;  // accepted for POSIX-style scripts
        if (!a.empty() && a[0] == '-' && a != "-") {
            std::cerr << "aswell: wait: invalid option — " << a << " (try: wait [-n] [job...])\n";
            return 2;
        }
        targets.push_back(a);
    }

    int status = 0;

    if (next_only) {
        if (targets.empty()) {
            int r = jobs.wait_for_any();
            return r < 0 ? 0 : r;
        }
        // `wait -n %1 %2` — first of the listed jobs to finish.
        for (const auto& spec : targets) {
            Job* job = jobs.resolve(spec);
            if (!job) continue;
            return jobs.wait_for_job(job->id);
        }
        return 0;
    }

    if (targets.empty()) {
        for (int id : jobs.job_ids()) {
            status = jobs.wait_for_job(id);
        }
        return 0;
    }

    for (const auto& spec : targets) {
        Job* job = jobs.resolve(spec);
        if (job) {
            status = jobs.wait_for_job(job->id);
            continue;
        }
        // Bare numbers are also allowed to be plain pids that were never a job.
        bool numeric = !spec.empty();
        for (char c : spec) {
            if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
        }
        if (!numeric) {
            std::cerr << "aswell: wait: " << spec << ": not a job\n";
            status = 127;
            continue;
        }
        pid_t pid = 0;
        try { pid = std::stoi(spec); } catch (...) { status = 127; continue; }
        int raw = 0;
        if (waitpid(pid, &raw, 0) <= 0) {
            // Not our child (or already reaped): POSIX reports 127.
            status = 127;
            continue;
        }
        if (WIFEXITED(raw)) status = WEXITSTATUS(raw);
        else if (WIFSIGNALED(raw)) status = 128 + WTERMSIG(raw);
    }

    return status;
}

int Builtins::builtin_jobs(const std::vector<std::string>& args, JobManager& jobs) {
    bool verbose = false;
    bool pids_only = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-l" || a == "--verbose") verbose = true;
        else if (a == "-p") pids_only = true;
        else if (a == "-n") continue;  // "only running" — we never list finished jobs
        else if (a == "-pn" || a == "-np") { verbose = true; pids_only = true; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "aswell: jobs: invalid option — " << a << " (use -l or -p)\n";
            return 2;
        }
    }

    if (pids_only) {
        for (int id : jobs.job_ids()) {
            Job* job = jobs.get_job(id);
            if (!job) continue;
            for (const auto& proc : job->processes) {
                std::cout << proc.pid << "\n";
            }
        }
        return 0;
    }

    jobs.list_jobs(verbose);
    return 0;
}

// Shared by fg and bg: accept `fg %1`, `fg`, `fg 1` and `fg %?make`.
static int job_spec_to_id(JobManager& jobs, const std::vector<std::string>& args,
                          const char* who, int& out_id) {
    if (args.size() <= 1) {
        Job* job = jobs.get_current_job();
        if (!job) {
            std::cerr << "aswell: " << who << ": no current job\n";
            return 1;
        }
        out_id = job->id;
        return 0;
    }

    Job* job = jobs.resolve(args[1]);
    if (!job) {
        std::cerr << "aswell: " << who << ": no such job: " << args[1] << "\n";
        return 1;
    }
    out_id = job->id;
    return 0;
}

int Builtins::builtin_fg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 0;
    int bad = job_spec_to_id(jobs, args, "fg", id);
    if (bad != 0) return bad;
    jobs.put_job_in_foreground(id, true);
    return 0;
}

int Builtins::builtin_bg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 0;
    int bad = job_spec_to_id(jobs, args, "bg", id);
    if (bad != 0) return bad;
    jobs.put_job_in_background(id, true);
    return 0;
}

int Builtins::builtin_kill(const std::vector<std::string>& args, Environment& /*env*/, JobManager& jobs) {
    if (args.size() <= 1) {
        std::cerr << "aswell: kill: usage: kill [-sig] pid|job...\n";
        return 1;
    }

    int sig = SIGTERM;
    bool group = false;
    size_t idx = 1;

    while (idx < args.size() && !args[idx].empty() && args[idx][0] == '-') {
        const std::string& flag = args[idx];
        if (flag == "--") { idx++; break; }
        if (flag == "-l" || flag == "--list") {
            for (int s = 1; s <= 31; ++s) {
                std::cout << std::setw(2) << s << " " << SignalManager::signum_to_name(s);
                std::cout << ((s % 6) == 0 ? "\n" : "\t");
            }
            std::cout << "\n";
            return 0;
        }
        if (flag == "-s" && idx + 1 < args.size()) {
            int parsed = SignalManager::signame_to_num(args[idx + 1]);
            if (parsed < 0) {
                std::cerr << "aswell: kill: unknown signal: " << args[idx + 1] << "\n";
                return 1;
            }
            sig = parsed;
            idx += 2;
            continue;
        }
        if (flag == "-g") { group = true; idx++; continue; }
        int parsed = SignalManager::signame_to_num(flag.substr(1));
        if (parsed >= 0) {
            sig = parsed;
            idx++;
        } else {
            std::cerr << "aswell: kill: unknown signal: " << flag << "\n";
            return 1;
        }
    }

    if (idx >= args.size()) {
        std::cerr << "aswell: kill: usage: kill [-sig] pid|job...\n";
        return 1;
    }

    int failures = 0;
    for (size_t i = idx; i < args.size(); ++i) {
        const std::string& target = args[i];

        // Negative pid (`kill -- -1234`) and `%job` both address a group.
        bool is_job = !target.empty() && target[0] == '%';
        bool negative_pid = false;
        pid_t pid = 0;

        if (is_job) {
            Job* job = jobs.resolve(target);
            if (!job) {
                std::cerr << "aswell: kill: no such job: " << target << "\n";
                failures++;
                continue;
            }
            pid = job->pgid;
            negative_pid = true;  // signal the whole job, not just its leader
        } else {
            try {
                pid = std::stoi(target);
            } catch (...) {
                std::cerr << "aswell: kill: illegal pid: " << target << "\n";
                failures++;
                continue;
            }
            negative_pid = pid < 0;
        }

        if (group) negative_pid = true;
        int rc = negative_pid ? kill(pid, sig) : kill(pid, sig);
        if (rc != 0) {
            std::cerr << "aswell: kill: " << target << ": " << strerror(errno) << "\n";
            failures++;
        }
    }

    return failures ? 1 : 0;
}

} // namespace aswell
