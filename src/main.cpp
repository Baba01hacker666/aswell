#include "aswell/common.hpp"
#include "aswell/version.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/jobs.hpp"
#include "aswell/shell/signals.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/ui/terminal.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/editor/editor.hpp"
#include "aswell/config/alias_library.hpp"
#include "aswell/config/config.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/config/config_editor.hpp"
#include "aswell/shell/bash_compat.hpp"
#include "aswell/plugin/plugin.hpp"
#include "aswell/ui/demo.hpp"
#include "aswell/ui/template_engine.hpp"
#include <fstream>
#include <iomanip>
#include <dirent.h>
#include <sys/stat.h>

using namespace aswell;

static void print_help() {
    std::cout << "Usage: aswell [OPTIONS] [SCRIPT [ARGS...]]\n"
              << "       aswell -c COMMAND [ARGS...]\n"
              << "       aswell config [COMMAND]        settings hub\n"
              << "       aswell theme [COMMAND]         prompt themes\n"
              << "       aswell aliases [COMMAND]       curated alias library\n"
              << "       aswell doctor                  validate config, theme & prompt\n\n"
              << "A modern, beautiful, powerful Unix shell with POSIX compatibility.\n\n"
              << "Options:\n"
              << "  -c COMMAND     Execute command string\n"
              << "  --config PATH  Use PATH as the configuration directory (sets $ASWELL_CONFIG_DIR)\n"
              << "  --no-theme     Disable theme engine and run plain POSIX output\n"
              << "  --safe-mode    Disable external plugins and third-party scripts\n"
              << "  --no-bashrc    Skip importing ~/.bashrc aliases and environment\n"
              << "  --anon        Low-footprint profile: no history file, no config writes, no\n"
              << "                 plugin autoload, no animations, and one process per command\n"
              << "  -j, --jobs N   Default concurrency for `parallel` (also: parallel_jobs setting)\n"
              << "  --version, -v  Print version information\n"
              << "  --help, -h     Print this help message\n\n"
              << "Subcommands:\n"
              << "  config         Settings hub: list, get, set, toggle, unset, help, export, import,\n"
              << "                 show, path, edit (interactive TUI). Try `aswell config help`.\n"
              << "  theme          list | set <name> | preview [name|--all] [--animate] | show |\n"
              << "                 new <name> [--from preset] | reset\n"
              << "  aliases        Curated alias library: list, show, search, preview, install,\n"
              << "                 uninstall — writes ~/.config/aswell/aliases (sourced on start)\n"
              << "  color          Print colored text or inspect palettes\n"
              << "  doctor         Check config.txt, theme, prompt.html, plugins and terminal setup\n"
              << "  reload         Re-read settings, theme and prompt template\n"
              << "  demo           Run engine animation & UI showcase (--auto for headless)\n\n"
              << "Environment:\n"
              << "  ASWELL_CONFIG_DIR  configuration directory (default ~/.config/aswell)\n"
              << "  ASWELL_THEME       overrides the configured theme for one session\n"
              << "  ASWELL_NO_BASHRC=1 skip ~/.bashrc import for this session\n"
              << "  ASWELL_ANON=1       same as --anon\n";
}

