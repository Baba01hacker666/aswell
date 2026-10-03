#include "aswell/config/settings.hpp"
#include "aswell/config/theme.hpp"
#include <cstdlib>

namespace aswell {

namespace {

// The rows below are built through these small factories so every setting gets
// identical parsing, validation and serialization behaviour for free.
std::string bool_str(bool v) { return v ? "true" : "false"; }

SettingDef bool_setting(const char* key, std::vector<std::string> aliases, const char* category,
                        const char* description, bool ShellConfig::*field, bool advanced = false) {
    SettingDef def;
    def.key = key;
    def.aliases = std::move(aliases);
    def.type = SettingType::BOOL;
    def.category = category;
    def.description = description;
    def.advanced = advanced;
    def.get = [field](const ShellConfig& c) { return bool_str(c.*field); };
    def.set = [field](ShellConfig& c, const std::string& v) {
        bool parsed = false;
        if (SettingsRegistry::parse_bool(v, parsed)) c.*field = parsed;
    };
    return def;
}

SettingDef string_setting(const char* key, std::vector<std::string> aliases, const char* category,
                          const char* description, std::string ShellConfig::*field,
                          bool advanced = false) {
    SettingDef def;
    def.key = key;
    def.aliases = std::move(aliases);
    def.type = SettingType::STRING;
    def.category = category;
    def.description = description;
    def.advanced = advanced;
    def.get = [field](const ShellConfig& c) { return c.*field; };
    def.set = [field](ShellConfig& c, const std::string& v) { c.*field = v; };
    return def;
}

// Enum values are canonicalised (trimmed + lower case) so `theme = CyberPunk`
// and `theme=cyberpunk` behave identically everywhere.
SettingDef enum_setting(const char* key, std::vector<std::string> aliases, const char* category,
                        const char* description, std::string ShellConfig::*field) {
    SettingDef def;
    def.key = key;
    def.aliases = std::move(aliases);
    def.type = SettingType::ENUM;
    def.category = category;
    def.description = description;
    def.dynamic_choices = true;
    def.get = [field](const ShellConfig& c) { return c.*field; };
    def.set = [field](ShellConfig& c, const std::string& v) {
        c.*field = str_util::to_lower(str_util::trim(v));
    };
    return def;
}

SettingDef int_setting(const char* key, std::vector<std::string> aliases, const char* category,
                       const char* description, int ShellConfig::*field, long long min_value,
                       long long max_value, const char* units, bool advanced = false) {
    SettingDef def;
    def.key = key;
    def.aliases = std::move(aliases);
    def.type = SettingType::INT;
    def.category = category;
    def.description = description;
    def.min_value = min_value;
    def.max_value = max_value;
    def.units = units;
    def.advanced = advanced;
    def.get = [field](const ShellConfig& c) { return std::to_string(c.*field); };
    def.set = [field, min_value, max_value](ShellConfig& c, const std::string& v) {
        char* end = nullptr;
        long long parsed = std::strtoll(v.c_str(), &end, 10);
        if (end != v.c_str() && parsed >= min_value && parsed <= max_value) {
            c.*field = static_cast<int>(parsed);
        }
    };
    return def;
}

// ---------------------------------------------------------------------------
// THE SETTINGS TABLE — single source of truth for config.txt, `aswell config`,
// the TUI editor, `aswell doctor`, theme validation and Tab completion.
// Order here is the display order everywhere else.
// ---------------------------------------------------------------------------
const std::vector<SettingDef>& registry() {
    static const std::vector<SettingDef> kSettings = {
        // --- prompt --------------------------------------------------------
        bool_setting("show_username", {"user_segment"}, "prompt",
                     "Show <user> in generated prompts", &ShellConfig::show_username),
        bool_setting("show_hostname", {"hostname_segment"}, "prompt",
                     "Show <hostname> in generated prompts", &ShellConfig::show_hostname),
        bool_setting("show_directory", {"cwd_segment"}, "prompt",
                     "Show <directory> in generated prompts", &ShellConfig::show_directory),
        bool_setting("show_git", {"git_segment", "show_branch"}, "prompt",
                     "Show the <git> branch badge in generated prompts", &ShellConfig::show_git),
        bool_setting("show_runtime", {"duration_segment"}, "prompt",
                     "Show the <runtime> (last command duration) badge", &ShellConfig::show_runtime),
        bool_setting("show_jobs", {"jobs_segment"}, "prompt",
                     "Show the <jobs> (background job count) badge", &ShellConfig::show_jobs),
        bool_setting("show_status", {"status_segment"}, "prompt",
                     "Show the <status> (last exit code) badge", &ShellConfig::show_status),
        string_setting("username", {"user", "custom_username"}, "prompt",
                       "Display name replacing $USER in the prompt (empty = system user)",
                       &ShellConfig::custom_username),
        string_setting("hostname", {"host", "custom_hostname"}, "prompt",
                       "Display name replacing the host name in the prompt (empty = real host)",
                       &ShellConfig::custom_hostname),
        string_setting("curated_aliases", {"alias_packs", "good_aliases"}, "shell",
                       "Install these alias-library selections at startup (category, name, comma list or 'all')",
                       &ShellConfig::curated_aliases, true),
        int_setting("parallel_jobs", {"parallel_default_jobs"}, "shell",
                    "Default concurrent jobs for the parallel builtin (0 = one per CPU)",
                    &ShellConfig::parallel_jobs, 0, 256, "jobs", true),
        string_setting("time_format", {"time_fmt"}, "prompt",
                       "Format for the <time> prompt element (%H %M %S %I %p, and more)",
                       &ShellConfig::time_format, true),
        string_setting("date_format", {"date_fmt"}, "prompt",
                       "Format for the <date> prompt element (%Y %m %d %a %b %j, and more)",
                       &ShellConfig::date_format, true),

        // --- appearance ------------------------------------------------------
        enum_setting("theme", {"preset"}, "appearance",
                     "Prompt theme: built-in preset or ~/.config/aswell/themes/<name>.css",
                     &ShellConfig::theme_name),
        bool_setting("animation", {"animations"}, "appearance",
                     "Allow CSS animations (pulse, wave, rainbow, fire, spin, scramble) in the prompt",
                     &ShellConfig::enable_animation),
        bool_setting("command_animation", {"type_animation"}, "appearance",
                     "Animate the command line itself while typing",
                     &ShellConfig::enable_command_animation),
        bool_setting("command_banner", {"banner"}, "appearance",
                     "Print an execution summary banner after each command",
                     &ShellConfig::enable_command_banner),
        bool_setting("colored_output", {"color_output"}, "appearance",
                     "Export TrueColor hints and colored ls/grep/diff aliases",
                     &ShellConfig::enable_colored_output),

        // --- editor ----------------------------------------------------------
        bool_setting("syntax_highlighting", {"highlight", "highlighting"}, "editor",
                     "Colorize the command line as you type", &ShellConfig::enable_syntax_highlighting),
        bool_setting("autosuggestions", {"suggestions", "autosuggest"}, "editor",
                     "Ghost-text history autosuggestions while typing",
                     &ShellConfig::enable_autosuggestions),
        bool_setting("vi_mode", {"vim"}, "editor",
                     "Start the line editor with Vi normal/insert modes", &ShellConfig::vi_mode),
        bool_setting("history_ignore_dups", {"history_dedup", "ignoredups"}, "editor",
                     "Drop repeated commands anywhere in history, not only consecutive ones",
                     &ShellConfig::history_ignore_dups),
        int_setting("history_size", {"history_max"}, "editor",
                    "Maximum number of saved history entries", &ShellConfig::history_size,
                    10, 1000000, "entries", true),

        // --- shell -------------------------------------------------------------
        bool_setting("auto_reload", {"hot_reload", "live_reload"}, "shell",
                     "Reload theme.css, prompt.html and config.txt as soon as they change",
                     &ShellConfig::auto_reload),
        bool_setting("import_bashrc", {"bashrc", "bash_compat"}, "shell",
                     "Import ~/.bashrc aliases, exports and functions on startup",
                     &ShellConfig::import_bashrc),
    };
    return kSettings;
}

bool contains_value(const std::vector<std::string>& list, const std::string& value) {
    std::string lowered = str_util::to_lower(value);
    for (const auto& item : list) {
        if (str_util::to_lower(item) == lowered) return true;
    }
    return false;
}

bool has_control_chars(const std::string& value) {
    for (char ch : value) {
        if (static_cast<unsigned char>(ch) < 0x20) return true;
    }
    return false;
}

} // namespace

const std::vector<SettingDef>& SettingsRegistry::all() {
    return registry();
}

const SettingDef* SettingsRegistry::find(std::string_view key) {
    std::string wanted = str_util::to_lower(str_util::trim(key));
    if (wanted.empty()) return nullptr;

    for (const auto& def : registry()) {
        if (def.key == wanted) return &def;
    }
    for (const auto& def : registry()) {
        if (contains_value(def.aliases, wanted)) return &def;
    }
    return nullptr;
}

std::string SettingsRegistry::suggest(std::string_view key) {
    std::string wanted = str_util::to_lower(str_util::trim(key));
    if (wanted.empty()) return "";

    std::string best;
    int best_dist = 5;
    for (const auto& def : registry()) {
        int dist = str_util::levenshtein_distance(wanted, def.key);
        if (dist < best_dist) {
            best_dist = dist;
            best = def.key;
        }
        for (const auto& alias : def.aliases) {
            if (str_util::levenshtein_distance(wanted, alias) < best_dist) {
                best_dist = str_util::levenshtein_distance(wanted, alias);
                best = def.key;
            }
        }
    }
    return best;
}

std::vector<std::string> SettingsRegistry::choices(const SettingDef& def) {
    if (def.dynamic_choices) {
        return ThemeManager::theme_names(ConfigManager::get_config_dir());
    }
    return def.choices;
}

bool SettingsRegistry::parse_bool(std::string_view value, bool& out) {
    std::string v = str_util::to_lower(str_util::trim(value));
    if (v == "true" || v == "1" || v == "yes" || v == "on" || v == "y") {
        out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off" || v == "n") {
        out = false;
        return true;
    }
    return false;
}

std::string SettingsRegistry::default_value(const SettingDef& def) {
    ShellConfig fresh;
    return def.get(fresh);
}

std::string SettingsRegistry::value(const SettingDef& def, const ShellConfig& cfg) {
    return def.get(cfg);
}

bool SettingsRegistry::validate(const SettingDef& def, const std::string& candidate, std::string& err) {
    err.clear();
    switch (def.type) {
        case SettingType::BOOL: {
            bool parsed = false;
            if (!parse_bool(candidate, parsed)) {
                err = "'" + candidate + "' is not a boolean (use true/false, 1/0, yes/no or on/off)";
                return false;
            }
            return true;
        }
        case SettingType::INT: {
            if (candidate.empty()) {
                err = "expected a number between " + std::to_string(def.min_value) + " and " +
                      std::to_string(def.max_value);
                return false;
            }
            for (size_t i = 0; i < candidate.size(); ++i) {
                bool digit = std::isdigit(static_cast<unsigned char>(candidate[i])) != 0;
                bool sign = (i == 0 && (candidate[i] == '+' || candidate[i] == '-'));
                if (!digit && !sign) {
                    err = "'" + candidate + "' is not a whole number";
                    return false;
                }
            }
            char* end = nullptr;
            long long parsed = std::strtoll(candidate.c_str(), &end, 10);
            if (parsed < def.min_value || parsed > def.max_value) {
                err = std::to_string(parsed) + " is outside the allowed range " +
                      std::to_string(def.min_value) + ".." + std::to_string(def.max_value);
                if (!def.units.empty()) err += " " + def.units;
                return false;
            }
            return true;
        }
        case SettingType::ENUM: {
            std::vector<std::string> options = choices(def);
            if (candidate.empty() || !contains_value(options, candidate)) {
                std::string joined;
                for (size_t i = 0; i < options.size(); ++i) {
                    joined += (i ? ", " : "") + options[i];
                }
                err = "'" + candidate + "' is not a valid value for " + def.key;
                if (!joined.empty()) err += " (available: " + joined + ")";
                std::string hint = str_util::to_lower(str_util::trim(candidate));
                for (const auto& option : options) {
                    if (str_util::levenshtein_distance(hint, option) <= 2) {
                        err += " — did you mean '" + option + "'?";
                        break;
                    }
                }
                return false;
            }
            return true;
        }
        case SettingType::STRING: {
            if (has_control_chars(candidate)) {
                err = "values may not contain control characters or newlines";
                return false;
            }
            return true;
        }
    }
    return true;
}

bool SettingsRegistry::apply(ShellConfig& cfg, std::string_view key, const std::string& candidate,
                            std::string& err) {
    const SettingDef* def = find(key);
    if (!def) {
        err = "unknown setting '" + std::string(str_util::trim(key)) + "'";
        std::string hint = suggest(key);
        if (!hint.empty()) err += " (did you mean '" + hint + "'?)";
        err += "\n  List every setting with: aswell config help";
        return false;
    }
    if (!validate(*def, candidate, err)) {
        err = "cannot set " + def->key + ": " + err;
        return false;
    }
    def->set(cfg, candidate);
    return true;
}

bool SettingsRegistry::reset(ShellConfig& cfg, std::string_view key, std::string& err) {
    const SettingDef* def = find(key);
    if (!def) {
        err = "unknown setting '" + std::string(str_util::trim(key)) + "'";
        std::string hint = suggest(key);
        if (!hint.empty()) err += " (did you mean '" + hint + "'?)";
        return false;
    }
    def->set(cfg, default_value(*def));
    return true;
}

bool SettingsRegistry::is_modified(const SettingDef& def, const ShellConfig& cfg) {
    if (def.type == SettingType::ENUM) {
        return str_util::to_lower(def.get(cfg)) != str_util::to_lower(default_value(def));
    }
    return def.get(cfg) != default_value(def);
}

std::vector<std::string> SettingsRegistry::categories() {
    std::vector<std::string> out;
    for (const auto& def : registry()) {
        if (std::find(out.begin(), out.end(), def.category) == out.end()) {
            out.push_back(def.category);
        }
    }
    return out;
}

} // namespace aswell
