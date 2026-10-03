#include "aswell/ui/animation.hpp"
#include "aswell/config/config_editor.hpp"
#include "aswell/config/settings.hpp"
#include "aswell/ui/terminal.hpp"
#include <iostream>

namespace aswell {

namespace {

// The interactive editor renders one row per registered setting, so every new
// entry in SettingsRegistry shows up here without touching this file.
//
//   \u2191/\u2193 or j/k   navigate          Space / x / t   toggle booleans
//   \u2190/\u2192          cycle enum values (themes)      Enter         edit text & numbers
//   a               show advanced      d               restore defaults
//   s               save + quit        q / Esc         quit without saving
struct MenuItem {
    const SettingDef* def = nullptr;
    bool is_theme = false;
};

std::string pad(const std::string& s, size_t width) {
    size_t len = str_util::visual_width(s);
    if (len >= width) return s;
    return s + std::string(width - len, ' ');
}

std::vector<MenuItem> build_items(const ShellConfig& cfg, bool show_advanced) {
    std::vector<MenuItem> items;
    for (const auto& def : SettingsRegistry::all()) {
        if (def.advanced && !show_advanced) continue;
        MenuItem item;
        item.def = &def;
        item.is_theme = (def.type == SettingType::ENUM);
        items.push_back(item);
    }
    (void)cfg;
    return items;
}

// Blocking inline editor for string / integer rows. Returns false on Escape.
bool edit_value(const std::string& title, std::string& value, bool digits_only) {
    std::string buffer = value;
    while (true) {
        std::cout << "\r\033[K";
        std::cout << "  \033[1;33m" << title << "\033[0m = " << buffer;
        if (buffer.empty()) std::cout << "\033[90m(empty)\033[0m";
        std::cout << "  \033[90m[Enter: accept  Esc: cancel  Backspace: delete]\033[0m";
        std::cout.flush();

        KeyEvent ev = Terminal::read_key();
        if (ev.key == Key::ENTER) {
            value = buffer;
            return true;
        }
        if (ev.key == Key::ESC) return false;
        if (ev.key == Key::BACKSPACE) {
            if (!buffer.empty()) buffer.pop_back();
            continue;
        }
        if (ev.key == Key::CTRL_U || ev.key == Key::CTRL_W) {
            buffer.clear();
            continue;
        }
        if (ev.key == Key::CHAR) {
            for (char c : ev.ch) {
                if (digits_only) {
                    if (c >= '0' && c <= '9') buffer += c;
                } else if (c >= 32 && c != 127) {
                    buffer += c;
                }
            }
        }
    }
}

} // namespace

int ConfigEditor::run_interactive(Environment& env) {
    RawModeGuard raw_guard;
    ShellConfig cfg = ConfigManager::load();

    std::string config_dir = ConfigManager::get_config_dir();
    std::vector<std::string> themes = ThemeManager::theme_names(config_dir);
    if (themes.empty()) themes = ThemeManager::get_builtin_theme_names();

    bool show_advanced = false;
    std::vector<MenuItem> items = build_items(cfg, show_advanced);
    size_t selected_item = 0;
    bool dirty = false;

    PromptEngine preview_prompt(env);
    if (!cfg.custom_username.empty()) preview_prompt.set_custom_user(cfg.custom_username);
    if (!cfg.custom_hostname.empty()) preview_prompt.set_custom_hostname(cfg.custom_hostname);

    auto selected_def = [&]() -> const SettingDef* {
        if (selected_item >= items.size()) return nullptr;
        return items[selected_item].def;
    };

    while (true) {
        items = build_items(cfg, show_advanced);
        if (selected_item >= items.size()) selected_item = items.empty() ? 0 : items.size() - 1;

        Terminal::clear_screen();

        // Rebuild the live preview from the current in-memory config.
        std::string theme_warning;
        ThemeInfo tinfo = ThemeManager::resolve(config_dir, cfg.theme_name, &theme_warning);
        preview_prompt.set_animations_enabled(cfg.enable_animation);
        preview_prompt.set_time_format(cfg.time_format);
        preview_prompt.set_theme_css(tinfo.css_content);
        preview_prompt.set_template_html(ConfigManager::build_template(cfg));

        size_t modified = 0;
        for (const auto& def : SettingsRegistry::all()) {
            if (SettingsRegistry::is_modified(def, cfg)) modified++;
        }

        std::cout << "\033[1;36m\u256d";
        for (int i = 0; i < 63; ++i) std::cout << "\u2500";
        std::cout << "\u256e\033[0m\n";
        std::cout << "\033[1;36m\u2502\033[0m          \033[1;37mASWELL CUSTOMIZATION EDITOR\033[0m           \033[1;36m\u2502\033[0m\n";
        std::cout << "\033[1;36m\u2570";
        for (int i = 0; i < 63; ++i) std::cout << "\u2500";
        std::cout << "\u256f\033[0m\n\n";

        std::cout << "  \033[1;33mSettings\033[0m \033[90m(" << items.size() << " row"
                  << (items.size() == 1 ? "" : "s") << ", " << modified << " changed"
                  << (show_advanced ? ", advanced visible" : "") << ")\033[0m\n";

        const char* current_category = "";
        for (size_t idx = 0; idx < items.size(); ++idx) {
            const SettingDef& def = *items[idx].def;
            if (def.category != current_category) {
                current_category = def.category.c_str();
                std::cout << "\n  \033[1;34m" << def.category << "\033[0m\n";
            }

            std::string value = def.get(cfg);
            bool changed = SettingsRegistry::is_modified(def, cfg);
            std::string label = def.key;
            std::string rendered = value.empty() ? "\033[90m(default)\033[0m" : value;

            if (def.type == SettingType::BOOL) {
                std::cout << (idx == selected_item ? "  \033[1;32m\u279c \033[0m" : "    ");
                std::cout << (value == "true" ? "\033[1;32m[\u2713]\033[0m " : "\033[90m[ ]\033[0m ");
                std::cout << pad(label, 24);
                std::cout << "\033[90m" << def.description << "\033[0m";
                if (changed) std::cout << "  \033[1;33m*\033[0m";
                std::cout << "\n";
            } else if (def.type == SettingType::ENUM) {
                std::cout << (idx == selected_item ? "  \033[1;32m\u279c \033[0m" : "    ");
                std::cout << pad(label, 24) << "\033[1;35m< " << rendered << " >\033[0m";
                std::cout << "\033[90m " << tinfo.description << "\033[0m";
                if (changed) std::cout << "  \033[1;33m*\033[0m";
                std::cout << "\n";
                if (idx == selected_item) {
                    std::string row = "      ";
                    for (const auto& name : themes) {
                        row += (name == cfg.theme_name) ? "\033[1;32m" + name + "\033[0m  " : "\033[90m" + name + "\033[0m  ";
                    }
                    std::cout << row << "\n";
                }
            } else {
                std::cout << (idx == selected_item ? "  \033[1;32m\u279c \033[0m" : "    ");
                std::cout << pad(label, 24) << rendered;
                if (def.type == SettingType::INT && !def.units.empty()) {
                    std::cout << "\033[90m " << def.units << "\033[0m";
                }
                std::cout << "  \033[90m" << def.description << "\033[0m";
                if (changed) std::cout << "  \033[1;33m*\033[0m";
                std::cout << "\n";
            }
        }

        std::cout << "\n  \033[1;33mLive prompt preview\033[0m\n";
        std::cout << "  \033[90m\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\033[0m\n";

        PromptContext ctx = preview_prompt.gather_context(142.0, 1, cfg.vi_mode);
        RenderResult pr = preview_prompt.render(ctx, cfg.enable_animation ? AnimationEngine::now_ms() : 0);
        std::cout << "  " << pr.ansi_output;
        // Keep the fake command on the prompt's own input line.
        std::cout << "ls -la\n";
        std::cout << "  \033[90m\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\033[0m\n\n";

        if (dirty) {
            std::cout << "  \033[1;33munsaved changes\033[0m  ";
        }
        std::cout << "  [\033[1;32ms\033[0m] save  [\033[1;31mq\033[0m] quit  [\033[1;36mx\033[0m] toggle  "
                     "[\033[1;36m\u2190/\u2192\033[0m] cycle  [\033[1;36mEnter\033[0m] edit  "
                     "[\033[1;36ma\033[0m] advanced  [\033[1;36md\033[0m] defaults\n";
        std::cout << "  \033[90msaves to " << config_dir << "/config.txt\033[0m\n";
        std::cout.flush();

        KeyEvent ev = Terminal::read_key();
        const SettingDef* def = selected_def();

        if (ev.key == Key::UP || (ev.key == Key::CHAR && ev.ch == "k")) {
            if (!items.empty()) selected_item = (selected_item + items.size() - 1) % items.size();
            continue;
        }
        if (ev.key == Key::DOWN || (ev.key == Key::CHAR && ev.ch == "j")) {
            if (!items.empty()) selected_item = (selected_item + 1) % items.size();
            continue;
        }
        if (ev.key == Key::PAGE_UP && !items.empty()) {
            selected_item = (selected_item + items.size() - 5) % items.size();
            continue;
        }
        if (ev.key == Key::PAGE_DOWN && !items.empty()) {
            selected_item = (selected_item + 5) % items.size();
            continue;
        }

        if ((ev.key == Key::LEFT || ev.key == Key::RIGHT) && def &&
            (def->type == SettingType::ENUM || def->type == SettingType::BOOL)) {
            int dir = (ev.key == Key::RIGHT) ? 1 : -1;
            if (def->type == SettingType::ENUM) {
                std::vector<std::string> options = SettingsRegistry::choices(*def);
                if (!options.empty()) {
                    std::string current = str_util::to_lower(def->get(cfg));
                    size_t pos = 0;
                    for (size_t i = 0; i < options.size(); ++i) {
                        if (str_util::to_lower(options[i]) == current) pos = i;
                    }
                    pos = (pos + options.size() + static_cast<size_t>(dir)) % options.size();
                    def->set(cfg, options[pos]);
                    dirty = true;
                }
            } else {
                def->set(cfg, def->get(cfg) == "true" ? "false" : "true");
                dirty = true;
            }
            continue;
        }

        if (ev.key == Key::ENTER && def) {
            if (def->type == SettingType::STRING || def->type == SettingType::INT) {
                std::string value = def->get(cfg);
                if (edit_value(def->key, value, def->type == SettingType::INT)) {
                    std::string err;
                    if (SettingsRegistry::apply(cfg, def->key, value, err)) dirty = true;
                }
            } else if (def->type == SettingType::BOOL) {
                def->set(cfg, def->get(cfg) == "true" ? "false" : "true");
                dirty = true;
            } else if (def->type == SettingType::ENUM) {
                std::vector<std::string> options = SettingsRegistry::choices(*def);
                if (!options.empty()) {
                    std::string current = str_util::to_lower(def->get(cfg));
                    size_t pos = 0;
                    for (size_t i = 0; i < options.size(); ++i) {
                        if (str_util::to_lower(options[i]) == current) pos = i;
                    }
                    def->set(cfg, options[(pos + 1) % options.size()]);
                    dirty = true;
                }
            }
            continue;
        }

        if (ev.key != Key::CHAR) {
            if (ev.key == Key::ESC) {
                Terminal::clear_screen();
                if (dirty) std::cout << "Discarded unsaved changes.\n";
                return 0;
            }
            continue;
        }

        char c = ev.ch.empty() ? 0 : ev.ch[0];
        if (c == ' ') {
            if (def) {
                if (def->type == SettingType::BOOL) {
                    def->set(cfg, def->get(cfg) == "true" ? "false" : "true");
                    dirty = true;
                } else if (def->type == SettingType::INT) {
                    long long step = def->max_value > 10000 ? 1000 : 10;
                    long long current = std::atoll(def->get(cfg).c_str());
                    std::string next = std::to_string(current + step);
                    std::string err;
                    if (SettingsRegistry::apply(cfg, def->key, next, err)) dirty = true;
                } else if (def->type == SettingType::ENUM) {
                    std::vector<std::string> options = SettingsRegistry::choices(*def);
                    if (!options.empty()) {
                        std::string current = str_util::to_lower(def->get(cfg));
                        size_t pos = 0;
                        for (size_t i = 0; i < options.size(); ++i) {
                            if (str_util::to_lower(options[i]) == current) pos = i;
                        }
                        def->set(cfg, options[(pos + 1) % options.size()]);
                        dirty = true;
                    }
                }
            }
        } else if (c == 'x' || c == 't' || c == 'y' || c == 'n') {
            if (def && def->type == SettingType::BOOL) {
                bool target = (c == 'y');
                if (c == 'x' || c == 't') target = (def->get(cfg) != "true");
                def->set(cfg, target ? "true" : "false");
                dirty = true;
            }
        } else if (c == 'a') {
            show_advanced = !show_advanced;
        } else if (c == 'd') {
            ShellConfig fresh;
            cfg = fresh;
            dirty = true;
        } else if (c == 's' || c == 'S') {
            ConfigManager::save(cfg);
            Terminal::clear_screen();
            std::cout << "\033[1;32m\xe2\x9c\x93 Saved to " << config_dir
                      << "/config.txt\033[0m\n";
            std::cout << "\033[90m  Live themes: aswell theme list | Validate: aswell doctor\033[0m\n";
            return 0;
        } else if (c == 'q' || c == 'Q') {
            Terminal::clear_screen();
            std::cout << (dirty ? "\033[1;33mLeft without saving.\033[0m\n" : "");
            return 0;
        } else if (c == 'h' || c == '?') {
            Terminal::clear_screen();
            std::cout << "aswell config editor\n\n"
                      << "  \xe2\x86\x91/\xe2\x86\x93 (j/k)   move          Space / x   toggle the highlighted switch\n"
                      << "  \xe2\x86\x90/\xe2\x86\x92       cycle value     Enter      edit text and numbers\n"
                      << "  a             advanced rows  d          restore all defaults\n"
                      << "  s             save & quit    q / Esc    quit (discards)\n\n"
                      << "Every row here is a key of ~/.config/aswell/config.txt, so you can\n"
                      << "also script it:  aswell config set <key> <value>\n\n"
                      << "Press any key to continue\n";
            Terminal::read_key();
        }
    }
}

} // namespace aswell
