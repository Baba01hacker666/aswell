#pragma once

#include "aswell/common.hpp"

namespace aswell {

struct ThemeInfo {
    std::string name;
    std::string description;
    std::string css_content;
    std::string template_html;
};

class ThemeManager {
public:
    static std::vector<std::string> get_builtin_theme_names();
    static ThemeInfo get_theme(const std::string& name);
    static ThemeInfo get_custom_theme(const std::string& theme_dir, const std::string& name);

    static std::string get_default_css();
    static std::string get_cyberpunk_css();
    static std::string get_nord_css();
    static std::string get_minimal_css();
    static std::string get_dracula_css();
    static std::string get_powerline_css();
};

} // namespace aswell
