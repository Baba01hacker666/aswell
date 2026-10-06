#include "aswell/shell/jobs.hpp"
#include "aswell/shell/signals.hpp"
#include <iomanip>

namespace aswell {

JobManager::JobManager() {
    shell_pgid_ = getpgrp();
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &shell_tmodes_);
    }
}

JobManager::~JobManager() = default;

void JobManager::set_interactive(bool interactive) {
    is_interactive_ = interactive;
    if (is_interactive_ && isatty(STDIN_FILENO)) {
        shell_pgid_ = getpid();
        setpgid(shell_pgid_, shell_pgid_);
        tcsetpgrp(STDIN_FILENO, shell_pgid_);
        tcgetattr(STDIN_FILENO, &shell_tmodes_);
    }
}

int JobManager::add_job(pid_t pgid, const std::string& cmd_line, const std::vector<pid_t>& pids) {
    Job job;
    job.id = next_job_id_++;
    job.pgid = pgid;
    job.command_line = cmd_line;
    job.state = JobState::RUNNING;
    job.exit_status = 0;
    job.notified = false;

    for (pid_t pid : pids) {
        Process proc;
        proc.pid = pid;
        proc.command = cmd_line;
        proc.completed = false;
        proc.stopped = false;
        proc.status = 0;
        job.processes.push_back(proc);
    }

    // Snapshot the terminal modes so `fg` can hand them back. Without this,
    // fg would tcsetattr() a struct termios that was never initialised.
    job.tmodes = shell_tmodes_;
    if (isatty(STDIN_FILENO)) {
        struct termios live {};
        if (tcgetattr(STDIN_FILENO, &live) == 0) job.tmodes = live;
    }

    jobs_[job.id] = job;
    return job.id;
}

Job* JobManager::get_job(int id) {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        return &it->second;
    }
    return nullptr;
}

Job* JobManager::get_job_by_pgid(pid_t pgid) {
    for (auto& [id, job] : jobs_) {
        if (job.pgid == pgid) {
            return &job;
        }
    }
    return nullptr;
}

Job* JobManager::get_job_by_pid(pid_t pid) {
    for (auto& [id, job] : jobs_) {
        for (const auto& proc : job.processes) {
            if (proc.pid == pid) {
                return &job;
            }
        }
    }
    return nullptr;
}

Job* JobManager::get_current_job() {
    // Bash convention: `+` is the newest job that is still running or stopped;
    // if every job has finished, it is simply the newest one.
    Job* best = nullptr;
    for (auto& [id, job] : jobs_) {
        if (job.state != JobState::DONE) best = &job;
    }
    if (!best && !jobs_.empty()) best = &jobs_.rbegin()->second;
    return best;
}

void JobManager::remove_job(int id) {
    jobs_.erase(id);
}

std::string JobManager::state_label(const Job& job) {
    if (job.state == JobState::RUNNING) return "Running";
    if (job.state == JobState::STOPPED) return "Stopped";

    // Finished: report the signal name when killed, the code otherwise.
    for (const auto& proc : job.processes) {
        if (WIFSIGNALED(proc.status)) {
            return SignalManager::signum_to_name(WTERMSIG(proc.status));
        }
    }
    if (job.exit_status == 0) return "Done";
    return "Exit " + std::to_string(job.exit_status);
}

bool JobManager::update_status() {
    int status = 0;
    bool changed = false;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto& [id, job] : jobs_) {
            bool touched = false;
            for (auto& proc : job.processes) {
                if (proc.pid != pid) continue;
                proc.status = status;
                touched = true;
                if (WIFSTOPPED(status)) {
                    proc.stopped = true;
                } else if (WIFCONTINUED(status)) {
                    proc.stopped = false;
                } else {
                    proc.completed = true;
                }
                break;
            }
            if (!touched) continue;

            bool all_completed = true;
            bool any_stopped = false;
            for (const auto& proc : job.processes) {
                if (!proc.completed) all_completed = false;
                if (proc.stopped) any_stopped = true;
            }

            JobState next = JobState::RUNNING;
            if (all_completed) next = JobState::DONE;
            else if (any_stopped) next = JobState::STOPPED;

            if (next != job.state) changed = true;
            job.state = next;

            if (next == JobState::DONE && !job.processes.empty()) {
                // A job's status is the status of its last process, which is
                // what `$?` and the notification line report.
                const Process& last = job.processes.back();
                if (WIFEXITED(last.status)) {
                    job.exit_status = WEXITSTATUS(last.status);
                } else if (WIFSIGNALED(last.status)) {
                    job.exit_status = 128 + WTERMSIG(last.status);
                } else {
                    job.exit_status = 0;
                }
                job.notified = false;
            }
        }
        changed = true;
    }

    return changed;
}

