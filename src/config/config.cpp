#include "aswell/config/config.hpp"
#include "aswell/config/settings.hpp"
#include <fstream>
#include <sys/stat.h>

namespace aswell {

std::string ConfigManager::get_config_dir() {
    const char* override_dir = std::getenv("ASWELL_CONFIG_DIR");
    if (override_dir && *override_dir) {
        std::string dir = str_util::trim(override_dir);
        if (str_util::starts_with(dir, "~/") || dir == "~") {
            const char* home = std::getenv("HOME");
            std::string home_dir = home ? home : "/root";
            dir = (dir == "~") ? home_dir : home_dir + dir.substr(1);
        }
        while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
        return dir;
    }

    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/root";
    return base + "/.config/aswell";
}

std::string ConfigManager::raw_config_text() {
    std::ifstream f(get_config_dir() + "/config.txt");
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void ConfigManager::parse_into(const std::string& text, ShellConfig& cfg, ConfigReport* report) {
    std::istringstream in(text);
    std::string line;
    size_t line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        // Strip trailing CR so CRLF files (Termux/Windows editors) still parse.
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string_view trimmed = str_util::trim_sv(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        size_t eq = trimmed.find('=');
        if (eq == std::string_view::npos) {
            if (report) {
                ConfigReport::Issue issue;
                issue.line = line_no;
                issue.key = std::string(trimmed);
                issue.message = "expected key=value";
                issue.suggestion = "prefix the line with '#' to keep it as a comment";
                issue.fix = "aswell config help";
                report->issues.push_back(issue);
            }
            continue;
        }

        std::string raw_key = str_util::trim(trimmed.substr(0, eq));
        std::string value = str_util::trim(trimmed.substr(eq + 1));
        // Allow optional quotes around string values: username="Doraemon"
        if (value.size() >= 2) {
            char front = value.front();
            char back = value.back();
            if ((front == '"' && back == '"') || (front == '\'' && back == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
        }

        const SettingDef* def = SettingsRegistry::find(raw_key);
        if (!def) {
            if (report) {
                ConfigReport::Issue issue;
                issue.line = line_no;
                issue.key = raw_key;
                issue.value = value;
                issue.message = "unknown setting (ignored)";
                std::string hint = SettingsRegistry::suggest(raw_key);
                if (!hint.empty()) {
                    issue.suggestion = "did you mean '" + hint + "'?";
                    issue.fix = "aswell config help " + hint;
                } else {
                    issue.fix = "remove line " + std::to_string(line_no) +
                                ", or prefix it with '#' to keep it as a note";
                }
                report->issues.push_back(issue);
            }
            continue;
        }

        std::string err;
        if (!SettingsRegistry::validate(*def, value, err)) {
            if (report) {
                ConfigReport::Issue issue;
                issue.line = line_no;
                issue.key = def->key;
                issue.value = value;
                issue.message = err;
                issue.suggestion = "valid: " + SettingsRegistry::default_value(*def);
                std::vector<std::string> choices = SettingsRegistry::choices(*def);
                if (def->type == SettingType::BOOL) {
                    issue.suggestion = "valid: true | false";
                } else if (!choices.empty()) {
                    std::string joined;
                    for (size_t i = 0; i < choices.size() && i < 8; ++i) {
                        joined += (i ? ", " : "") + choices[i];
                    }
                    issue.suggestion = "valid: " + joined;
                } else if (def->type == SettingType::INT) {
                    issue.suggestion = "valid range: " + std::to_string(def->min_value) + ".." +
                                       std::to_string(def->max_value);
                }
                // A pasteable command that restores a working value.
                std::string fallback = SettingsRegistry::default_value(*def);
                issue.fix = "aswell config set " + def->key + " " +
                            (fallback.empty() ? std::string("<value>") : fallback);
                report->issues.push_back(issue);
            }
            continue;
        }

        def->set(cfg, value);
        if (report) report->applied++;
    }
}

ShellConfig ConfigManager::load() {
    return load(nullptr);
}

ShellConfig ConfigManager::load(ConfigReport* report) {
    ShellConfig cfg;
    std::string file = get_config_dir() + "/config.txt";
    if (report) report->path = file;

    std::ifstream f(file);
    if (!f) return cfg;

    std::stringstream ss;
    ss << f.rdbuf();
    parse_into(ss.str(), cfg, report);
    return cfg;
}

std::string ConfigManager::serialize(const ShellConfig& cfg) {
    std::ostringstream out;
    out << "# Aswell shell configuration\n";
    out << "# Edit by hand, or use: aswell config set <key> <value>\n";
    out << "# Browse everything:   aswell config help\n";
    out << "# Validate this file:   aswell doctor\n\n";

    std::string current_category;
    for (const auto& def : SettingsRegistry::all()) {
        if (def.category != current_category) {
            current_category = def.category;
            out << "\n# --- " << current_category << " ---\n";
        }
        std::string value = def.get(cfg);
        if (def.type == SettingType::STRING && value.empty()) continue;
        out << def.key << "=" << value << "\n";
    }
    return out.str();
}

void ConfigManager::save(const ShellConfig& cfg) {
    std::string dir = get_config_dir();
    fs_util::mkdir_p(dir + "/themes");
    fs_util::mkdir_p(dir + "/plugins");
    fs_util::mkdir_p(dir + "/commands");

    std::ofstream f(dir + "/config.txt");
    if (!f) return;
    f << serialize(cfg);
}

bool ConfigManager::update_keys(const std::vector<std::pair<std::string, std::string>>& updates) {
    if (updates.empty()) return true;

    std::string dir = get_config_dir();
    std::string file = dir + "/config.txt";

    std::vector<std::string> lines;
    bool existed = false;
    std::ifstream in(file);
    if (in) {
        existed = true;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }
    if (!existed) fs_util::mkdir_p(dir);

    std::vector<std::pair<std::string, std::string>> pending = updates;

    // Rewrite matching keys in place so comments and user ordering survive.
    for (auto& line : lines) {
        std::string_view trimmed = str_util::trim_sv(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;
        size_t eq = trimmed.find('=');
        if (eq == std::string_view::npos) continue;
        std::string key = str_util::to_lower(str_util::trim(trimmed.substr(0, eq)));

        for (size_t i = 0; i < pending.size(); ++i) {
            if (pending[i].first != key) continue;
            line = key + "=" + pending[i].second;
            pending.erase(pending.begin() + static_cast<long>(i));
            break;
        }
        if (pending.empty()) break;
    }

    for (const auto& kv : pending) {
        if (!lines.empty() && !str_util::trim(lines.back()).empty()) lines.push_back("");
        lines.push_back(kv.first + "=" + kv.second);
    }

    std::ofstream out(file, std::ios::trunc);
    if (!out) return false;
    for (const auto& l : lines) out << l << "\n";
    return true;
}

std::string ConfigManager::build_template(const ShellConfig& cfg) {
    std::string html = "<prompt class=\"main\">\n  <segment class=\"top-line\">\n";
    html += "    <text class=\"bracket\">╭─</text>\n";

    if (cfg.show_username) html += "    <user />\n";
    if (cfg.show_username && cfg.show_hostname) html += "    <text class=\"at\">@</text>\n";
    if (cfg.show_hostname) html += "    <hostname />\n";
    if (cfg.show_directory) {
        if (cfg.show_username || cfg.show_hostname) html += "    <text class=\"sep\"> in </text>\n";
        html += "    <directory />\n";
    }
    if (cfg.show_git) html += "    <git />\n";
    if (cfg.show_runtime) html += "    <runtime />\n";
    if (cfg.show_status) html += "    <status />\n";
    if (cfg.show_jobs) html += "    <jobs />\n";

    html += "  </segment>\n  <newline />\n  <segment class=\"bottom-line\">\n";
    html += "    <text class=\"bracket\">╰─</text>\n    <mode />\n    <symbol />\n    <text> </text>\n";
    html += "  </segment>\n</prompt>\n";

    return html;
}

// ---------------------------------------------------------------------------
// ConfigWatcher
// ---------------------------------------------------------------------------

bool ConfigWatcher::stat_of(const std::string& path, long long& mtime, long long& size) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    mtime = static_cast<long long>(st.st_mtime);
    size = static_cast<long long>(st.st_size);
    return true;
}

void ConfigWatcher::watch(std::string path) {
    for (const auto& e : files_) {
        if (e.path == path) return;
    }
    files_.push_back(Entry{std::move(path), 0, 0});
}

void ConfigWatcher::snapshot() {
    for (auto& e : files_) {
        if (!stat_of(e.path, e.mtime, e.size)) {
            e.mtime = 0;
            e.size = -1; // sentinel: file currently absent
        }
    }
}

bool ConfigWatcher::stat_differs(const Entry& e) {
    long long mtime = 0;
    long long size = 0;
    if (!stat_of(e.path, mtime, size)) {
        return !(e.mtime == 0 && e.size == -1); // absent now, present before (or vice versa)
    }
    return mtime != e.mtime || size != e.size;
}

bool ConfigWatcher::changed() const {
    for (const auto& e : files_) {
        if (stat_differs(e)) return true;
    }
    return false;
}

std::vector<std::string> ConfigWatcher::changed_files() const {
    std::vector<std::string> out;
    for (const auto& e : files_) {
        if (stat_differs(e)) out.push_back(e.path);
    }
    return out;
}

} // namespace aswell
