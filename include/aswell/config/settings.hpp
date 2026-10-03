#pragma once

#include "aswell/common.hpp"
#include "aswell/config/config.hpp"

namespace aswell {

// Every user-facing knob of Aswell is described exactly once, in
// `SettingsRegistry::all()`. The table is the single source of truth used by:
//   * ConfigManager::load()/save()      (parsing & serializing config.txt)
//   * `aswell config list/get/set/...`  (the CLI/shell settings hub)
//   * `aswell config help`              (self-documenting help text)
//   * the interactive TUI editor        (menu rows are generated from it)
//   * `aswell doctor`                   (validation + "did you mean" hints)
//   * Tab completion                    (setting names & their allowed values)
//
// Adding a new setting therefore means: 1 struct field in ShellConfig +
// 1 row in the table below. Everything else follows automatically.
enum class SettingType {
    BOOL,
    STRING,
    ENUM,
    INT,
};

struct SettingDef {
    std::string key;                        // canonical name used in config.txt
    std::vector<std::string> aliases;       // alternative names accepted when reading
    SettingType type = SettingType::BOOL;
    std::string category;                   // grouping for `list` / TUI sections
    std::string description;                // one-line human documentation
    std::vector<std::string> choices;       // ENUM: static candidate values
    bool dynamic_choices = false;           // ENUM: candidates resolved at runtime (themes)
    long long min_value = 0;                // INT: inclusive lower bound
    long long max_value = 0;                // INT: inclusive upper bound
    std::string units;                      // INT: display suffix (e.g. "entries")
    bool advanced = false;                  // hidden from `list` unless --all / TUI "advanced"

    // Canonical serialization / deserialization against the typed config struct.
    std::function<std::string(const ShellConfig&)> get;
    std::function<void(ShellConfig&, const std::string&)> set;
};

class SettingsRegistry {
public:
    static const std::vector<SettingDef>& all();

    // Exact key, alias, or case-insensitive match. nullptr when unknown.
    static const SettingDef* find(std::string_view key);

    // Closest known key for a typo, or "" when nothing is plausibly similar.
    static std::string suggest(std::string_view key);

    // Candidate values for a setting (dynamic ones are resolved from disk).
    static std::vector<std::string> choices(const SettingDef& def);

    // "true"/"false"/"1"/"0"/"yes"/"no"/"on"/"off"/"y"/"n" -> bool.
    static bool parse_bool(std::string_view value, bool& out);

    // Default value of a setting, rendered the same way as `get`.
    static std::string default_value(const SettingDef& def);

    // Current value of a setting for the given config.
    static std::string value(const SettingDef& def, const ShellConfig& cfg);

    // Validates a candidate value, filling `err` with an actionable message.
    static bool validate(const SettingDef& def, const std::string& value, std::string& err);

    // validate() + apply. Returns false and leaves cfg untouched on invalid input.
    static bool apply(ShellConfig& cfg, std::string_view key, const std::string& value,
                      std::string& err);

    // Restores one setting to its registered default.
    static bool reset(ShellConfig& cfg, std::string_view key, std::string& err);

    // True when the setting currently differs from its default value.
    static bool is_modified(const SettingDef& def, const ShellConfig& cfg);

    static std::vector<std::string> categories();
};

} // namespace aswell
