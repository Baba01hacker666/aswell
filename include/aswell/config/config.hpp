#pragma once

#include "aswell/common.hpp"

namespace aswell {

struct ShellConfig {
    std::string theme_name = "modern";
    bool enable_animation = true;
    bool enable_syntax_highlighting = true;
    bool enable_autosuggestions = true;
    bool show_username = true;
    bool show_hostname = true;
    bool show_directory = true;
    bool show_git = true;
    bool show_runtime = true;
    bool show_jobs = true;
    bool show_status = true;
    bool enable_command_animation = false;
    bool enable_command_banner = false;
    bool enable_colored_output = true;
    bool vi_mode = false;
    bool import_bashrc = true;
    bool auto_reload = true;
    int history_size = 10000;
    bool history_ignore_dups = false;
    std::string time_format = "%H:%M:%S";
    std::string date_format = "%Y-%m-%d";
    std::string custom_username;
    std::string custom_hostname;
    std::string custom_template_html;
};

// Diagnostics produced while reading config.txt. Unknown keys and malformed
// values used to be dropped silently, which made typos invisible; `aswell
// doctor` and the startup path surface them through this struct instead.
struct ConfigReport {
    struct Issue {
        size_t line = 0;
        std::string key;
        std::string value;
        std::string message;    // what is wrong
        std::string suggestion; // human hint ("did you mean ..."), may be empty
        std::string fix;        // ready-to-paste command, may be empty
    };

    std::string path;
    std::vector<Issue> issues;
    size_t applied = 0;

    bool clean() const { return issues.empty(); }
    size_t problem_count() const { return issues.size(); }
};

// Watches the user-facing customization files so edits are picked up without
// restarting the shell. Detection is a handful of stat() calls per prompt.
class ConfigWatcher {
public:
    void watch(std::string path);
    void clear() { files_.clear(); }

    // Records the current modification times as the reference state.
    void snapshot();

    // True when a watched file appeared, disappeared or changed since snapshot().
    bool changed() const;
    std::vector<std::string> changed_files() const;

    size_t watched_count() const { return files_.size(); }

private:
    struct Entry {
        std::string path;
        long long mtime = 0;
        long long size = 0;
    };
    std::vector<Entry> files_;
    static bool stat_of(const std::string& path, long long& mtime, long long& size);
    static bool stat_differs(const Entry& e);
};

class ConfigManager {
public:
    // $ASWELL_CONFIG_DIR (or ~/.config/aswell). Also honoured by `aswell --config PATH`.
    static std::string get_config_dir();

    static ShellConfig load();
    static ShellConfig load(ConfigReport* report);

    // Parses arbitrary config.txt content (used by import, tests and doctor).
    static void parse_into(const std::string& text, ShellConfig& cfg, ConfigReport* report);

    static void save(const ShellConfig& cfg);
    static std::string serialize(const ShellConfig& cfg);

    // Line-oriented key editing that keeps comments, ordering and unknown keys.
    // Returns false when the file does not exist yet (caller may create it).
    static bool update_keys(const std::vector<std::pair<std::string, std::string>>& updates);

    static std::string build_template(const ShellConfig& cfg);

    // Raw contents of ~/.config/aswell/config.txt ("" when missing).
    static std::string raw_config_text();
};

} // namespace aswell
