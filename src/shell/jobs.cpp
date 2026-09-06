#include "aswell/shell/jobs.hpp"
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

Job* JobManager::get_current_job() {
    if (jobs_.empty()) return nullptr;
    return &jobs_.rbegin()->second;
}

void JobManager::remove_job(int id) {
    jobs_.erase(id);
}

void JobManager::update_status() {
    int status = 0;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto& [id, job] : jobs_) {
            for (auto& proc : job.processes) {
                if (proc.pid == pid) {
                    proc.status = status;
                    if (WIFSTOPPED(status)) {
                        proc.stopped = true;
                    } else if (WIFCONTINUED(status)) {
                        proc.stopped = false;
                    } else {
                        proc.completed = true;
                    }
                    break;
                }
            }

            // Check job state
            bool all_completed = true;
            bool any_stopped = false;
            for (const auto& proc : job.processes) {
                if (!proc.completed) all_completed = false;
                if (proc.stopped) any_stopped = true;
            }

            if (all_completed) {
                job.state = JobState::DONE;
            } else if (any_stopped) {
                job.state = JobState::STOPPED;
            } else {
                job.state = JobState::RUNNING;
            }
        }
    }
}

void JobManager::wait_for_job(int id) {
    Job* job = get_job(id);
    if (!job) return;

    int status = 0;
    while (job->state == JobState::RUNNING) {
        pid_t pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid <= 0) break;

        for (auto& proc : job->processes) {
            if (proc.pid == pid) {
                proc.status = status;
                if (WIFSTOPPED(status)) {
                    proc.stopped = true;
                } else {
                    proc.completed = true;
                }
                break;
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
            break;
        } else if (any_stopped) {
            job->state = JobState::STOPPED;
            break;
        }
    }

    if (is_interactive_ && isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pgid_);
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes_);
    }

    if (job->state == JobState::DONE) {
        remove_job(id);
    }
}

void JobManager::put_job_in_foreground(int id, bool cont) {
    Job* job = get_job(id);
    if (!job) {
        std::cerr << "aswell: fg: no such job: " << id << "\n";
        return;
    }

    if (is_interactive_ && isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }

    if (cont) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &job->tmodes);
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
    }

    std::cout << "[" << job->id << "] " << job->command_line << " &\n";
}

void JobManager::list_jobs(bool /*verbose*/) {
    update_status();
    std::vector<int> done_jobs;

    for (auto& [id, job] : jobs_) {
        std::string state_str;
        switch (job.state) {
            case JobState::RUNNING: state_str = "Running"; break;
            case JobState::STOPPED: state_str = "Stopped"; break;
            case JobState::DONE:    state_str = "Done"; break;
        }

        std::cout << "[" << job.id << "]  " << std::left << std::setw(10) << state_str << " " << job.command_line << "\n";
        if (job.state == JobState::DONE) {
            done_jobs.push_back(id);
        }
    }

    for (int id : done_jobs) {
        remove_job(id);
    }
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
