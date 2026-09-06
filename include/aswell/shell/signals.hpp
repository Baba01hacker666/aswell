#pragma once

#include "aswell/common.hpp"

namespace aswell {

class Environment;

class SignalManager {
public:
    static void init_signals(bool interactive);
    static void reset_signals_for_child();
    static void handle_pending_traps(Environment& env, const std::function<void(const std::string&)>& runner);
    static int signame_to_num(const std::string& name);
    static std::string signum_to_name(int signum);
    static volatile sig_atomic_t received_signals[64];
    static volatile sig_atomic_t any_signal_pending;
};

} // namespace aswell
