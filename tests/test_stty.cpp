#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "aswell/shell/builtins.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/jobs.hpp"

using namespace aswell;

// A fresh pseudoterminal per test: the builtin operates on the slave side
// via -F, and assertions read it back with tcgetattr/ioctl.
struct Pty {
    int master = -1;
    int slave = -1;
    std::string path;
    Pty() {
        assert(openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);
        char buf[128] = {0};
        // ttyname (not ptsname: the latter needs ptmx ioctls that some
        // sandboxes deny, while ttyname works everywhere).
        assert(ttyname_r(slave, buf, sizeof(buf)) == 0 && buf[0] != '\0');
        path = buf;
    }
    ~Pty() {
        if (master >= 0) close(master);
        if (slave >= 0) close(slave);
    }
};

struct Ctx {
    Environment env;
    JobManager jobs;
    Executor exec;
    ControlFlow flow;
    Ctx() : exec(env, jobs) {}

    int stty(const std::vector<std::string>& operands) {
        std::vector<std::string> args = {"stty"};
        args.insert(args.end(), operands.begin(), operands.end());
        return Builtins::execute("stty", args, env, jobs, exec, flow);
    }
};

static std::string capture_stdout(const std::function<void()>& fn) {
    std::cout.flush();
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

static struct termios read_slave(const Pty& p) {
    struct termios tio{};
    assert(tcgetattr(p.slave, &tio) == 0);
    return tio;
}

static void test_flag_toggle() {
    Pty p;
    Ctx c;
    assert(c.stty({"-F", p.path, "-echo"}) == 0);
    assert((read_slave(p).c_lflag & ECHO) == 0);
    assert(c.stty({"-F", p.path, "echo"}) == 0);
    assert((read_slave(p).c_lflag & ECHO) != 0);
    assert(c.stty({"-F", p.path, "-icanon", "-isig"}) == 0);
    struct termios tio = read_slave(p);
    assert((tio.c_lflag & ICANON) == 0);
    assert((tio.c_lflag & ISIG) == 0);
    assert(c.stty({"-F", p.path, "icanon", "isig"}) == 0);
    tio = read_slave(p);
    assert((tio.c_lflag & ICANON) != 0);
    assert((tio.c_lflag & ISIG) != 0);
    std::cout << "[PASS] test_flag_toggle\n";
}

static void test_control_chars() {
    Pty p;
    Ctx c;
    assert(c.stty({"-F", p.path, "erase", "^H"}) == 0);
    assert(read_slave(p).c_cc[VERASE] == 8);
    assert(c.stty({"-F", p.path, "intr", "^-"}) == 0);
    assert(read_slave(p).c_cc[VINTR] == 0);
    assert(c.stty({"-F", p.path, "kill", "undef"}) == 0);
    assert(read_slave(p).c_cc[VKILL] == 0);
    assert(c.stty({"-F", p.path, "eof", "^D"}) == 0);
    assert(read_slave(p).c_cc[VEOF] == 4);
    assert(c.stty({"-F", p.path, "min", "1", "time", "5"}) == 0);
    struct termios tio = read_slave(p);
    assert(tio.c_cc[VMIN] == 1 && tio.c_cc[VTIME] == 5);
    std::cout << "[PASS] test_control_chars\n";
}

static void test_size_and_speed() {
    Pty p;
    Ctx c;
    assert(c.stty({"-F", p.path, "rows", "40", "cols", "100"}) == 0);
    struct winsize ws{};
    assert(ioctl(p.slave, TIOCGWINSZ, &ws) == 0);
    assert(ws.ws_row == 40 && ws.ws_col == 100);

    assert(c.stty({"-F", p.path, "speed", "9600"}) == 0);
    struct termios tio = read_slave(p);
    assert(cfgetispeed(&tio) == B9600 && cfgetospeed(&tio) == B9600);

    std::string out = capture_stdout([&]() {
        assert(c.stty({"-F", p.path, "size"}) == 0);
    });
    assert(out == "40 100\n");
    out = capture_stdout([&]() {
        assert(c.stty({"-F", p.path, "speed"}) == 0);
    });
    assert(out == "9600\n");
    std::cout << "[PASS] test_size_and_speed\n";
}

static void test_sane_raw_and_save_restore() {
    Pty p;
    Ctx c;
    assert(c.stty({"-F", p.path, "raw"}) == 0);
    struct termios raw_tio = read_slave(p);
    assert((raw_tio.c_lflag & (ICANON | ECHO)) == 0);

    std::string saved = capture_stdout([&]() {
        assert(c.stty({"-F", p.path, "-g"}) == 0);
    });
    assert(saved.compare(0, 12, "aswell-stty:") == 0);
    assert(!saved.empty() && saved.back() == '\n');
    saved.pop_back();

    assert(c.stty({"-F", p.path, "sane"}) == 0);
    struct termios sane_tio = read_slave(p);
    assert((sane_tio.c_lflag & (ICANON | ECHO)) != 0);

    assert(c.stty({"-F", p.path, saved}) == 0);
    struct termios restored = read_slave(p);
    assert(restored.c_iflag == raw_tio.c_iflag && restored.c_oflag == raw_tio.c_oflag &&
           restored.c_cflag == raw_tio.c_cflag && restored.c_lflag == raw_tio.c_lflag);
    for (int i = 0; i < NCCS; ++i) assert(restored.c_cc[i] == raw_tio.c_cc[i]);
    std::cout << "[PASS] test_sane_raw_and_save_restore\n";
}

static void test_display_and_errors() {
    Pty p;
    Ctx c;
    std::string out = capture_stdout([&]() {
        assert(c.stty({"-F", p.path, "-a"}) == 0);
    });
    assert(out.find("speed") != std::string::npos);
    assert(out.find("rows") != std::string::npos);
    assert(out.find("intr = ") != std::string::npos);

    out = capture_stdout([&]() {
        assert(c.stty({"-F", p.path}) == 0);
    });
    assert(out.find("baud") != std::string::npos);

    // Error paths: non-tty device, unknown operand, missing value, bad speed.
    assert(c.stty({"-F", "/dev/null"}) == 1);
    assert(c.stty({"-F", p.path, "bogusflag"}) == 1);
    assert(c.stty({"-F", p.path, "rows"}) == 1);
    assert(c.stty({"-F", p.path, "speed", "12345"}) == 1);
    assert(c.stty({"-F", p.path, "erase", "toolong!!"}) == 1);
    assert(c.stty({"--help"}) == 0);
    std::cout << "[PASS] test_display_and_errors\n";
}

int main() {
    test_flag_toggle();
    test_control_chars();
    test_size_and_speed();
    test_sane_raw_and_save_restore();
    test_display_and_errors();
    std::cout << "[PASS] stty suite\n";
    return 0;
}
