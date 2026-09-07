#include "aswell/shell/builtins.hpp"
#include "aswell/config/config.hpp"
#include "aswell/ui/template_engine.hpp"
#include "aswell/ui/terminal.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <dirent.h>
#include <sys/stat.h>

namespace aswell {

int Builtins::builtin_aswell(const std::vector<std::string>& args, Environment& env, Executor& /*executor*/) {
    if (args.size() <= 1) {
        std::cout << SHELL_BANNER << "\n"
                  << "Type 'aswell help' for commands, 'aswell color' for colored output, or 'aswell theme' to configure themes.\n";
        return 0;
    }

    const std::string& sub = args[1];
    if (sub == "version" || sub == "--version") {
        std::cout << "aswell version " << SHELL_VERSION << " (POSIX Compatible Shell)\n";
        return 0;
    }
    if (sub == "help" || sub == "--help") {
        builtin_help(args);
        return 0;
    }
    if (sub == "color") {
        std::vector<std::string> sub_args;
        sub_args.push_back("color");
        for (size_t i = 2; i < args.size(); ++i) {
            sub_args.push_back(args[i]);
        }
        return builtin_color(sub_args, env);
    }
    if (sub == "theme") {
        if (args.size() == 2 || (args.size() == 3 && args[2] == "list")) {
            std::cout << "\033[1;34mAvailable Aswell Themes:\033[0m\n"
                      << "  * \033[1;32mmodern\033[0m     - Clean, elegant unicode boxes, branch glyphs, subtle colors\n"
                      << "  * \033[1;35mcyberpunk\033[0m  - Neon cyan/magenta glowing accents, sharp brackets\n"
                      << "  * \033[1;36mnord\033[0m       - Arctic blue, frost, muted Scandinavian aesthetic\n"
                      << "  * \033[1;37mminimal\033[0m    - Monochrome single-character prompt, blazing speed\n"
                      << "  * \033[1;31mdracula\033[0m    - Vampire dark theme with purple and emerald highlights\n"
                      << "  * \033[1;33mpowerline\033[0m  - Segmented status arrows with contrasting backgrounds\n";
            return 0;
        }
        // Support: aswell theme set <name>
        if (args.size() >= 4 && args[2] == "set") {
            env.set_var("ASWELL_THEME", args[3], true);
            std::cout << "Aswell theme changed to: " << args[3] << "\n";
            return 0;
        }
        // Support: aswell theme <name>  (without "set" keyword)
        if (args.size() >= 3) {
            env.set_var("ASWELL_THEME", args[2], true);
            std::cout << "Aswell theme changed to: " << args[2] << "\n";
            return 0;
        }
    }
    if (sub == "config") {
        if (args.size() == 2 || (args.size() >= 3 && args[2] == "list")) {
            std::cout << "\033[1;34mAswell Configuration:\033[0m\n"
                      << "  * \033[1;32mtheme\033[0m    - Switch or list visual themes\n"
                      << "  * \033[1;32mcustom\033[0m   - Manage custom commands\n"
                      << "  * \033[1;32mtemplate\033[0m - Manage prompt templates\n"
                      << "  * \033[1;32mhooks\033[0m    - Manage dynamic script hooks\n"
                      << "\nUsage: aswell config <subcommand>\n"
                      << "Run 'aswell config list' to see available options.\n";
            return 0;
        }
        if (args.size() >= 3 && args[2] == "path") {
            std::cout << aswell::ConfigManager::get_config_dir() << "\n";
            return 0;
        }
        if (args.size() >= 3 && args[2] == "theme") {
            if (args.size() >= 4) {
                // Either "aswell config theme set <name>" or "aswell config theme <name>"
                if (args[3] == "set" && args.size() >= 5) {
                    env.set_var("ASWELL_THEME", args[4], true);
                    std::cout << "Aswell theme changed to: " << args[4] << "\n";
                } else {
                    // "aswell config theme <name>" or "aswell config theme set <name>" with single arg
                    env.set_var("ASWELL_THEME", args[3], true);
                    std::cout << "Aswell theme changed to: " << args[3] << "\n";
                }
            } else {
                // "aswell config theme" - show current theme
                std::cout << "Current theme: " << env.get_var("ASWELL_THEME") << "\n";
            }
            return 0;
        }
    }
    if (sub == "custom") {
        const char* home_env = std::getenv("HOME");
        std::string cmd_dir = (home_env ? std::string(home_env) : "/root") + "/.config/aswell/commands";
        fs_util::mkdir_p(cmd_dir);

        if (args.size() == 2 || (args.size() >= 3 && args[2] == "list")) {
            std::cout << "\033[1;34mCustom Commands (~/.config/aswell/commands/):\033[0m\n";
            DIR* d = opendir(cmd_dir.c_str());
            bool found = false;
            if (d) {
                struct dirent* entry;
                while ((entry = readdir(d)) != nullptr) {
                    if (entry->d_name[0] != '.') {
                        std::cout << "  * \033[1;32m" << entry->d_name << "\033[0m\n";
                        found = true;
                    }
                }
                closedir(d);
            }
            if (!found) {
                std::cout << "  (none yet - add with: aswell custom add <name> <command>)\n";
            }
            return 0;
        }
        if (args.size() >= 4 && args[2] == "add") {
            std::string name = args[3];
            std::string path = cmd_dir + "/" + name;
            std::ofstream f(path);
            if (!f) {
                std::cerr << "aswell: failed to create custom command " << path << "\n";
                return 1;
            }
            std::string shebang = "#!/bin/sh\n";
            if (access("/data/data/com.termux/files/usr/bin/bash", X_OK) == 0) {
                shebang = "#!/data/data/com.termux/files/usr/bin/bash\n";
            } else if (access("/data/data/com.termux/files/usr/bin/sh", X_OK) == 0) {
                shebang = "#!/data/data/com.termux/files/usr/bin/sh\n";
            }
            f << shebang;
            for (size_t i = 4; i < args.size(); ++i) {
                f << args[i] << (i + 1 < args.size() ? " " : "");
            }
            f << "\n";
            f.close();
            chmod(path.c_str(), 0755);
            std::cout << "Created custom command '\033[1;32m" << name << "\033[0m' in " << path << "\n";
            return 0;
        }
        if (args.size() >= 3 && args[2] == "path") {
            std::cout << cmd_dir << "\n";
            return 0;
        }
        std::cout << "Usage: aswell custom [list | add <name> <script...> | path]\n";
        return 0;
    }
    if (sub == "hooks" || sub == "hook") {
        std::cout << "\033[1;34mAswell Dynamic Script Hooks:\033[0m\n"
                  << "  * \033[1;36maswell_on_prompt\033[0m     - Triggered before prompt render (or 'precmd')\n"
                  << "  * \033[1;36maswell_before_command\033[0m - Triggered before execution (or 'preexec') with $1=command\n"
                  << "  * \033[1;36maswell_after_command\033[0m  - Triggered after execution with $1=cmd, $2=ms, $3=status\n"
                  << "  * \033[1;36maswell_on_error\033[0m      - Triggered on non-zero exit with $1=cmd, $2=status\n"
                  << "  * \033[1;36maswell_on_dir_change\033[0m - Triggered on cd/dir change with $1=new_dir\n"
                  << "  * \033[1;36maswell_on_exit\033[0m       - Triggered when shell exits\n";
        return 0;
    }
    if (sub == "template" || sub == "tmpl") {
        TemplateEngine::ensure_default_templates();
        std::string tdir = TemplateEngine::get_templates_dir();

        if (args.size() == 2 || (args.size() >= 3 && args[2] == "list")) {
            std::cout << "\033[1;34mAswell Templates Directory (~/.config/aswell/templates/):\033[0m\n";
            DIR* d = opendir(tdir.c_str());
            if (d) {
                struct dirent* ent;
                while ((ent = readdir(d)) != nullptr) {
                    if (ent->d_name[0] != '.') {
                        std::cout << "  * \033[1;32m" << ent->d_name << "\033[0m\n";
                    }
                }
                closedir(d);
            }
            std::cout << "Run: aswell template render <file> [var=val...]\n"
                      << "     aswell template eval '<markup>'\n";
            return 0;
        }
        if (args.size() >= 4 && args[2] == "eval") {
            TemplateContext ctx;
            for (size_t i = 4; i < args.size(); ++i) {
                size_t eq = args[i].find('=');
                if (eq != std::string::npos) {
                    ctx.set(args[i].substr(0, eq), args[i].substr(eq + 1));
                }
            }
            std::cout << TemplateEngine::render(args[3], ctx) << "\n";
            return 0;
        }
        if (args.size() >= 4 && args[2] == "render") {
            std::string path = args[3];
            if (path.find('/') == std::string::npos) {
                path = tdir + "/" + path;
            }
            TemplateContext ctx;
            for (size_t i = 4; i < args.size(); ++i) {
                size_t eq = args[i].find('=');
                if (eq != std::string::npos) {
                    ctx.set(args[i].substr(0, eq), args[i].substr(eq + 1));
                }
            }
            std::cout << TemplateEngine::render_file(path, ctx) << "\n";
            return 0;
        }
        if (args.size() >= 3 && (args[2] == "fire" || args[2] == "test")) {
            std::string target = (args.size() >= 4) ? args[3] : "sample.txt";
            TemplateContext ctx;
            ctx.set("target", target);
            std::string fire_file = tdir + "/fire.html";
            std::ifstream f(fire_file);
            std::string tmpl_src = f ? std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) : "";
            bool headless = !Terminal::is_interactive_tty();
            TemplateEngine::play_animation(tmpl_src, ctx, headless);
            return 0;
        }
        std::cout << "Usage: aswell template [list | eval <markup> | render <file> | fire [target]]\n";
        return 0;
    }
    if (sub == "event" || sub == "events" || sub == "fire") {
        TemplateEngine::ensure_default_templates();
        std::string tdir = TemplateEngine::get_templates_dir();
        if (sub == "fire" || (args.size() >= 3 && args[2] == "fire")) {
            std::string target = (sub == "fire" && args.size() >= 3) ? args[2] : ((args.size() >= 4) ? args[3] : "sample_file.txt");
            TemplateContext ctx;
            ctx.set("target", target);
            std::string fire_file = tdir + "/fire.html";
            std::ifstream f(fire_file);
            std::string tmpl_src = f ? std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) : "";
            bool headless = (args.size() >= 4 && args.back() == "--headless") || !Terminal::is_interactive_tty();
            TemplateEngine::play_animation(tmpl_src, ctx, headless);
            return 0;
        }
        if (args.size() >= 3 && args[2] == "test") {
            TemplateContext ctx;
            ctx.set("target", "test_artifact.log");
            std::string fire_file = tdir + "/fire.html";
            std::ifstream f(fire_file);
            std::string tmpl_src = f ? std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) : "";
            TemplateEngine::play_animation(tmpl_src, ctx, true);
            return 0;
        }
        std::cout << "\033[1;34mAswell Declarative Template Event Subsystem:\033[0m\n"
                  << "  * \033[1;32mEvent Template\033[0m  : ~/.config/aswell/templates/events.html\n"
                  << "  * \033[1;32mFILE_REMOVE\033[0m     : Triggered on rm / unlink — executed purely via TemplateEngine\n"
                  << "  * \033[1;32mTest trigger\033[0m    : aswell fire <target>  or  aswell template fire <target>\n"
                  << "Usage: aswell event [list | fire <target> | test]\n";
        return 0;
    }
    if (sub == "ui") {
        std::cout << "\033[1;34mAswell Modern UI Status:\033[0m\n"
                  << "  * \033[1;32mPrompt Engine\033[0m    : HTML & CSS rendered\n"
                  << "  * \033[1;32mColor Engine\033[0m     : 24-bit TrueColor, palettes, gradient, rainbow, and 'color' command\n"
                  << "  * \033[1;32mCustom Commands\033[0m  : First-class (~/.config/aswell/commands/)\n"
                  << "  * \033[1;32mRight Prompt\033[0m     : Supported via <rprompt> with column alignment\n"
                  << "  * \033[1;32mAutocomplete\033[0m     : Modern popup cards [CUSTOM, CMD, DIR, BUILT, ALIAS, FUNC]\n"
                  << "  * \033[1;32mReactive DOM\033[0m     : $VAR expansion + 'show-if' / 'hide-if' conditional rendering\n";
        return 0;
    }

    std::cout << "Unknown aswell command: " << sub << ". Run 'aswell help' for options.\n";
    return 1;
}

} // namespace aswell
