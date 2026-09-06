#pragma once

#include "aswell/common.hpp"

namespace aswell {

enum class HookType {
    ON_START,
    ON_COMMAND,
    BEFORE_COMMAND,
    AFTER_COMMAND,
    ON_DIR_CHANGE,
    ON_EXIT,
    ON_ERROR,
    ON_PROMPT
};

using HookCallback = std::function<void(const std::vector<std::string>& args)>;
using ShellHookDispatcher = std::function<void(HookType, const std::vector<std::string>&)>;

class HookManager {
public:
    void register_hook(HookType type, HookCallback cb) {
        hooks_[type].push_back(std::move(cb));
    }

    void set_shell_dispatcher(ShellHookDispatcher dispatcher) {
        shell_dispatcher_ = std::move(dispatcher);
    }

    void trigger_hook(HookType type, const std::vector<std::string>& args = {}) {
        auto it = hooks_.find(type);
        if (it != hooks_.end()) {
            for (const auto& cb : it->second) {
                cb(args);
            }
        }
        if (shell_dispatcher_) {
            shell_dispatcher_(type, args);
        }
    }

    void clear() {
        hooks_.clear();
    }

private:
    std::unordered_map<HookType, std::vector<HookCallback>> hooks_;
    ShellHookDispatcher shell_dispatcher_;
};

} // namespace aswell
