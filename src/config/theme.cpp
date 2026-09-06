#include "aswell/config/theme.hpp"
#include <fstream>

namespace aswell {

std::vector<std::string> ThemeManager::get_builtin_theme_names() {
    return {"modern", "cyberpunk", "matrix", "nord", "minimal", "dracula", "powerline"};
}

std::string ThemeManager::get_default_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #6272a4;
}
user {
    color: #8be9fd;
    font-weight: bold;
}
.at {
    color: #6272a4;
}
hostname {
    color: #bd93f9;
}
.sep {
    color: #6272a4;
}
directory {
    color: #50fa7b;
    font-weight: bold;
}
git {
    color: #f1fa8c;
    margin-left: 1;
}
git.dirty {
    color: #ffb86c;
}
runtime {
    color: #f1fa8c;
    margin-left: 1;
}
status.error {
    color: #ff5555;
    margin-left: 1;
    font-weight: bold;
}
status.success {
    color: #50fa7b;
}
jobs {
    color: #ff79c6;
    margin-left: 1;
}
mode.normal {
    color: #ffb86c;
    font-weight: bold;
    margin-right: 1;
}
symbol {
    color: #ff79c6;
    animation: pulse 1500ms infinite;
}
symbol.error {
    color: #ff5555;
}
)";
}

std::string ThemeManager::get_cyberpunk_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #ff007f;
    font-weight: bold;
}
user {
    color: #00f0ff;
    font-weight: bold;
}
.at {
    color: #fee715;
}
hostname {
    color: #00f0ff;
}
.sep {
    color: #ff007f;
}
directory {
    color: #fee715;
    font-weight: bold;
}
git {
    color: #00f0ff;
    margin-left: 1;
}
git.dirty {
    color: #ff007f;
}
runtime {
    color: #00f0ff;
    margin-left: 1;
}
status.error {
    color: #ff0055;
    margin-left: 1;
    font-weight: bold;
}
jobs {
    color: #fee715;
    margin-left: 1;
}
mode.normal {
    color: #fee715;
    font-weight: bold;
    margin-right: 1;
}
symbol {
    color: #00f0ff;
    animation: fire 1000ms infinite;
}
symbol.error {
    color: #ff0055;
}
)";
}

std::string ThemeManager::get_nord_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #4c566a;
}
user {
    color: #88c0d0;
    font-weight: bold;
}
.at {
    color: #4c566a;
}
hostname {
    color: #81a1c1;
}
.sep {
    color: #4c566a;
}
directory {
    color: #eceff4;
    font-weight: bold;
}
git {
    color: #a3be8c;
    margin-left: 1;
}
git.dirty {
    color: #ebcb8b;
}
runtime {
    color: #81a1c1;
    margin-left: 1;
}
status.error {
    color: #bf616a;
    margin-left: 1;
}
jobs {
    color: #b48ead;
    margin-left: 1;
}
mode.normal {
    color: #ebcb8b;
    font-weight: bold;
    margin-right: 1;
}
symbol {
    color: #88c0d0;
    animation: pulse 2000ms infinite;
}
symbol.error {
    color: #bf616a;
}
)";
}

std::string ThemeManager::get_minimal_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #666666;
}
user {
    color: #ffffff;
}
.at {
    color: #666666;
}
hostname {
    color: #aaaaaa;
}
directory {
    color: #ffffff;
    font-weight: bold;
    margin-left: 1;
}
git {
    color: #888888;
    margin-left: 1;
}
runtime {
    color: #666666;
    margin-left: 1;
}
symbol {
    color: #ffffff;
}
symbol.error {
    color: #ff4444;
}
)";
}

std::string ThemeManager::get_dracula_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #6272a4;
}
user {
    color: #8be9fd;
    font-weight: bold;
}
.at {
    color: #6272a4;
}
hostname {
    color: #bd93f9;
}
directory {
    color: #50fa7b;
    font-weight: bold;
    margin-left: 1;
}
git {
    color: #f1fa8c;
    margin-left: 1;
}
git.dirty {
    color: #ffb86c;
}
runtime {
    color: #ff79c6;
    margin-left: 1;
}
status.error {
    color: #ff5555;
    margin-left: 1;
}
symbol {
    color: #bd93f9;
    animation: rainbow 3000ms infinite;
}
symbol.error {
    color: #ff5555;
}
)";
}

std::string ThemeManager::get_powerline_css() {
    return R"(
prompt.main {
    display: block;
}
user {
    color: #ffffff;
    background: #6272a4;
    padding: 0 1;
    font-weight: bold;
}
directory {
    color: #282a36;
    background: #50fa7b;
    padding: 0 1;
    font-weight: bold;
}
git {
    color: #282a36;
    background: #f1fa8c;
    padding: 0 1;
}
status.error {
    color: #ffffff;
    background: #ff5555;
    padding: 0 1;
}
symbol {
    color: #50fa7b;
    margin-left: 1;
}
symbol.error {
    color: #ff5555;
}
)";
}

std::string ThemeManager::get_matrix_css() {
    return R"(
prompt.main {
    display: block;
}
.bracket {
    color: #00ff66;
}
user {
    color: #00ffaa;
    font-weight: bold;
    animation: scramble 40ms infinite;
}
.at {
    color: #008833;
}
hostname {
    color: #00cc55;
}
.sep {
    color: #00ff66;
}
directory {
    color: #50fa7b;
    font-weight: bold;
}
git {
    color: #55ff99;
    margin-left: 1;
}
git.dirty {
    color: #ffb86c;
}
runtime {
    color: #00ffaa;
    margin-left: 1;
}
status.error {
    color: #ff5555;
    margin-left: 1;
}
symbol {
    color: #00ff66;
    animation: pulse 1000ms infinite;
}
symbol.error {
    color: #ff5555;
}
)";
}

ThemeInfo ThemeManager::get_theme(const std::string& name) {
    ThemeInfo info;
    info.name = name;

    if (name == "matrix") {
        info.description = "Terminal green matrix with high-speed random scrambling username";
        info.css_content = get_matrix_css();
    } else if (name == "cyberpunk") {
        info.description = "Neon cyan & magenta cyberpunk aesthetic with fiery accents";
        info.css_content = get_cyberpunk_css();
    } else if (name == "nord") {
        info.description = "Arctic blue and snow frost palette";
        info.css_content = get_nord_css();
    } else if (name == "minimal") {
        info.description = "Ultra-clean monochrome minimal prompt";
        info.css_content = get_minimal_css();
    } else if (name == "dracula") {
        info.description = "Classic dark vampire theme with vibrant highlights";
        info.css_content = get_dracula_css();
    } else if (name == "powerline") {
        info.description = "Contrasting segmented powerline bars";
        info.css_content = get_powerline_css();
    } else {
        info.name = "modern";
        info.description = "Modern elegant two-line prompt with unicode connectors";
        info.css_content = get_default_css();
    }

    return info;
}

ThemeInfo ThemeManager::get_custom_theme(const std::string& theme_dir, const std::string& name) {
    std::string file_path = theme_dir + "/" + name + ".css";
    std::ifstream f(file_path);
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        ThemeInfo info;
        info.name = name;
        info.description = "Custom theme from " + file_path;
        info.css_content = ss.str();
        return info;
    }
    return get_theme(name);
}

} // namespace aswell
