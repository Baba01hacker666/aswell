#pragma once

#include "aswell/common.hpp"

namespace aswell {

struct ThemeInfo {
    std::string name;
    std::string description;
    std::string css_content;
    std::string template_html;
    std::string source;   // "builtin" or the absolute path the CSS came from
    bool is_user = false; // loaded from ~/.config/aswell/themes/*.css
};

struct ThemeEntry {
    std::string name;
    std::string description;
    std::string source;
    bool is_user = false;
    bool is_active = false;
};

class ThemeManager {
public:
    static std::vector<std::string> get_builtin_theme_names();
    static ThemeInfo get_theme(const std::string& name);
    static ThemeInfo get_custom_theme(const std::string& theme_dir, const std::string& name);

    // Every selectable theme: built-in presets plus the user's own
    // ~/.config/aswell/themes/*.css stylesheets, with the active one flagged.
    static std::vector<ThemeEntry> list_themes(const std::string& config_dir,
                                               const std::string& active_name);
    static std::vector<std::string> theme_names(const std::string& config_dir);

    // Resolution order for a theme name:
    //   themes/<name>.css  ->  built-in <name>  ->  legacy theme.css  ->  modern
    // `warning` receives a human-readable note when the name could not be
    // honoured (unknown theme, unreadable file, empty stylesheet).
    static ThemeInfo resolve(const std::string& config_dir, const std::string& name,
                             std::string* warning = nullptr);

    static bool exists(const std::string& config_dir, const std::string& name);

    // Nearest known theme name for a typo ("" when nothing is close enough).
    static std::string suggest(const std::string& config_dir, const std::string& name);

    // Scaffolds ~/.config/aswell/themes/<name>.css seeded from an existing theme.
    static bool create_theme(const std::string& config_dir, const std::string& name,
                            const std::string& from, std::string& path_out, std::string& err,
                            bool force = false);

    static std::string get_default_css();
    static std::string get_cyberpunk_css();
    static std::string get_nord_css();
    static std::string get_minimal_css();
    static std::string get_dracula_css();
    static std::string get_powerline_css();
    static std::string get_matrix_css();
};

} // namespace aswell
