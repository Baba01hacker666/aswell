#include "aswell/config/theme.hpp"
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

namespace aswell {

namespace {

struct BuiltinTheme {
    const char* name;
    const char* description;
    std::string (*css)();
};

// Single source of truth for the shipped presets: names, descriptions and
// stylesheets all derive from this table (theme list, `theme set` validation,
// settings choices, prompt previews).
const std::vector<BuiltinTheme>& builtin_themes() {
    static const std::vector<BuiltinTheme> kThemes = {
        {"modern", "Modern elegant two-line prompt with unicode connectors", &ThemeManager::get_default_css},
        {"cyberpunk", "Neon cyan & magenta cyberpunk aesthetic with fiery accents", &ThemeManager::get_cyberpunk_css},
        {"matrix", "Terminal green matrix with high-speed random scrambling username", &ThemeManager::get_matrix_css},
        {"nord", "Arctic blue and snow frost palette", &ThemeManager::get_nord_css},
        {"minimal", "Ultra-clean monochrome minimal prompt", &ThemeManager::get_minimal_css},
        {"dracula", "Classic dark vampire theme with vibrant highlights", &ThemeManager::get_dracula_css},
        {"powerline", "Contrasting segmented powerline bars", &ThemeManager::get_powerline_css},
    };
    return kThemes;
}

std::string read_file_contents(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool is_regular_file(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

// Pulls a human description out of the leading CSS comment block so custom
// themes advertise themselves in `aswell theme list`.
std::string description_from_css(const std::string& css) {
    size_t open = css.find("/*");
    if (open == std::string::npos || open > 4) return "";
    size_t close = css.find("*/", open + 2);
    if (close == std::string::npos) return "";
    std::string body = css.substr(open + 2, close - (open + 2));
    std::vector<std::string> lines = str_util::split(body, '\n');
    for (auto& raw : lines) {
        std::string line = str_util::trim(raw);
        while (!line.empty() && line.front() == '*') line = str_util::trim(line.substr(1));
        if (line.empty()) continue;
        if (str_util::starts_with(line, "Aswell theme")) continue;
        return line;
    }
    return "";
}

} // namespace

std::vector<std::string> ThemeManager::get_builtin_theme_names() {
    std::vector<std::string> names;
    names.reserve(builtin_themes().size());
    for (const auto& t : builtin_themes()) names.push_back(t.name);
    return names;
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
    std::string lowered = str_util::to_lower(str_util::trim(name));
    for (const auto& t : builtin_themes()) {
        if (lowered == t.name) {
            ThemeInfo info;
            info.name = t.name;
            info.description = t.description;
            info.css_content = t.css();
            info.source = "builtin";
            return info;
        }
    }

    // Unknown names fall back to the default preset so the shell always renders.
    ThemeInfo info;
    info.name = "modern";
    info.description = "Modern elegant two-line prompt with unicode connectors";
    info.css_content = get_default_css();
    info.source = "builtin";
    return info;
}

ThemeInfo ThemeManager::get_custom_theme(const std::string& theme_dir, const std::string& name) {
    std::string file_path = theme_dir + "/" + name + ".css";
    std::string css = read_file_contents(file_path);
    if (!css.empty()) {
        ThemeInfo info;
        info.name = name;
        info.description = "Custom theme from " + file_path;
        info.css_content = css;
        info.source = file_path;
        info.is_user = true;
        return info;
    }
    return get_theme(name);
}

std::vector<ThemeEntry> ThemeManager::list_themes(const std::string& config_dir,
                                                  const std::string& active_name) {
    std::vector<ThemeEntry> entries;
    std::string themes_dir = config_dir + "/themes";
    std::string legacy_css = config_dir + "/theme.css";

    for (const auto& t : builtin_themes()) {
        ThemeEntry e;
        e.name = t.name;
        e.description = t.description;
        e.source = "builtin";
        entries.push_back(e);
    }

    // User stylesheets shadow built-ins of the same name.
    DIR* d = opendir(themes_dir.c_str());
    if (d) {
        std::vector<std::string> files;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            std::string fname = ent->d_name;
            if (fname.empty() || fname.front() == '.') continue;
            if (!str_util::ends_with(fname, ".css")) continue;
            files.push_back(fname);
        }
        closedir(d);
        std::sort(files.begin(), files.end());

        for (const auto& fname : files) {
            std::string name = fname.substr(0, fname.size() - 4);
            if (name == "theme") continue; // legacy single-file theme, listed separately
            std::string css = read_file_contents(themes_dir + "/" + fname);
            ThemeEntry e;
            e.name = name;
            e.is_user = true;
            e.source = themes_dir + "/" + fname;
            e.description = description_from_css(css);
            if (e.description.empty()) e.description = "Custom user stylesheet";

            bool replaced = false;
            for (auto& existing : entries) {
                if (existing.name == name) {
                    existing = e;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) entries.push_back(e);
        }
    }

    if (is_regular_file(legacy_css)) {
        std::string css = read_file_contents(legacy_css);
        ThemeEntry e;
        e.name = "theme.css";
        e.is_user = true;
        e.source = legacy_css;
        e.description = description_from_css(css);
        if (e.description.empty()) e.description = "Legacy single-file stylesheet (applies when no theme matches)";
        entries.push_back(e);
    }

    std::string wanted = str_util::to_lower(str_util::trim(active_name));
    for (auto& e : entries) {
        if (e.name == wanted) e.is_active = true;
    }
    return entries;
}

std::vector<std::string> ThemeManager::theme_names(const std::string& config_dir) {
    std::vector<std::string> names;
    for (const auto& e : list_themes(config_dir, "")) names.push_back(e.name);
    // The legacy single-file theme is a real choice, and `aswell theme set custom`
    // is the documented way to reach it. resolve() understands these names but the
    // registry validates against this list, so they have to appear here too.
    if (is_regular_file(config_dir + "/theme.css")) {
        for (const char* legacy : {"custom", "user", "theme.css"}) names.push_back(legacy);
    }
    return names;
}

bool ThemeManager::exists(const std::string& config_dir, const std::string& name) {
    std::string wanted = str_util::to_lower(str_util::trim(name));
    if (wanted.empty()) return false;
    if (is_regular_file(config_dir + "/themes/" + wanted + ".css")) return true;
    if (wanted == "theme.css" || wanted == "custom" || wanted == "user") {
        return is_regular_file(config_dir + "/theme.css");
    }
    for (const auto& t : builtin_themes()) {
        if (wanted == t.name) return true;
    }
    return false;
}

ThemeInfo ThemeManager::resolve(const std::string& config_dir, const std::string& name,
                               std::string* warning) {
    std::string wanted = str_util::to_lower(str_util::trim(name));

    // 1. A user stylesheet with a matching name always wins.
    if (!wanted.empty()) {
        std::string user_path = config_dir + "/themes/" + wanted + ".css";
        std::string css = read_file_contents(user_path);
        if (!css.empty()) {
            if (css.find('{') == std::string::npos && warning) {
                *warning = user_path + " contains no CSS rules, so the prompt renders unstyled";
            }
            ThemeInfo info;
            info.name = wanted;
            info.description = description_from_css(css);
            if (info.description.empty()) info.description = "Custom theme from " + user_path;
            info.css_content = css;
            info.source = user_path;
            info.is_user = true;
            return info;
        }
        if (is_regular_file(user_path) && warning) {
            *warning = user_path + " exists but could not be read or is empty";
        }
    }

    // 2. Built-in presets.
    for (const auto& t : builtin_themes()) {
        if (wanted == t.name) {
            ThemeInfo info;
            info.name = t.name;
            info.description = t.description;
            info.css_content = t.css();
            info.source = "builtin";
            return info;
        }
    }

    // 3. Legacy ~/.config/aswell/theme.css. Checked after the user themes above, so
    // an explicit `theme=<name>` with themes/<name>.css still wins, and addressed
    // directly by `custom`, `user` or `theme.css`.
    std::string legacy = read_file_contents(config_dir + "/theme.css");
    if (!legacy.empty() &&
        (wanted == "theme.css" || wanted == "custom" || wanted == "user" || wanted.empty())) {
        ThemeInfo info;
        info.name = wanted.empty() ? "theme.css" : wanted;
        info.description = "Legacy single-file theme";
        info.css_content = legacy;
        info.source = config_dir + "/theme.css";
        info.is_user = true;
        return info;
    }

    // 4. Unknown theme: keep the shell usable and say what happened.
    ThemeInfo info = get_theme("modern");
    if (warning && !wanted.empty() && wanted != "modern") {
        std::string hint = suggest(config_dir, wanted);
        *warning = "theme '" + wanted + "' is not installed; using '" + info.name + " theme'" +
                   (hint.empty() ? "" : " (nearest match: '" + hint + "')");
    }
    return info;
}

std::string ThemeManager::suggest(const std::string& config_dir, const std::string& name) {
    std::string wanted = str_util::to_lower(str_util::trim(name));
    if (wanted.empty()) return "";

    std::string best;
    int best_dist = 4; // generous enough for typos, tight enough to avoid noise
    for (const auto& candidate : theme_names(config_dir)) {
        int dist = str_util::levenshtein_distance(wanted, str_util::to_lower(candidate));
        if (dist < best_dist) {
            best_dist = dist;
            best = candidate;
        }
    }
    return best;
}

bool ThemeManager::create_theme(const std::string& config_dir, const std::string& name,
                               const std::string& from, std::string& path_out, std::string& err,
                               bool force) {
    std::string wanted = str_util::to_lower(str_util::trim(name));
    if (wanted.empty()) {
        err = "theme name must not be empty";
        return false;
    }
    for (char c : wanted) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) {
            err = "theme names may only contain a-z, 0-9, '_', '-' and '.'";
            return false;
        }
    }
    if (wanted == "theme") {
        err = "'theme' is reserved for the legacy theme.css override";
        return false;
    }

