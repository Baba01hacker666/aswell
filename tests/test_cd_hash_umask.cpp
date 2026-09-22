#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aswell/shell/builtins.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/jobs.hpp"

using namespace aswell;

// Capture stdout of fn() without forking (single-threaded test).
static std::string capture_stdout(const std::function<void()>& fn) {
    std::cout.flush(); // drain pre-existing buffered output first
    int pipefd[2];
    assert(pipe(pipefd) == 0);
    int saved = dup(STDOUT_FILENO);
    assert(saved >= 0);
    assert(dup2(pipefd[1], STDOUT_FILENO) >= 0);
    close(pipefd[1]);
    fn();
    std::cout.flush();
    assert(dup2(saved, STDOUT_FILENO) >= 0);
    close(saved);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    assert(fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == 0);
    std::string out;
    char buf[256];
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        out.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);
    return out;
}

static std::string get_cwd() {
    char buf[4096];
    assert(getcwd(buf, sizeof(buf)) != nullptr);
    return buf;
}

static void test_smart_cd() {
    char tmpl[] = "/tmp/aswell_test_cdXXXXXX";
    assert(mkdtemp(tmpl) != nullptr);
    std::string base = tmpl;
    assert(mkdir((base + "/Documents").c_str(), 0755) == 0);
    assert(mkdir((base + "/Downloads").c_str(), 0755) == 0);
    assert(mkdir((base + "/Music").c_str(), 0755) == 0);
    std::string saved = get_cwd();
    assert(chdir(base.c_str()) == 0);

    Environment env;
    env.opt_interactive = true;
    JobManager jobs;
    Executor exec(env, jobs);
    ControlFlow flow;

    // Unique prefix resolves and lands there.
    assert(Builtins::execute("cd", {"cd", "Doc"}, env, jobs, exec, flow) == 0);
    assert(get_cwd() == base + "/Documents");

    // Ambiguous prefix fails.
    assert(chdir(base.c_str()) == 0);
    assert(Builtins::execute("cd", {"cd", "D"}, env, jobs, exec, flow) == 1);
    assert(get_cwd() == base);

    // Case-insensitive fallback.
    assert(Builtins::execute("cd", {"cd", "mus"}, env, jobs, exec, flow) == 0);
    assert(get_cwd() == base + "/Music");

    // No match keeps the classic error.
    assert(Builtins::execute("cd", {"cd", "zzz_no_such"}, env, jobs, exec, flow) == 1);

    // Non-interactive stays strict POSIX: no guessing in scripts.
    assert(chdir(base.c_str()) == 0);
    env.opt_interactive = false;
    assert(Builtins::execute("cd", {"cd", "Doc"}, env, jobs, exec, flow) == 1);
    assert(get_cwd() == base);

    assert(chdir(saved.c_str()) == 0);
    (void)rmdir((base + "/Documents").c_str());
    (void)rmdir((base + "/Downloads").c_str());
    (void)rmdir((base + "/Music").c_str());
    (void)rmdir(base.c_str());
    std::cout << "[PASS] test_smart_cd\n";
}

static void test_hash() {
    Environment env;
    JobManager jobs;
    Executor exec(env, jobs);
    ControlFlow flow;

    std::string ls_path = env.find_in_path("ls");
    assert(!ls_path.empty());

    // Unknown command is rejected, table untouched.
    assert(Builtins::execute("hash", {"hash", "zz_no_such_cmd_xyz"}, env, jobs, exec, flow) == 1);
    std::string junk;
    assert(!env.get_remembered_command("zz_no_such_cmd_xyz", junk));

    // Resolving remembers the path.
    assert(Builtins::execute("hash", {"hash", "ls"}, env, jobs, exec, flow) == 0);
    std::string remembered;
    assert(env.get_remembered_command("ls", remembered) && remembered == ls_path);

    // -t prints the remembered path.
    std::string out = capture_stdout([&]() {
        assert(Builtins::execute("hash", {"hash", "-t", "ls"}, env, jobs, exec, flow) == 0);
    });
    assert(out == ls_path + "\n");

    // Bare hash lists the table.
    out = capture_stdout([&]() {
        assert(Builtins::execute("hash", {"hash"}, env, jobs, exec, flow) == 0);
    });
    assert(out.find("hits") != std::string::npos && out.find(ls_path) != std::string::npos);

    // -d forgets; second -d reports not found.
    assert(Builtins::execute("hash", {"hash", "-d", "ls"}, env, jobs, exec, flow) == 0);
    assert(!env.get_remembered_command("ls", remembered));
    assert(Builtins::execute("hash", {"hash", "-d", "ls"}, env, jobs, exec, flow) == 1);

    // -p seeds the table directly.
    assert(Builtins::execute("hash", {"hash", "-p", ls_path, "myls"}, env, jobs, exec, flow) == 0);
    assert(env.get_remembered_command("myls", remembered) && remembered == ls_path);
    assert(Builtins::execute("hash", {"hash", "-p", "/nonexistent_xyz_123", "bad"}, env, jobs, exec, flow) == 1);

    // -r clears everything.
    assert(Builtins::execute("hash", {"hash", "-r"}, env, jobs, exec, flow) == 0);
    assert(env.get_command_hash().empty());

    // Invalid option.
    assert(Builtins::execute("hash", {"hash", "-z"}, env, jobs, exec, flow) == 1);
    std::cout << "[PASS] test_hash\n";
}

struct UmaskGuard {
    mode_t saved;
    UmaskGuard() {
        saved = umask(0);
        umask(saved);
    }
    ~UmaskGuard() { umask(saved); }
};

static void test_umask() {
    UmaskGuard guard;
    Environment env;
    JobManager jobs;
    Executor exec(env, jobs);
    ControlFlow flow;

    auto read_mask = []() {
        mode_t m = umask(0);
        umask(m);
        return m;
    };

    // Octal set + octal display (unchanged behavior).
    assert(Builtins::execute("umask", {"umask", "022"}, env, jobs, exec, flow) == 0);
    assert(read_mask() == static_cast<mode_t>(022));

    // Symbolic display.
    std::string out = capture_stdout([&]() {
        assert(Builtins::execute("umask", {"umask", "-S"}, env, jobs, exec, flow) == 0);
    });
    assert(out == "u=rwx,g=rx,o=rx\n");

    // Symbolic set.
    assert(Builtins::execute("umask", {"umask", "u=rwx,g=,o="}, env, jobs, exec, flow) == 0);
    assert(read_mask() == static_cast<mode_t>(077));

    // Relative adjust from current.
    assert(Builtins::execute("umask", {"umask", "a+rx"}, env, jobs, exec, flow) == 0);
    assert(read_mask() == static_cast<mode_t>(022));
    assert(Builtins::execute("umask", {"umask", "go-w"}, env, jobs, exec, flow) == 0);
    assert(read_mask() == static_cast<mode_t>(022)); // w already masked; unchanged

    // Invalid specs rejected, mask untouched.
    assert(Builtins::execute("umask", {"umask", "zzz"}, env, jobs, exec, flow) == 1);
    assert(Builtins::execute("umask", {"umask", "0999"}, env, jobs, exec, flow) == 1);
    assert(read_mask() == static_cast<mode_t>(022));
    std::cout << "[PASS] test_umask\n";
}

int main() {
    test_smart_cd();
    test_hash();
    test_umask();
    std::cout << "[PASS] cd/hash/umask suite\n";
    return 0;
}
