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
    bool notified = false;
    struct termios tmodes;
};

class JobManager {
public:
    JobManager();
    ~JobManager();

    void set_interactive(bool interactive);
    bool is_interactive() const { return is_interactive_; }

    int add_job(pid_t pgid, const std::string& cmd_line, const std::vector<pid_t>& pids);
    Job* get_job(int id);
    Job* get_job_by_pgid(pid_t pgid);
    Job* get_current_job();
    void remove_job(int id);

    void update_status();
    void wait_for_job(int id);
    void put_job_in_foreground(int id, bool cont);
    void put_job_in_background(int id, bool cont);

    void list_jobs(bool verbose = false);
    size_t active_job_count() const;

    const std::map<int, Job>& get_jobs() const { return jobs_; }

private:
    std::map<int, Job> jobs_;
    int next_job_id_ = 1;
    bool is_interactive_ = false;
    pid_t shell_pgid_ = 0;
    struct termios shell_tmodes_;
};

} // namespace aswell
