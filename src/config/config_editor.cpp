#include "aswell/ui/animation.hpp"
#include "aswell/config/config_editor.hpp"
#include "aswell/ui/terminal.hpp"

namespace aswell {

int ConfigEditor::run_interactive(Environment& env) {
    RawModeGuard raw_guard;
    ShellConfig cfg = ConfigManager::load();
    auto themes = ThemeManager::get_builtin_theme_names();

    size_t cur_theme_idx = 0;
    for (size_t i = 0; i < themes.size(); ++i) {
        if (themes[i] == cfg.theme_name) {
            cur_theme_idx = i;
            break;
        }
    }

    int selected_item = 0;
    const int total_items = 11; // 7 components + theme + anim + hl + autosugg

    PromptEngine preview_prompt(env);

    while (true) {
        Terminal::clear_screen();

        // Update preview prompt engine
        ThemeInfo tinfo = ThemeManager::get_theme(themes[cur_theme_idx]);
        preview_prompt.set_theme_css(tinfo.css_content);
        preview_prompt.set_template_html(ConfigManager::build_template(cfg));

        std::cout << "\033[1;36m╭─────────────────────────────────────────────────────────────╮\033[0m\n";
        std::cout << "\033[1;36m│\033[0m             \033[1;37mASWELL SHELL CUSTOMIZATION EDITOR\033[0m               \033[1;36m│\033[0m\n";
        std::cout << "\033[1;36m╰─────────────────────────────────────────────────────────────╯\033[0m\n\n";

        std::cout << "  \033[1;33mPrompt Components:\033[0m\n";

        auto render_toggle = [&](int idx, const std::string& name, bool enabled) {
            if (selected_item == idx) std::cout << "  \033[1;32m➜ \033[0m";
            else std::cout << "    ";
            std::cout << (enabled ? "\033[1;32m[✓]\033[0m " : "\033[90m[ ]\033[0m ") << name << "\n";
        };

        render_toggle(0, "Username", cfg.show_username);
        render_toggle(1, "Hostname", cfg.show_hostname);
        render_toggle(2, "Directory", cfg.show_directory);
        render_toggle(3, "Git Status", cfg.show_git);
        render_toggle(4, "Command Execution Duration", cfg.show_runtime);
        render_toggle(5, "Active Jobs Count", cfg.show_jobs);
        render_toggle(6, "Exit Status Code", cfg.show_status);

        std::cout << "\n  \033[1;33mAppearance & Features:\033[0m\n";

        // Theme selector
        if (selected_item == 7) std::cout << "  \033[1;32m➜ \033[0m";
        else std::cout << "    ";
        std::cout << "Theme: \033[1;35m< " << themes[cur_theme_idx] << " >\033[0m (" << tinfo.description << ")\n";

        render_toggle(8, "Prompt Animations", cfg.enable_animation);
        render_toggle(9, "Real-time Syntax Highlighting", cfg.enable_syntax_highlighting);
        render_toggle(10, "History Autosuggestions", cfg.enable_autosuggestions);

        // Render Live Preview
        std::cout << "\n  \033[1;33mLive Prompt Preview:\033[0m\n";
        std::cout << "  \033[90m─────────────────────────────────────────────────────────────\033[0m\n";

        PromptContext ctx = preview_prompt.gather_context(142.0, 1, false);
        RenderResult pr = preview_prompt.render(ctx, AnimationEngine::now_ms());
        std::cout << "  " << pr.ansi_output << "ls -la\n";

        std::cout << "  \033[90m─────────────────────────────────────────────────────────────\033[0m\n\n";
        std::cout << "  [\033[1;32mS\033[0m] Save Configuration    [\033[1;31mQ\033[0m] Quit without saving    [\033[1;36m↑/↓\033[0m] Navigate    [\033[1;36mSpace\033[0m] Toggle\n";
        std::cout.flush();

        KeyEvent ev = Terminal::read_key();
        if (ev.key == Key::UP) {
            selected_item = (selected_item - 1 + total_items) % total_items;
        } else if (ev.key == Key::DOWN) {
            selected_item = (selected_item + 1) % total_items;
        } else if (ev.key == Key::LEFT) {
            if (selected_item == 7) {
                cur_theme_idx = (cur_theme_idx - 1 + themes.size()) % themes.size();
                cfg.theme_name = themes[cur_theme_idx];
            }
        } else if (ev.key == Key::RIGHT) {
            if (selected_item == 7) {
                cur_theme_idx = (cur_theme_idx + 1) % themes.size();
                cfg.theme_name = themes[cur_theme_idx];
            }
        } else if (ev.key == Key::CHAR) {
            char c = ev.ch.empty() ? 0 : ev.ch[0];
            if (c == ' ') {
                switch (selected_item) {
                    case 0: cfg.show_username = !cfg.show_username; break;
                    case 1: cfg.show_hostname = !cfg.show_hostname; break;
                    case 2: cfg.show_directory = !cfg.show_directory; break;
                    case 3: cfg.show_git = !cfg.show_git; break;
                    case 4: cfg.show_runtime = !cfg.show_runtime; break;
                    case 5: cfg.show_jobs = !cfg.show_jobs; break;
                    case 6: cfg.show_status = !cfg.show_status; break;
                    case 7:
                        cur_theme_idx = (cur_theme_idx + 1) % themes.size();
                        cfg.theme_name = themes[cur_theme_idx];
                        break;
                    case 8: cfg.enable_animation = !cfg.enable_animation; break;
                    case 9: cfg.enable_syntax_highlighting = !cfg.enable_syntax_highlighting; break;
                    case 10: cfg.enable_autosuggestions = !cfg.enable_autosuggestions; break;
                }
            } else if (c == 's' || c == 'S') {
                ConfigManager::save(cfg);
                Terminal::clear_screen();
                std::cout << "\033[1;32mConfiguration saved successfully to ~/.config/aswell/\033[0m\n";
                return 0;
            } else if (c == 'q' || c == 'Q') {
                Terminal::clear_screen();
                std::cout << "Configuration discarded.\n";
                return 0;
            }
        } else if (ev.key == Key::ESC) {
            Terminal::clear_screen();
            return 0;
        }
    }
}

} // namespace aswell