int JobManager::notify_finished() {
    update_status();

    int printed = 0;
    std::vector<int> forget;

    for (auto& [id, job] : jobs_) {
        if (job.state == JobState::RUNNING || job.notified) continue;
        // Only say something when the user asked for it (interactive shells get
        // the notification; scripts must not see surprise output).
        if (!is_interactive_) {
            job.notified = true;
            if (job.state == JobState::DONE) forget.push_back(id);
            continue;
        }

        const bool current = (get_current_job() == &job);
        std::cout << "[" << job.id << (current ? "+" : "-") << "]  "
                  << std::left << std::setw(18) << state_label(job) << job.command_line << "\n";
        job.notified = true;
        printed++;
        if (job.state == JobState::DONE) forget.push_back(id);
    }

    for (int id : forget) remove_job(id);
    return printed;
}

int JobManager::wait_for_job(int id) {
    Job* job = get_job(id);
    if (!job) return 0;

    while (job->state == JobState::RUNNING) {
        bool progressed = false;
        for (auto& proc : job->processes) {
            if (proc.completed) continue;
            int status = 0;
            pid_t r = waitpid(proc.pid, &status, WUNTRACED);
            if (r > 0) {
                proc.status = status;
                if (WIFSTOPPED(status)) {
                    proc.stopped = true;
                } else {
                    proc.completed = true;
                    proc.stopped = false;
                }
                progressed = true;
            } else if (r < 0 && errno == EINTR) {
                // A SIGCHLD from an unrelated child must not be mistaken for
                // "this job is gone"; keep waiting.
                continue;
            } else if (r < 0) {
                // Already reaped (e.g. by the poll before the prompt): trust the
                // bookkeeping instead of blocking forever.
                proc.completed = true;
                progressed = true;
            }
        }

        bool all_completed = true;
        bool any_stopped = false;
        for (const auto& proc : job->processes) {
            if (!proc.completed) all_completed = false;
            if (proc.stopped) any_stopped = true;
        }

        if (all_completed) {
            job->state = JobState::DONE;
            if (!job->processes.empty()) {
                const Process& last = job->processes.back();
                if (WIFEXITED(last.status)) job->exit_status = WEXITSTATUS(last.status);
                else if (WIFSIGNALED(last.status)) job->exit_status = 128 + WTERMSIG(last.status);
            }
            break;
        }
        if (any_stopped) {
            job->state = JobState::STOPPED;
            break;
        }
        if (!progressed) break;
    }

    if (is_interactive_ && isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pgid_);
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes_);
    }

    job->notified = true;
    int exit_status = job->exit_status;
    if (job->state == JobState::DONE) remove_job(id);
    return exit_status;
}

int JobManager::wait_for_any() {
    update_status();
    for (auto& [id, job] : jobs_) {
        if (job.state != JobState::RUNNING) return wait_for_job(id);
    }
    if (jobs_.empty()) return -1;

    // Block until one of our tracked children changes state. Children that do
    // not belong to a job (none in practice, since foreground commands reap their
    // own children) are ignored and we keep waiting.
    while (true) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
        if (pid < 0 && errno == EINTR) continue;
        if (pid <= 0) return -1;

        Job* owner = get_job_by_pid(pid);
        if (!owner) continue;

        for (auto& proc : owner->processes) {
            if (proc.pid != pid) continue;
            proc.status = status;
            if (WIFSTOPPED(status)) proc.stopped = true;
            else if (WIFCONTINUED(status)) proc.stopped = false;
            else proc.completed = true;
        }

        bool all_completed = true;
        bool any_stopped = false;
        for (const auto& proc : owner->processes) {
            if (!proc.completed) all_completed = false;
            if (proc.stopped) any_stopped = true;
        }
        if (all_completed) {
            owner->state = JobState::DONE;
            const Process& last = owner->processes.back();
            if (WIFEXITED(last.status)) owner->exit_status = WEXITSTATUS(last.status);
            else if (WIFSIGNALED(last.status)) owner->exit_status = 128 + WTERMSIG(last.status);
            return owner->exit_status;
        }
        if (any_stopped) {
            owner->state = JobState::STOPPED;
            return owner->exit_status;
        }
    }
}