int main(int argc, char* argv[]) {
    Environment env;
    JobManager jobs;
    HookManager hooks;
    PluginManager plugins(env, hooks);

    ShellConfig early_cfg = ConfigManager::load();

    std::string command_string;
    std::string script_path;
    std::vector<std::string> script_args;
    std::string custom_config_path;
    bool no_bashrc_flag = false;
    long cli_parallel_jobs = -1;

    // The low-footprint profile can be asked for three ways; the flag wins because
    // it is the most recent word from the caller.
    const char* anon_env = std::getenv("ASWELL_ANON");
    bool anon_flag = (anon_env && std::string(anon_env) == "1");

    // Pre-pass. `aswell --anon config set ...` dispatches the customization hub
    // from inside the parsing loop below, so the profile has to be known before
    // that loop runs or the command would quietly ignore it. --config is picked up
    // here too, so the read-only guard applies to the right directory.
    if (!anon_flag) {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--anon" || a == "--stateless") {
                anon_flag = true;
            } else if (a == "--config" && i + 1 < argc) {
                ::setenv("ASWELL_CONFIG_DIR", argv[i + 1], 1);
            }
        }
    }
    if (anon_flag) ConfigManager::set_read_only(true);

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
        } else if (arg == "--no-bashrc") {
            no_bashrc_flag = true;
        } else if (arg == "--anon" || arg == "--stateless") {
            anon_flag = true;
        } else if (arg == "-j" || arg == "--jobs" || str_util::starts_with(arg, "--jobs=")) {
            // Default concurrency for the `parallel` builtin; beats parallel_jobs
            // from config.txt because a flag on the command line is the newer wish.
            std::string value;
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                value = arg.substr(eq + 1);
            } else if (i + 1 < argc) {
                value = argv[++i];
            } else {
                std::cerr << "aswell: " << arg << ": option requires an argument\n";
                return 2;
            }
            char* end = nullptr;
            long parsed = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str() || parsed < 0) {
                std::cerr << "aswell: " << arg << ": '" << value << "' is not a job count\n";
                return 2;
            }
            cli_parallel_jobs = parsed;
        } else if (arg == "--config" && i + 1 < argc) {
            custom_config_path = argv[++i];
            // Honour the custom directory everywhere, including child processes
            // and the config/theme builtins, via the canonical env override.
            ::setenv("ASWELL_CONFIG_DIR", custom_config_path.c_str(), 1);
            // Environment already snapshotted environ at construction time, so the
            // shell needs its own copy or `$ASWELL_CONFIG_DIR` stays empty inside.
            env.set_var("ASWELL_CONFIG_DIR", custom_config_path, true);
        } else if (arg == "config" || arg == "settings" || arg == "theme" || arg == "themes" ||
                   arg == "aliases" ||
                   arg == "doctor" || arg == "check" || arg == "reload") {
            // The customization hub implements these identically for the CLI and
            // for the in-shell builtin, so behaviour can never diverge.
            std::vector<std::string> hub_args;
            hub_args.push_back("aswell");
            hub_args.push_back((arg == "settings") ? "config" : (arg == "themes" ? "theme" : arg));
            for (int j = i + 1; j < argc; ++j) {
                hub_args.push_back(argv[j]);
            }
            Executor early_exec(env, jobs);
            ControlFlow flow;
            return Builtins::execute("aswell", hub_args, env, jobs, early_exec, flow);
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

    // ---------------------------------------------------------------------
    // Low-footprint profile (`--anon`, ASWELL_ANON=1, or anon_mode=true).
    //
    // The promise is narrow and checkable: this shell keeps no state on disk,
    // loads no third-party code, writes nothing but the output you asked for,
    // and uses one process per command. It is a footprint reduction, not
    // concealment — the profile is always visible in `aswell config list`.
    // ---------------------------------------------------------------------
    early_cfg = ConfigManager::load();   // re-read: --config may have moved it
    env.opt_anon = anon_flag || early_cfg.anon_mode;
    if (env.opt_anon) {
        ConfigManager::set_read_only(true);
        env.set_var("ASWELL_ANON", "1", true);
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

    // Everything the shell applies from config.txt that is not about drawing the
    // prompt: the `parallel` default and the curated alias library. Both the
    // one-shot (-c / script / pipe) path and the interactive path call this, so
    // `aswell -c` behaves like a shell regarding `ll`, `gs` and friends.
    std::vector<std::string> curated_installed;   // what we installed from the library
    bool parallel_jobs_exported = false;          // did we ever own $ASWELL_PARALLEL_JOBS?
    auto apply_runtime_settings = [&](const ShellConfig& next_cfg) {
        const long jobs_wanted = cli_parallel_jobs >= 0 ? cli_parallel_jobs : next_cfg.parallel_jobs;
        // An inherited $ASWELL_PARALLEL_JOBS stays authoritative until the user
        // configures the setting (or passes -j); once they do, we keep the value
        // in sync — including writing 0 to mean "no preference, use the CPU count".
        if (jobs_wanted > 0 || parallel_jobs_exported) {
            env.set_var("ASWELL_PARALLEL_JOBS", std::to_string(jobs_wanted > 0 ? jobs_wanted : 0), true);
        }
        parallel_jobs_exported = jobs_wanted > 0;

        // Undo the previous selection first, so `aswell config set curated_aliases ''`
        // (or a narrower selection) actually removes aliases. Only entries we
        // installed are touched — hand-written aliases are never uninstalled.
        // Only drop entries that are still the library's own definitions: if the
        // user has since written their own version of one of these names, replacing
        // it would silently discard their work.
        std::vector<std::string> stale;
        for (const std::string& n : curated_installed) {
            const AliasDef* def = AliasLibrary::find(n);
            std::string current;
            if (def && env.get_alias(n, current) && current == def->definition) stale.push_back(n);
        }
        AliasLibrary::uninstall(env, AliasLibrary::for_names(stale));
        curated_installed.clear();

        std::string curated = str_util::trim(next_cfg.curated_aliases);
        if (curated.empty() || env.opt_safe_mode) return;
        std::vector<std::string> unknown;
        std::vector<const AliasDef*> defs = AliasLibrary::expand(str_util::split(curated, ','), unknown);
        std::vector<std::string> skipped;
        curated_installed = AliasLibrary::install(env, &executor, defs, false, skipped);
        for (const auto& word : unknown) {
            std::cerr << "aswell: curated_aliases: no category or entry named '" << word
                      << "' — see: aswell aliases list\n";
        }
        // `skipped` is intentionally not reported at startup: an alias the user
        // defined by hand winning over the library is the expected outcome, and
        // `aswell aliases list` shows the same information on demand.
        (void)skipped;
    };

    // 1. Run command string (-c)
    // `aswell -c` and `aswell script.sh` get the alias library too, so `ll`,
    // `gs` and friends behave the same in a one-shot command as in a shell.
    // (Unlike an interactive shell, no ~/.bashrc import here: scripts must stay
    // reproducible, and bash itself does not source rc files for -c either.)
    if (!command_string.empty() || !script_path.empty() || !isatty(STDIN_FILENO)) {
        apply_runtime_settings(early_cfg);
        if (!env.opt_anon) AliasLibrary::source_into(executor);
    }

    if (!command_string.empty()) {
        SignalManager::init_signals(false);
        // Anon profile: `aswell --anon -c 'ls -l'` becomes one process, not two.
        // Only taken when there is nothing for the shell to do afterwards.
        if (env.opt_anon && !env.has_trap(0)) {
            executor.try_exec_direct(command_string);
        }
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
        const std::string script = ss.str();
        // Same one-process shortcut, for a script that is a single command.
        if (env.opt_anon && !env.has_trap(0)) executor.try_exec_direct(script);
        int ret = executor.execute_script(script);
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

    // Startup configuration (colours + bash compatibility import). The full
    // settings application (prompt, theme, editor) happens in apply_config()
    // below so the exact same code path runs on hot reload.
    ShellConfig cfg = ConfigManager::load();
    env.opt_vi_mode = cfg.vi_mode;
    if (cfg.enable_colored_output && !env.opt_no_theme) {
        env.init_color_aliases();
    }

    // Load startup file (~/.aswellrc or ~/.config/aswell/aswellrc)
    // Bash compatibility first: import ~/.bashrc aliases/exports/functions so
    // `ll`, `gs`, custom PATH entries etc. just work. ~/.aswellrc is loaded
    // afterwards and always wins on conflicts.
    if (cfg.import_bashrc && !env.opt_safe_mode && !env.opt_anon && !no_bashrc_flag) {
        const char* no_bash = std::getenv("ASWELL_NO_BASHRC");
        if (!no_bash || std::string(no_bash) != "1") {
            BashCompat::import_bashrc(env, &executor, false);
        }
    }
    // Curated aliases installed by `aswell aliases install` live in a plain
    // shell file the user can edit. Load it after the bashrc import (so the
    // user's own choices win over what we imported) and before ~/.aswellrc
    // (so the rc file always wins over everything).
    AliasLibrary::source_into(executor);

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
    if (!env.opt_anon) {
        // dlopen of third-party code is the loudest thing this shell can do, so the
        // anon profile skips it outright.
        std::string user_plugin_dir = ConfigManager::get_config_dir() + "/plugins";
        plugins.load_plugins_from_directory(user_plugin_dir, env.opt_safe_mode);
    }

    // ---------------------------------------------------------------------
    // Live configuration: one function applies everything the user can tweak
    // (settings, theme stylesheet, prompt template, editor behaviour). It runs
    // at startup and again whenever config.txt/theme.css/prompt.html change on
    // disk (`auto_reload`) or the user asks for it (`aswell reload`, and every
    // `aswell config set` / `aswell theme set`).
    // ---------------------------------------------------------------------
    PromptEngine prompt_engine(env);
    LineEditor editor(env, prompt_engine);
    ConfigWatcher watcher;
    bool auto_reload_enabled = true;
    bool command_banner_enabled = false;
    bool first_apply = true;

    // The prompt engine falls back to ASWELL_USER / ASWELL_HOSTNAME, so the two
    // settings and those exports must always be updated together.
    auto sync_identity_env = [&](const ShellConfig& cfg) {
        if (cfg.custom_username.empty()) env.unset_var("ASWELL_USER");
        else env.set_var("ASWELL_USER", cfg.custom_username, true);
        if (cfg.custom_hostname.empty()) env.unset_var("ASWELL_HOSTNAME");
        else env.set_var("ASWELL_HOSTNAME", cfg.custom_hostname, true);
    };
    sync_identity_env(early_cfg);

    auto apply_config = [&](bool announce) {
        ConfigReport report;
        ShellConfig next_cfg = ConfigManager::load(&report);

        // An explicit ASWELL_THEME always wins for this session, which makes
        // `ASWELL_THEME=nord aswell` and `aswell theme set nord` behave alike.
        std::string theme_override = env.get_var("ASWELL_THEME");
        if (!theme_override.empty()) {
            next_cfg.theme_name = str_util::to_lower(str_util::trim(theme_override));
        }

        auto_reload_enabled = next_cfg.auto_reload && !env.opt_anon;
        // Anon mode wins over every "make it look nicer" knob: a repainting
        // prompt, a post-command ribbon and command animations are all ambient
        // output, which is exactly what this profile exists to avoid.
        command_banner_enabled = next_cfg.enable_command_banner && !env.opt_anon;
        env.opt_vi_mode = next_cfg.vi_mode;

        if (next_cfg.anon_mode != env.opt_anon) {
            // Turning the profile on from inside a session (`aswell config set
            // anon_mode true`) cannot un-load plugins or un-import bashrc, so say so
            // rather than pretending the change took effect.
            env.opt_anon = next_cfg.anon_mode || anon_flag;
            ConfigManager::set_read_only(env.opt_anon);
            std::cerr << "aswell: anon mode " << (env.opt_anon ? "enabled" : "disabled")
                      << " â takes full effect on the next shell; "
                         "aswell config writes are "
                      << (env.opt_anon ? "now read-only" : "enabled again") << "\n";
        }

        // `parallel` default + curated alias library (shared with the one-shot path).
        apply_runtime_settings(next_cfg);

        prompt_engine.set_animations_enabled(next_cfg.enable_animation && !env.opt_anon);
        prompt_engine.set_time_format(next_cfg.time_format);
        prompt_engine.set_date_format(next_cfg.date_format);
        prompt_engine.set_custom_user(next_cfg.custom_username);
        prompt_engine.set_custom_hostname(next_cfg.custom_hostname);
        // PromptEngine falls back to these variables, so clearing the setting has to
        // clear the export too. Otherwise `config unset username` reports success and
        // the prompt keeps showing the old name for the rest of the session.
        sync_identity_env(next_cfg);

        editor.set_autosuggestions(next_cfg.enable_autosuggestions);
        editor.set_syntax_highlighting(next_cfg.enable_syntax_highlighting);
        editor.set_command_animation(next_cfg.enable_command_animation && !env.opt_anon);
        // No history file: the session keeps entries in memory and forgets them.
        editor.history().set_persistent(!env.opt_anon);
        if (next_cfg.history_size > 0) {
            editor.history().set_max_entries(static_cast<size_t>(next_cfg.history_size));
        }
        editor.history().set_ignore_dups(next_cfg.history_ignore_dups);

        if (env.opt_no_theme) {
            prompt_engine.set_template_html("<prompt><text>aswell $ </text></prompt>");
            prompt_engine.set_theme_css("prompt { color: none; }");
        } else {
            std::string config_dir = ConfigManager::get_config_dir();
            std::string theme_warning;
            ThemeInfo theme = ThemeManager::resolve(config_dir, next_cfg.theme_name, &theme_warning);

            // A stylesheet that yields no rules at all (half-typed CSS saved by
            // accident) must never blank out a working prompt: keep the last
            // good one and tell the user what happened.
            StyleSheet incoming = CSSParser::parse(theme.css_content);
            if (incoming.rules().empty() && !prompt_engine.stylesheet().rules().empty()) {
                std::cerr << "aswell: keeping previous theme — " << theme.source
                          << " produced no style rules\n";
            } else {
                prompt_engine.set_theme_css(theme.css_content);
            }
            if (!theme_warning.empty()) {
                std::cerr << "aswell: " << theme_warning << "\n";
            }

            const std::string html_path = config_dir + "/prompt.html";
            std::string html;
            bool html_exists = false;
            if (std::ifstream html_f{html_path}) {
                std::stringstream hs;
                hs << html_f.rdbuf();
                html = hs.str();
                html_exists = true;
            }
            // Same hazard as the stylesheet above: an empty or markup-less file
            // parses to nothing and would leave the user with no prompt at all.
            if (html_exists && (html.find('<') == std::string::npos)) {
                std::cerr << "aswell: keeping previous prompt â " << html_path
                          << (html.empty() ? " is empty" : " contains no markup") << "\n";
            } else {
                prompt_engine.set_template_html(html_exists ? html : ConfigManager::build_template(next_cfg));
            }
        }

        // Watch the customization files for the next iteration of the loop.
        watcher.clear();
        std::string config_dir = ConfigManager::get_config_dir();
        // Stat-polling four files before every prompt is ambient work; the anon
        // profile accepts that edits are picked up on the next start instead.
        if (!env.opt_anon) {
        watcher.watch(config_dir + "/config.txt");
        watcher.watch(config_dir + "/theme.css");
        watcher.watch(config_dir + "/prompt.html");
        watcher.watch(config_dir + "/themes/" + next_cfg.theme_name + ".css");
        }
        watcher.snapshot();

        // Typos in config.txt used to vanish silently. Report them, but stay
        // quiet afterwards unless the user asked for a reload.
        if (!report.issues.empty() && (first_apply || announce)) {
            for (size_t i = 0; i < report.issues.size() && i < 4; ++i) {
                const ConfigReport::Issue& issue = report.issues[i];
                std::cerr << "aswell: " << config_dir << "/config.txt:" << issue.line << ": "
                          << issue.key << " " << issue.message;
                if (!issue.suggestion.empty()) std::cerr << " (" << issue.suggestion << ")";
                std::cerr << "\n";
            }
            if (report.issues.size() > 4) {
                std::cerr << "aswell: " << (report.issues.size() - 4)
                          << " more config issue(s) — run `aswell doctor`\n";
            }
        }

        if (announce) {
            std::cout << "\033[90maswell: reloaded settings, theme '\033[0m\033[1m" << next_cfg.theme_name
                      << "\033[0m\033[90m' and prompt template\033[0m\n";
        }
    };

    // `aswell config set ...`, `aswell theme set ...` and `aswell reload` call
    // this to apply their changes to the running prompt immediately.
    env.config_reload = [&apply_config](bool announce) { apply_config(announce); };
    apply_config(false);
    first_apply = false;

    hooks.trigger_hook(HookType::ON_START);

    std::string last_pwd = env.get_var("PWD");

    // Main Interactive REPL loop
    while (true) {
        std::string cur_pwd = env.get_var("PWD");
        if (cur_pwd != last_pwd) {
            hooks.trigger_hook(HookType::ON_DIR_CHANGE, {cur_pwd});
            last_pwd = cur_pwd;
        }

        // Live customization: if config.txt, the active theme stylesheet or
        // prompt.html changed on disk, re-apply them before drawing the prompt.
        if (auto_reload_enabled && watcher.changed()) {
            std::vector<std::string> changed = watcher.changed_files();
            apply_config(false);
            if (!changed.empty()) {
                std::string names;
                for (size_t i = 0; i < changed.size(); ++i) {
                    const std::string& file = changed[i];
                    size_t slash = file.find_last_of('/');
                    if (i) names += ", ";
                    names += file.substr(slash == std::string::npos ? 0 : slash + 1);
                }
                std::cout << "\033[90maswell: reloaded " << names << "\033[0m\n";
            }
        }

        hooks.trigger_hook(HookType::ON_PROMPT);

        jobs.update_status();
        jobs.notify_finished();
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

        // Optional execution ribbon (off by default; config.txt `command_banner=true`).
        if (command_banner_enabled) {
            double ms = executor.get_last_command_duration_ms();
            std::string label = line;
            if (label.size() > 52) label = label.substr(0, 49) + "...";
            std::string head = (status == 0)
                ? std::string("\033[1;32m\xe2\x9c\x93\033[0m")
                : ("\033[1;31m\xe2\x9c\x97 " + std::to_string(status) + "\033[0m");
            std::cout << "\033[90m  \xe2\x8c\x9c\xe2\x8c\x90 \033[0m" << head << " \033[90m"
                      << label << "  " << std::fixed << std::setprecision(1) << ms << "ms\033[0m "
                      << "\033[90m\xe2\x8c\x90\xe2\x8c\x8c\033[0m\n";
        }
    }

    if (env.has_trap(0)) executor.execute_string(env.get_trap(0));
    hooks.trigger_hook(HookType::ON_EXIT, {std::to_string(env.last_exit_status)});
    return env.last_exit_status;
}
