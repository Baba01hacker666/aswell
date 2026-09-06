#include "aswell/common.hpp"
#include "aswell/version.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/jobs.hpp"
#include "aswell/shell/signals.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/ui/terminal.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/editor/editor.hpp"
#include "aswell/config/config.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/config/config_editor.hpp"
#include "aswell/plugin/plugin.hpp"
#include <fstream>

using namespace aswell;

static void print_help() {
    std::cout << "Usage: aswell [OPTIONS] [SCRIPT [ARGS...]]\n"
              << "       aswell -c COMMAND [ARGS...]\n"
              << "       aswell config\n"
              << "       aswell theme [list | set NAME]\n\n"
              << "A modern, beautiful, powerful Unix shell with POSIX compatibility.\n\n"
              << "Options:\n"
              << "  -c COMMAND     Execute command string\n"
              << "  --config PATH  Specify custom configuration directory\n"
              << "  --no-theme     Disable theme engine and run plain POSIX output\n"
              << "  --safe-mode    Disable external plugins and third-party scripts\n"
              << "  --version, -v  Print version information\n"
              << "  --help, -h     Print this help message\n\n"
              << "Subcommands:\n"
              << "  config         Open interactive configuration TUI\n"
              << "  theme          List or switch themes\n";
}

int main(int argc, char* argv[]) {
    Environment env;
    JobManager jobs;
    HookManager hooks;
    PluginManager plugins(env, hooks);

    std::string command_string;
    std::string script_path;
    std::vector<std::string> script_args;
    bool explicit_config = false;
    std::string custom_config_path;
    (void)explicit_config;
    (void)custom_config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-c") {
            if (i + 1 < argc) {
                command_string = argv[++i];
                // Remaining args become positional parameters $0, $1, ...
                if (i + 1 < argc) {
                    env.shell_name = argv[++i];
                    std::vector<std::string> pos_args;
                    for (int j = i + 1; j < argc; ++j) {
                        pos_args.push_back(argv[j]);
                    }
                    env.set_positional_params(pos_args);
                }
                break;
            } else {
                std::cerr << "aswell: -c: option requires an argument\n";
                return 2;
            }
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "aswell version " << VERSION << " (POSIX Compatible Shell)\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--no-theme") {
            env.opt_no_theme = true;
        } else if (arg == "--safe-mode") {
            env.opt_safe_mode = true;
        } else if (arg == "--config" && i + 1 < argc) {
            explicit_config = true;
            custom_config_path = argv[++i];
        } else if (arg == "config") {
            // Interactive Configuration Editor TUI
            return ConfigEditor::run_interactive(env);
        } else if (arg == "theme") {
            if (i + 1 >= argc || std::string(argv[i + 1]) == "list") {
                std::cout << "\033[1;34mAvailable Themes:\033[0m\n";
                for (const auto& name : ThemeManager::get_builtin_theme_names()) {
                    auto info = ThemeManager::get_theme(name);
                    std::cout << "  * \033[1;36m" << std::left << std::setw(12) << name << "\033[0m - " << info.description << "\n";
                }
                return 0;
            } else if (std::string(argv[i + 1]) == "set" && i + 2 < argc) {
                ShellConfig cfg = ConfigManager::load();
                cfg.theme_name = argv[i + 2];
                ConfigManager::save(cfg);
                std::cout << "Theme switched to \033[1;32m" << cfg.theme_name << "\033[0m\n";
                return 0;
            }
        } else if (arg[0] == '-') {
            // Option flag like -e, -u, -x
            for (size_t c = 1; c < arg.size(); ++c) {
                if (arg[c] == 'e') env.opt_errexit = true;
                else if (arg[c] == 'u') env.opt_nounset = true;
                else if (arg[c] == 'x') env.opt_xtrace = true;
                else if (arg[c] == 'v') env.opt_verbose = true;
            }
        } else {
            // Script file execution
            script_path = arg;
            env.shell_name = script_path;
            for (int j = i + 1; j < argc; ++j) {
                script_args.push_back(argv[j]);
            }
            env.set_positional_params(script_args);
            break;
        }
    }

    Executor executor(env, jobs);

    // 1. Run command string (-c)
    if (!command_string.empty()) {
        SignalManager::init_signals(false);
        int ret = executor.execute_string(command_string);
        if (env.has_trap(0)) executor.execute_string(env.get_trap(0));
        return ret;
    }

    // 2. Run script file
    if (!script_path.empty()) {
        SignalManager::init_signals(false);
        std::ifstream file(script_path);
        if (!file) {
            std::cerr << "aswell: " << script_path << ": No such file or directory\n";
            return 127;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        int ret = executor.execute_script(ss.str());
        if (env.has_trap(0)) executor.execute_string(env.get_trap(0));
        return ret;
    }

    // 3. Non-interactive pipe execution (e.g. echo "cmd" | aswell)
    if (!isatty(STDIN_FILENO)) {
        SignalManager::init_signals(false);
        std::stringstream ss;
        ss << std::cin.rdbuf();
        int ret = executor.execute_script(ss.str());
        if (env.has_trap(0)) executor.execute_string(env.get_trap(0));
        return ret;
    }

    // 4. Interactive Shell Mode
    env.opt_interactive = true;
    jobs.set_interactive(true);
    SignalManager::init_signals(true);

    // Load configuration
    ShellConfig cfg = ConfigManager::load();
    env.opt_vi_mode = cfg.vi_mode;

    // Load startup file (~/.aswellrc)
    const char* home = std::getenv("HOME");
    if (home) {
        std::string rc_path = std::string(home) + "/.aswellrc";
        std::ifstream rc_file(rc_path);
        if (rc_file) {
            std::stringstream rcss;
            rcss << rc_file.rdbuf();
            executor.execute_script(rcss.str());
        }
    }

    // Setup prompt engine
    PromptEngine prompt_engine(env);
    if (!env.opt_no_theme) {
        ThemeInfo tinfo = ThemeManager::get_theme(cfg.theme_name);
        prompt_engine.set_theme_css(tinfo.css_content);
        prompt_engine.set_template_html(ConfigManager::build_template(cfg));
    } else {
        // Plain prompt
        prompt_engine.set_template_html("<prompt><text>aswell $ </text></prompt>");
        prompt_engine.set_theme_css("prompt { color: none; }");
    }

    LineEditor editor(env, prompt_engine);

    // Startup banner
    if (!env.opt_no_theme) {
        std::cout << "\033[1;36m╭─────────────────────────────────────────────────────────────╮\033[0m\n";
        std::cout << "\033[1;36m│\033[0m   \033[1;37m" << SHELL_BANNER << "\033[0m    \033[1;36m│\033[0m\n";
        std::cout << "\033[1;36m│\033[0m   Type \033[1;33m'help'\033[0m for builtins, \033[1;33m'aswell config'\033[0m for visual settings  \033[1;36m│\033[0m\n";
        std::cout << "\033[1;36m╰─────────────────────────────────────────────────────────────╯\033[0m\n\n";
    }

    hooks.trigger_hook(HookType::ON_START);

    // Main Interactive REPL loop
    while (true) {
        hooks.trigger_hook(HookType::ON_PROMPT);

        jobs.update_status();
        size_t active_jobs = jobs.active_job_count();
        double last_duration = executor.get_last_command_duration_ms();

        auto line_opt = editor.read_line(last_duration, active_jobs);
        if (!line_opt.has_value()) {
            break; // EOF (Ctrl+D)
        }

        std::string line = *line_opt;
        if (str_util::trim(line).empty()) {
            continue;
        }

        hooks.trigger_hook(HookType::BEFORE_COMMAND, {line});
        int status = executor.execute_string(line);
        hooks.trigger_hook(HookType::AFTER_COMMAND, {line, std::to_string(executor.get_last_command_duration_ms()), std::to_string(status)});

        if (status != 0) {
            hooks.trigger_hook(HookType::ON_ERROR, {line, std::to_string(status)});
        }
    }

    if (env.has_trap(0)) executor.execute_string(env.get_trap(0));
    hooks.trigger_hook(HookType::ON_EXIT, {std::to_string(env.last_exit_status)});
    return env.last_exit_status;
}