Job* JobManager::resolve(const std::string& spec) {
    if (spec.empty()) return nullptr;

    if (spec[0] == '%') {
        std::string body = spec.substr(1);
        // `%`, `%+` and `%%` all mean "the current job" (bash convention).
        if (body.empty() || body == "+" || body == "%") return get_current_job();
        if (body == "-") {
            // The job before the current one.
            Job* cur = get_current_job();
            Job* prev = nullptr;
            for (auto& [id, job] : jobs_) {
                if (&job == cur) continue;
                if (job.state != JobState::DONE) prev = &job;
            }
            if (!prev && !jobs_.empty() && cur) {
                for (auto& [id, job] : jobs_) {
                    if (&job != cur) prev = &job;
                }
            }
            return prev;
        }
        if (body[0] == '?') {
            // %?cmd matches the newest job whose command line *contains* cmd.
            std::string want = body.substr(1);
            Job* found = nullptr;
            for (auto& [id, job] : jobs_) {
                if (job.command_line.find(want) != std::string::npos) found = &job;
            }
            return found;
        }
        bool numeric = true;
        for (char c : body) {
            if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
        }
        if (numeric) {
            int id = 0;
            try { id = std::stoi(body); } catch (...) { return nullptr; }
            return get_job(id);
        }
        // %string matches the newest job whose command line starts with the string.
        Job* found = nullptr;
        for (auto& [id, job] : jobs_) {
            if (str_util::starts_with(job.command_line, body)) found = &job;
        }
        return found;
    }

    bool numeric = true;
    for (char c : spec) {
        if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
    }
    if (numeric) {
        int id = 0;
        try { id = std::stoi(spec); } catch (...) { return nullptr; }
        if (Job* job = get_job(id)) return job;
        return get_job_by_pid(static_cast<pid_t>(id));
    }
    return nullptr;
}

void JobManager::put_job_in_foreground(int id, bool cont) {
    Job* job = get_job(id);
    if (!job) {
        std::cerr << "aswell: fg: no such job: " << id << "\n";
        return;
    }

    std::cout << job->command_line << "\n";

    if (is_interactive_ && isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }

    if (cont) {
        if (isatty(STDIN_FILENO)) tcsetattr(STDIN_FILENO, TCSADRAIN, &job->tmodes);
        kill(-job->pgid, SIGCONT);
        job->state = JobState::RUNNING;
    }

    wait_for_job(id);
}

void JobManager::put_job_in_background(int id, bool cont) {
    Job* job = get_job(id);
    if (!job) {
        std::cerr << "aswell: bg: no such job: " << id << "\n";
        return;
    }

    if (cont) {
        kill(-job->pgid, SIGCONT);
        job->state = JobState::RUNNING;
        job->notified = false;
    }

    std::cout << "[" << job->id << "] " << job->command_line << " &\n";
}

void JobManager::list_jobs(bool verbose) {
    update_status();
    Job* current = get_current_job();
    std::vector<int> done_jobs;

    for (auto& [id, job] : jobs_) {
        const bool cur = (&job == current);
        std::cout << "[" << job.id << (cur ? "+" : "-") << "]  ";
        if (verbose) {
            for (size_t i = 0; i < job.processes.size(); ++i) {
                if (i) std::cout << ";";
                std::cout << job.processes[i].pid;
            }
            std::cout << " ";
        }
        std::cout << std::left << std::setw(16) << state_label(job) << job.command_line << "\n";
        if (job.state == JobState::DONE) {
            done_jobs.push_back(id);
        }
    }

    for (int id : done_jobs) {
        remove_job(id);
    }
}

std::vector<int> JobManager::job_ids() const {
    std::vector<int> ids;
    for (const auto& [id, job] : jobs_) {
        if (job.state != JobState::DONE) ids.push_back(id);
    }
    return ids;
}

size_t JobManager::active_job_count() const {
    size_t count = 0;
    for (const auto& [id, job] : jobs_) {
        if (job.state != JobState::DONE) {
            count++;
        }
    }
    return count;
}

} // namespace aswell
