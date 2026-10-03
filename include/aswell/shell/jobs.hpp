#pragma once

#include "aswell/common.hpp"

namespace aswell {

enum class JobState {
    RUNNING,
    STOPPED,
    DONE
};

struct Process {
    pid_t pid = 0;
    std::string command;
    bool completed = false;
    bool stopped = false;
    int status = 0;
};

struct Job {
    int id = 1;
    pid_t pgid = 0;
    std::string command_line;
    std::vector<Process> processes;
    JobState state = JobState::RUNNING;
    // Exit status of the job (WEXITSTATUS / 128+signal), filled in when reaped.
    int exit_status = 0;
    // True once the "[1]+ Done cmd" line has been shown to the user.
    bool notified = false;
    struct termios tmodes;
};

class JobManager {
public:
    JobManager();
    ~JobManager();

    void set_interactive(bool interactive);
    bool is_interactive() const { return is_interactive_; }

    // `pgid` is the job's process group. Callers must put the child into that
    // group (setpgid) so `fg`, `bg` and `kill %1` can signal the whole job.
    int add_job(pid_t pgid, const std::string& cmd_line, const std::vector<pid_t>& pids);
    Job* get_job(int id);
    Job* get_job_by_pgid(pid_t pgid);
    Job* get_job_by_pid(pid_t pid);
    Job* get_current_job();
    void remove_job(int id);

    // Collects finished/stopped children. Returns true if anything changed.
    bool update_status();
    // Prints bash-style "[1]+  Done    cmd" lines for jobs that just stopped or
    // exited, and forgets the finished ones. Returns how many were printed.
    int notify_finished();
    // Blocks until the job finishes; returns its exit status.
    int wait_for_job(int id);
    // Blocks until *any* tracked job finishes; returns its exit status (-1 if
    // there is nothing to wait for). Backs `wait -n`.
    int wait_for_any();
    void put_job_in_foreground(int id, bool cont);
    void put_job_in_background(int id, bool cont);

    void list_jobs(bool verbose = false);
    // Job ids of everything still running or stopped (for `jobs -p` and
    // scripts that want to fan out work and `wait` on the result).
    std::vector<int> job_ids() const;
    size_t active_job_count() const;

    // Resolves a `%1` / `%+` / `%-` / `%string` job spec, or a bare number
    // (job id first, then pid). Returns nullptr when nothing matches.
    Job* resolve(const std::string& spec);

    // "Done", "Exit 1", "Stopped", "Running" — the way job state is displayed.
    static std::string state_label(const Job& job);

    const std::map<int, Job>& get_jobs() const { return jobs_; }

private:
    std::map<int, Job> jobs_;
    int next_job_id_ = 1;
    bool is_interactive_ = false;
    pid_t shell_pgid_ = 0;
    struct termios shell_tmodes_;
};

} // namespace aswell
