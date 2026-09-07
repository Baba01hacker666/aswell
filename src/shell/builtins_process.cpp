#include "aswell/shell/builtins.hpp"
#include "aswell/shell/signals.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <signal.h>
#include <sys/types.h>

namespace aswell {

int Builtins::builtin_wait(const std::vector<std::string>& args, Environment& /*env*/, JobManager& jobs) {
    if (args.size() <= 1) {
        for (const auto& [id, job] : jobs.get_jobs()) {
            jobs.wait_for_job(id);
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        int id = 0;
        try { id = std::stoi(args[i]); } catch (...) { continue; }
        jobs.wait_for_job(id);
    }
    return 0;
}

int Builtins::builtin_jobs(const std::vector<std::string>& args, JobManager& jobs) {
    bool verbose = (args.size() > 1 && args[1] == "-l");
    jobs.list_jobs(verbose);
    return 0;
}

int Builtins::builtin_fg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 1;
    if (args.size() > 1) {
        std::string s = args[1];
        if (!s.empty() && s[0] == '%') s = s.substr(1);
        try { id = std::stoi(s); } catch (...) { return 1; }
    }
    jobs.put_job_in_foreground(id, true);
    return 0;
}

int Builtins::builtin_bg(const std::vector<std::string>& args, JobManager& jobs) {
    int id = 1;
    if (args.size() > 1) {
        std::string s = args[1];
        if (!s.empty() && s[0] == '%') s = s.substr(1);
        try { id = std::stoi(s); } catch (...) { return 1; }
    }
    jobs.put_job_in_background(id, true);
    return 0;
}

int Builtins::builtin_kill(const std::vector<std::string>& args, Environment& /*env*/) {
    if (args.size() <= 1) {
        std::cerr << "aswell: kill: usage: kill [-sig] pid...\n";
        return 1;
    }

    int sig = SIGTERM;
    size_t idx = 1;
    if (args[idx].size() > 1 && args[idx][0] == '-') {
        std::string signame = args[idx].substr(1);
        int parsed = SignalManager::signame_to_num(signame);
        if (parsed >= 0) {
            sig = parsed;
            idx++;
        }
    }

    for (size_t i = idx; i < args.size(); ++i) {
        pid_t pid = 0;
        try { pid = std::stoi(args[i]); } catch (...) { continue; }
        kill(pid, sig);
    }
    return 0;
}

} // namespace aswell
