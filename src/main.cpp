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
#include "aswell/ui/demo.hpp"
#include "aswell/ui/template_engine.hpp"
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

using namespace aswell;

static void print_help() {
    std::cout << "Usage: aswell [OPTIONS] [SCRIPT [ARGS...]]\n"
              << "       aswell -c COMMAND [ARGS...]\n"
              << "       aswell config\n"
              << "       aswell theme [list | set NAME]\n"
              << "       aswell color [OPTIONS] <COLOR> [TEXT...]\n\n"
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
              << "  theme          List or switch themes\n"
              << "  color          Print colored text or inspect palettes\n"
              << "  demo           Run engine animation & UI showcase (--auto for headless)\n";
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

                std::string themes_dir = ConfigManager::get_config_dir() + "/themes";
                DIR* d = opendir(themes_dir.c_str());
                if (d) {
                    struct dirent* entry;
                    bool header_printed = false;
                    while ((entry = readdir(d)) != nullptr) {
                        std::string fname = entry->d_name;
                        if (str_util::ends_with(fname, ".css")) {
                            if (!header_printed) {
                                std::cout << "\n\033[1;35mCustom User Themes (~/.config/aswell/themes/):\033[0m\n";
                                header_printed = true;
                            }
                            std::string th_name = fname.substr(0, fname.size() - 4);
                            std::cout << "  * \033[1;32m" << std::left << std::setw(12) << th_name << "\033[0m - Custom user stylesheet\n";
                        }
                    }
                    closedir(d);
                }
                return 0;
            } else if (std::string(argv[i + 1]) == "set" && i + 2 < argc) {
                ShellConfig cfg = ConfigManager::load();
                cfg.theme_name = argv[i + 2];
                ConfigManager::save(cfg);
                std::cout << "Theme switched to \033[1;32m" << cfg.theme_name << "\033[0m\n";
                return 0;
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                ShellConfig cfg = ConfigManager::load();
                cfg.theme_name = argv[i + 1];
                ConfigManager::save(cfg);
                std::cout << "Theme switched to \033[1;32m" << cfg.theme_name << "\033[0m\n";
                return 0;
            }
        } else if (arg == "color") {
            std::vector<std::string> color_args;
            color_args.push_back("color");
            for (int j = i + 1; j < argc; ++j) {
                color_args.push_back(argv[j]);
            }
            return Builtins::builtin_color(color_args, env);
        } else if (arg == "demo" || arg == "--demo") {
            bool auto_mode = false;
            for (int j = i + 1; j < argc; ++j) {
                if (std::string(argv[j]) == "--auto") {
                    auto_mode = true;
                }
            }
            return EngineDemo::run(env, auto_mode);
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

    plugins.set_script_runner([&executor](const std::string& script) {
        return executor.execute_script(script);
    });

    hooks.set_shell_dispatcher([&executor, &env](HookType type, const std::vector<std::string>& args) {
        switch (type) {
            case HookType::ON_START:
                if (env.has_function("aswell_on_start")) executor.execute_function("aswell_on_start", args);
                break;
            case HookType::ON_PROMPT:
                if (env.has_function("aswell_on_prompt")) executor.execute_function("aswell_on_prompt", args);
                else if (env.has_function("precmd")) executor.execute_function("precmd", args);
                break;
            case HookType::BEFORE_COMMAND:
                if (env.has_function("aswell_before_command")) executor.execute_function("aswell_before_command", args);
                else if (env.has_function("preexec")) executor.execute_function("preexec", args);
                break;
            case HookType::AFTER_COMMAND:
                if (env.has_function("aswell_after_command")) executor.execute_function("aswell_after_command", args);
                else if (env.has_function("postexec")) executor.execute_function("postexec", args);
                break;
            case HookType::ON_ERROR:
                if (env.has_function("aswell_on_error")) executor.execute_function("aswell_on_error", args);
                break;
            case HookType::ON_DIR_CHANGE:
                if (env.has_function("aswell_on_dir_change")) executor.execute_function("aswell_on_dir_change", args);
                else if (env.has_function("chpwd")) executor.execute_function("chpwd", args);
                break;
            case HookType::ON_EXIT:
                if (env.has_function("aswell_on_exit")) executor.execute_function("aswell_on_exit", args);
                break;
            default:
                break;
        }
    });

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
    if (cfg.enable_colored_output && !env.opt_no_theme) {
        env.init_color_aliases();
    }

    // Load startup file (~/.aswellrc or ~/.config/aswell/aswellrc)
    const char* home = std::getenv("HOME");
    std::string rc_path;
    if (home) {
        std::string p1 = std::string(home) + "/.aswellrc";
        std::string p2 = ConfigManager::get_config_dir() + "/aswellrc";
        struct stat st;
        if (stat(p1.c_str(), &st) == 0) {
            rc_path = p1;
        } else if (stat(p2.c_str(), &st) == 0) {
            rc_path = p2;
        }
    }
    if (!rc_path.empty()) {
        std::ifstream rc_file(rc_path);
        if (rc_file) {
            std::stringstream rcss;
            rcss << rc_file.rdbuf();
            executor.execute_script(rcss.str());
        }
    }

    // Load user plugins from ~/.config/aswell/plugins
    std::string user_plugin_dir = ConfigManager::get_config_dir() + "/plugins";
    plugins.load_plugins_from_directory(user_plugin_dir, env.opt_safe_mode);

    // Setup prompt engine
    PromptEngine prompt_engine(env);
    if (!env.opt_no_theme) {
        // 1. Custom CSS theme check (~/.config/aswell/theme.css or ~/.config/aswell/themes/<name>.css)
        std::string custom_css_path = ConfigManager::get_config_dir() + "/theme.css";
        std::ifstream css_f(custom_css_path);
        if (css_f) {
            std::stringstream ss;
            ss << css_f.rdbuf();
            prompt_engine.set_theme_css(ss.str());
        } else {
            ThemeInfo tinfo = ThemeManager::get_custom_theme(ConfigManager::get_config_dir() + "/themes", cfg.theme_name);
            prompt_engine.set_theme_css(tinfo.css_content);
        }

        // 2. Custom prompt HTML template check (~/.config/aswell/prompt.html)
        std::string custom_html_path = ConfigManager::get_config_dir() + "/prompt.html";
        std::ifstream html_f(custom_html_path);
        if (html_f) {
            std::stringstream ss;
            ss << html_f.rdbuf();
            prompt_engine.set_template_html(ss.str());
        } else {
            prompt_engine.set_template_html(ConfigManager::build_template(cfg));
        }
    } else {
        // Plain prompt
        prompt_engine.set_template_html("<prompt><text>aswell $ </text></prompt>");
        prompt_engine.set_theme_css("prompt { color: none; }");
    }

    LineEditor editor(env, prompt_engine);
    editor.set_command_animation(cfg.enable_command_animation);

    hooks.trigger_hook(HookType::ON_START);

    std::string last_pwd = env.get_var("PWD");

    // Main Interactive REPL loop
    while (true) {
        std::string cur_pwd = env.get_var("PWD");
        if (cur_pwd != last_pwd) {
            hooks.trigger_hook(HookType::ON_DIR_CHANGE, {cur_pwd});
            last_pwd = cur_pwd;
        }

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

        // History expansion (!csh/bash-style)
        if (line.find('!') != std::string::npos) {
            editor.history().pop_last();
            std::string hist_err;
            auto expanded_opt = editor.history().expand_history(line, hist_err);
            if (!expanded_opt.has_value()) {
                std::cerr << "aswell: " << hist_err << "\n";
                env.last_exit_status = 1;
                continue;
            }
            line = *expanded_opt;
            editor.history().add(line);
            if (line != *line_opt) {
                std::cout << line << "\n";
            }
        }

        hooks.trigger_hook(HookType::BEFORE_COMMAND, {line});
        TemplateEngine::handle_event(line, Terminal::is_interactive_tty());
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