    std::string themes_dir = config_dir + "/themes";
    path_out = themes_dir + "/" + wanted + ".css";
    // --force unlinks here, after the name whitelist above has run: doing it before
    // validation would let `theme new ../../victim --force` delete an unrelated file.
    if (is_regular_file(path_out)) {
        if (!force) {
            err = path_out + " already exists (use --force to overwrite)";
            return false;
        }
        if (::unlink(path_out.c_str()) != 0) {
            err = "cannot overwrite " + path_out + ": " + std::strerror(errno);
            return false;
        }
    }
    if (!fs_util::mkdir_p(themes_dir)) {
        err = "could not create " + themes_dir;
        return false;
    }

    std::string seed_css;
    std::string seed_name = from.empty() ? std::string("modern") : from;
    ThemeInfo seed = resolve(config_dir, seed_name, nullptr);
    seed_css = seed.css_content;

    std::ofstream f(path_out, std::ios::trunc);
    if (!f) {
        err = "could not write " + path_out;
        return false;
    }
    f << "/* Aswell theme: " << wanted << "\n";
    f << " * Starter stylesheet seeded from '" << seed.name << "'.\n";
    f << " * Live preview while you edit:  the shell reloads this file on the next prompt\n";
    f << " * Validate it any time with:    aswell doctor\n";
    f << " * Selectors: prompt, user, hostname, directory, git, runtime, status, jobs,\n";
    f << " *            mode, symbol, .segment, text, #id  (see docs/CSS_REFERENCE.md)\n";
    f << " */\n\n";
    f << seed_css;
    if (!seed_css.empty() && seed_css.back() != '\n') f << "\n";
    f.close();
    return true;
}

} // namespace aswell
