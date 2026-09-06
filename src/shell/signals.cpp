#include "aswell/shell/signals.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

volatile sig_atomic_t SignalManager::received_signals[64] = {0};
volatile sig_atomic_t SignalManager::any_signal_pending = 0;

static void sig_handler(int signum) {
    if (signum >= 0 && signum < 64) {
        SignalManager::received_signals[signum] = 1;
        SignalManager::any_signal_pending = 1;
    }
}

void SignalManager::init_signals(bool interactive) {
    for (size_t i = 0; i < sizeof(received_signals) / sizeof(received_signals[0]); ++i) {
        received_signals[i] = 0;
    }
    any_signal_pending = 0;

    if (interactive) {
        // In interactive mode, ignore terminal stop/interrupt signals for shell itself
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, sig_handler);
    }
}

void SignalManager::reset_signals_for_child() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}

void SignalManager::handle_pending_traps(Environment& env, const std::function<void(const std::string&)>& runner) {
    if (!any_signal_pending) return;
    any_signal_pending = 0;

    for (int sig = 1; sig < 64; ++sig) {
        if (received_signals[sig]) {
            received_signals[sig] = 0;
            if (env.has_trap(sig)) {
                std::string action = env.get_trap(sig);
                if (!action.empty() && action != "''" && runner) {
                    runner(action);
                }
            }
        }
    }
}

int SignalManager::signame_to_num(const std::string& name) {
    std::string s = str_util::to_lower(name);
    if (str_util::starts_with(s, "sig")) {
        s = s.substr(3);
    }
    if (s == "exit" || s == "0") return 0;
    if (s == "hup" || s == "1") return SIGHUP;
    if (s == "int" || s == "2") return SIGINT;
    if (s == "quit" || s == "3") return SIGQUIT;
    if (s == "ill" || s == "4") return SIGILL;
    if (s == "abrt" || s == "6") return SIGABRT;
    if (s == "fpe" || s == "8") return SIGFPE;
    if (s == "kill" || s == "9") return SIGKILL;
    if (s == "segv" || s == "11") return SIGSEGV;
    if (s == "pipe" || s == "13") return SIGPIPE;
    if (s == "alrm" || s == "14") return SIGALRM;
    if (s == "term" || s == "15") return SIGTERM;
    if (s == "chld" || s == "17") return SIGCHLD;
    if (s == "cont" || s == "18") return SIGCONT;
    if (s == "stop" || s == "19") return SIGSTOP;
    if (s == "tstp" || s == "20") return SIGTSTP;
    if (s == "ttin" || s == "21") return SIGTTIN;
    if (s == "ttou" || s == "22") return SIGTTOU;
    if (s == "usr1" || s == "10") return SIGUSR1;
    if (s == "usr2" || s == "12") return SIGUSR2;
    return -1;
}

std::string SignalManager::signum_to_name(int signum) {
    switch (signum) {
        case 0: return "EXIT";
        case SIGHUP: return "HUP";
        case SIGINT: return "INT";
        case SIGQUIT: return "QUIT";
        case SIGKILL: return "KILL";
        case SIGTERM: return "TERM";
        case SIGCHLD: return "CHLD";
        case SIGTSTP: return "TSTP";
        case SIGCONT: return "CONT";
        case SIGUSR1: return "USR1";
        case SIGUSR2: return "USR2";
        default: return std::to_string(signum);
    }
}

} // namespace aswell
