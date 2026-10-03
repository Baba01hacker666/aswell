#pragma once

#include "aswell/common.hpp"
#include "aswell/config/config.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {
class Executor;
}

namespace aswell {

// The user-facing customization hub: `aswell config`, `aswell theme`,
// `aswell doctor` and `aswell reload`.
//
// A single implementation backs both the shell builtin (`aswell config ...`)
// and the standalone CLI (`aswell config ...` before the shell starts), so the
// two entry points can never drift apart again. Everything it prints about
// settings is generated from SettingsRegistry, which keeps the docs, the TUI,
// validation and Tab completion in sync with the code.
class SettingsCli {
public:
    // args[0] is the subcommand word itself ("config", "theme", ...).
    static int handle_config(const std::vector<std::string>& args, Environment& env);
    static int handle_theme(const std::vector<std::string>& args, Environment& env);
    static int handle_doctor(const std::vector<std::string>& args, Environment& env);
    static int handle_reload(const std::vector<std::string>& args, Environment& env);
    // The curated alias/function library. `executor` is needed to define shell
    // functions in the running session; the standalone CLI passes nullptr and
    // only writes ~/.config/aswell/aliases.
    static int handle_aliases(const std::vector<std::string>& args, Environment& env,
                              Executor* executor = nullptr);

    static void print_help();

    // Reusable pieces (also used by `aswell help` and the test suite).
    static std::string settings_as_json(const ShellConfig& cfg, bool include_defaults);
};

} // namespace aswell
