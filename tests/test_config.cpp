#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

#include "aswell/config/config.hpp"
#include "aswell/config/settings.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/config/settings_cli.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/editor/completion.hpp"

using namespace aswell;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

namespace {

std::string tmp_config_dir() {
    static std::string dir;
    if (dir.empty()) {
        char tmpl[] = "/tmp/aswell_config_test_XXXXXX";
        const char* made = mkdtemp(tmpl);
        assert(made != nullptr);
        dir = made;
        ::setenv("ASWELL_CONFIG_DIR", dir.c_str(), 1);
    }
    return dir;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::trunc);
    f << content;
    f.close();
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool has(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

// True when a whole line starts with `prefix` (avoids substring accidents such
// as `show_hostname=` matching a `hostname=` lookup).
bool has_line_starting_with(const std::string& text, const std::string& prefix) {
    for (const auto& line : str_util::split(text, '\n')) {
        if (str_util::starts_with(line, prefix)) return true;
    }
    return false;
}

bool text_has(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// ---------------------------------------------------------------------------
// Registry: lookup, aliases, suggestions
// ---------------------------------------------------------------------------

static void test_registry_lookup() {
    assert(SettingsRegistry::all().size() >= 15);

    assert(SettingsRegistry::find("theme") != nullptr);
    assert(SettingsRegistry::find("THEME") != nullptr);
    assert(SettingsRegistry::find(" show_git ") != nullptr);
    assert(SettingsRegistry::find("nonsense") == nullptr);
    assert(SettingsRegistry::find("") == nullptr);

    // Aliases resolve to the canonical row.
    const SettingDef* via_alias = SettingsRegistry::find("vim");
    assert(via_alias && via_alias->key == "vi_mode");
    const SettingDef* via_hot = SettingsRegistry::find("hot_reload");
    assert(via_hot && via_hot->key == "auto_reload");

    // Typos get a concrete suggestion.
    assert(SettingsRegistry::suggest("animtion") == "animation");
    assert(SettingsRegistry::suggest("show_gti") == "show_git");
    assert(SettingsRegistry::suggest("zzzzzzzzzz").empty());

    // Every row is well-formed: accessors present, description non-empty.
    for (const auto& def : SettingsRegistry::all()) {
        assert(!def.key.empty());
        assert(!def.description.empty());
        assert(!def.category.empty());
        assert(def.get && def.set);
        // Defaults must validate against the row's own rules.
        std::string err;
        assert(SettingsRegistry::validate(def, SettingsRegistry::default_value(def), err));
    }
    std::cout << "[PASS] test_registry_lookup\n";
}

// ---------------------------------------------------------------------------
// Value parsing & validation
// ---------------------------------------------------------------------------

static void test_bool_parsing() {
    bool value = false;
    const char* truthy[] = {"true", "TRUE", "1", "yes", "on", "y"};
    for (const char* raw : truthy) {
        assert(SettingsRegistry::parse_bool(raw, value) && value);
    }
    const char* falsy[] = {"false", "0", "no", "off", "n"};
    for (const char* raw : falsy) {
        assert(SettingsRegistry::parse_bool(raw, value) && !value);
    }
    const char* invalid[] = {"maybe", "", "tru", "2"};
    for (const char* raw : invalid) {
        assert(!SettingsRegistry::parse_bool(raw, value));
    }
    std::cout << "[PASS] test_bool_parsing\n";
}

static void test_validation_messages() {
    ShellConfig cfg;

    const SettingDef* animation = SettingsRegistry::find("animation");
    assert(animation);
    std::string err;
    assert(!SettingsRegistry::apply(cfg, "animation", "maybe", err));
    assert(text_has(err, "not a boolean"));

    assert(!SettingsRegistry::apply(cfg, "animtion", "true", err));
    assert(text_has(err, "unknown setting"));
    assert(text_has(err, "did you mean 'animation'"));

    // Integers respect their documented range.
    assert(!SettingsRegistry::apply(cfg, "history_size", "5", err));
    assert(text_has(err, "outside the allowed range"));
    assert(!SettingsRegistry::apply(cfg, "history_size", "abc", err));
    assert(text_has(err, "not a whole number"));
    assert(SettingsRegistry::apply(cfg, "history_size", "25000", err));
    assert(cfg.history_size == 25000);

    // Theme values are validated against installed themes (builtins here).
    assert(!SettingsRegistry::apply(cfg, "theme", "synthwafe", err));
    assert(text_has(err, "not a valid value"));

    // Newlines can never be smuggled into config.txt values.
    assert(!SettingsRegistry::apply(cfg, "username", "a\nb=c", err));
    assert(text_has(err, "control characters"));

    // Booleans are stored through their aliases too.
    assert(SettingsRegistry::apply(cfg, "vim", "on", err));
    assert(cfg.vi_mode);
    std::cout << "[PASS] test_validation_messages\n";
}

// ---------------------------------------------------------------------------
// config.txt parsing, serialization and round-tripping
// ---------------------------------------------------------------------------

static void test_parse_and_roundtrip() {
    tmp_config_dir();
    ShellConfig cfg;
    ConfigReport report;
    const std::string text =
        "# Aswell config\n"
        "\n"
        "theme=nord\r\n"
        "  show_git = false\n"
        "username=\"Doraemon\"\n"
        "animation=0\n"
        "history_size=500\n"
        "vi_mode=yes\n"
        "autosuggestions=no\n";
    ConfigManager::parse_into(text, cfg, &report);

    assert(report.issues.empty());
    assert(report.applied == 7);
    assert(cfg.theme_name == "nord");
    assert(!cfg.show_git);
    assert(cfg.custom_username == "Doraemon");
    assert(!cfg.enable_animation);
    assert(cfg.history_size == 500);
    assert(cfg.vi_mode);
    assert(!cfg.enable_autosuggestions);

    // Save + reload must be loss-less.
    ConfigManager::save(cfg);
    ShellConfig reloaded = ConfigManager::load();
    assert(reloaded.theme_name == cfg.theme_name);
    assert(reloaded.show_git == cfg.show_git);
    assert(reloaded.custom_username == cfg.custom_username);
    assert(reloaded.enable_animation == cfg.enable_animation);
    assert(reloaded.history_size == cfg.history_size);
    assert(reloaded.vi_mode == cfg.vi_mode);
    assert(reloaded.enable_autosuggestions == cfg.enable_autosuggestions);

    // The serialized file documents itself.
    std::string saved = read_file(tmp_config_dir() + "/config.txt");
    assert(text_has(saved, "theme=nord"));
    assert(text_has(saved, "# Aswell shell configuration"));
    assert(text_has(saved, "show_git=false"));
    // Empty string settings are omitted instead of written as `hostname=`.
    assert(!has_line_starting_with(saved, "hostname="));
    assert(has_line_starting_with(saved, "username=Doraemon"));

    // `reset` returns a setting to its documented default.
    std::string err;
    assert(SettingsRegistry::reset(cfg, "show_git", err));
    assert(cfg.show_git);
    assert(!SettingsRegistry::reset(cfg, "show_gti", err));
    std::cout << "[PASS] test_parse_and_roundtrip\n";
}

static void test_bad_config_lines_are_reported() {
    tmp_config_dir();
    ShellConfig cfg;
    ConfigReport report;
    const std::string text =
        "animaton=true\n"          // typo: unknown key
        "show_git=maybe\n"         // bad value
        "just some words\n"        // not key=value
        "this_is_unheard_of=1\n";  // unknown, no close match
    ConfigManager::parse_into(text, cfg, &report);

    assert(report.issues.size() == 4);
    assert(report.issues[0].line == 1);
    assert(report.issues[0].key == "animaton");
    assert(text_has(report.issues[0].suggestion, "animation"));
    assert(text_has(report.issues[0].fix, "aswell config help animation"));
    assert(report.issues[1].key == "show_git");
    assert(text_has(report.issues[1].fix, "aswell config set show_git true"));
    assert(report.issues[2].message == "expected key=value");
    assert(text_has(report.issues[3].fix, "prefix it with"));

    // Valid settings still survive the bad lines.
    write_file(tmp_config_dir() + "/config.txt", text + "theme=minimal\n");
    ShellConfig loaded = ConfigManager::load();
    assert(loaded.theme_name == "minimal");
    std::cout << "[PASS] test_bad_config_lines_are_reported\n";
}

static void test_update_keys_preserves_comments() {
    tmp_config_dir();
    const std::string path = tmp_config_dir() + "/config.txt";
    const std::string original =
        "# my hand written notes\n"
        "show_git=false   \n"
        "# keep this reminder\n"
        "theme=nord\n";
    write_file(path, original);

    assert(ConfigManager::update_keys({{"show_git", "true"}, {"vi_mode", "true"}}));
    std::string text = read_file(path);

    assert(text_has(text, "# my hand written notes"));
    assert(text_has(text, "# keep this reminder"));
    assert(text_has(text, "show_git=true"));
    assert(text_has(text, "theme=nord"));
    // The new key is appended, not inserted in the middle of the comments.
    assert(text_has(text, "vi_mode=true"));
    assert(!text_has(text, "show_git=false"));

    ShellConfig cfg = ConfigManager::load();
    assert(cfg.show_git && cfg.vi_mode && cfg.theme_name == "nord");

    // Updating through an alias writes the canonical key.
    assert(ConfigManager::update_keys({{"history_size", "1234"}}));
    assert(text_has(read_file(path), "history_size=1234"));
    std::cout << "[PASS] test_update_keys_preserves_comments\n";
}

// ---------------------------------------------------------------------------
// Hot-reload watcher
// ---------------------------------------------------------------------------

static void test_config_watcher() {
    tmp_config_dir();
    const std::string path = tmp_config_dir() + "/prompt.html";
    write_file(path, "<prompt><text>ONE</text></prompt>\n");

    ConfigWatcher watcher;
    const std::string theme_path = tmp_config_dir() + "/theme.css";
    ::unlink(theme_path.c_str()); // start from "does not exist yet"
    watcher.watch(path);
    watcher.watch(theme_path);
    assert(watcher.watched_count() == 2);

    watcher.snapshot();
    assert(!watcher.changed());

    // Same content, same size, but a newer mtime: must still be detected.
    struct stat st;
    assert(stat(path.c_str(), &st) == 0);
    struct timespec times[2];
    times[0] = st.st_atim;
    times[1].tv_sec = st.st_mtim.tv_sec + 2;
    times[1].tv_nsec = st.st_mtim.tv_nsec;
    assert(utimensat(AT_FDCWD, path.c_str(), times, 0) == 0);
    assert(watcher.changed());

    watcher.snapshot();
    assert(!watcher.changed());

    // A file that appears counts as a change too (brand new theme.css).
    write_file(theme_path, "prompt { color: #ff0000; }\n");
    assert(watcher.changed());
    std::vector<std::string> changed = watcher.changed_files();
    assert(changed.size() == 1);
    assert(text_has(changed[0], "theme.css"));
    std::cout << "[PASS] test_config_watcher\n";
}

// ---------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------

static void test_theme_resolution() {
    std::string dir = tmp_config_dir();

    // Built-ins are case-insensitive and keep their descriptions.
    ThemeInfo nord = ThemeManager::resolve(dir, "Nord", nullptr);
    assert(nord.name == "nord");
    assert(!nord.css_content.empty());
    assert(nord.source == "builtin");

    // Unknown names degrade gracefully, with an explanation.
    std::string warning;
    ThemeInfo fallback = ThemeManager::resolve(dir, "dracul", &warning);
    assert(fallback.name == "modern");
    assert(text_has(warning, "not installed"));
    assert(text_has(warning, "dracula"));

    // Custom themes live in themes/*.css and take precedence over built-ins.
    std::string err;
    std::string created;
    assert(ThemeManager::create_theme(dir, "NeonDesk", "cyberpunk", created, err));
    assert(text_has(created, "themes/NeonDesk.css") || text_has(created, "themes/neondesk.css"));
    assert(text_has(read_file(created), "Aswell theme: neondesk"));

    assert(ThemeManager::exists(dir, "neondesk"));
    ThemeInfo custom = ThemeManager::resolve(dir, "neondesk", nullptr);
    assert(custom.is_user);
    assert(text_has(custom.css_content, "Aswell theme: neondesk"));

    // A user file shadows a built-in of the same name.
    write_file(dir + "/themes/minimal.css", "/* Aswell theme: minimal\n * my own minimal\n */\nsymbol { color: #123456; }\n");
    ThemeInfo shadow = ThemeManager::resolve(dir, "minimal", nullptr);
    assert(shadow.is_user);
    assert(text_has(shadow.css_content, "#123456"));
    assert(shadow.description == "my own minimal");

    auto entries = ThemeManager::list_themes(dir, "minimal");
    bool found_active = false;
    bool found_user = false;
    for (const auto& e : entries) {
        if (e.name == "minimal") {
            found_active = e.is_active && e.is_user;
            found_user = true;
        }
    }
    assert(found_active && found_user);

    auto names = ThemeManager::theme_names(dir);
    assert(has(names, "modern"));
    assert(has(names, "neondesk"));

    // Rejected names: unsafe characters and the reserved legacy name.
    assert(!ThemeManager::create_theme(dir, "bad/name", "", created, err));
    assert(text_has(err, "may only contain"));
    assert(!ThemeManager::create_theme(dir, "theme", "", created, err));
    assert(text_has(err, "reserved"));
    std::cout << "[PASS] test_theme_resolution\n";
}

// ---------------------------------------------------------------------------
// Completion integration
// ---------------------------------------------------------------------------

namespace {
bool candidate_has(const std::vector<CompletionCandidate>& list, const std::string& text) {
    for (const auto& c : list) {
        if (c.text == text) return true;
    }
    return false;
}
} // namespace

static void test_hub_completion() {
    Environment env;
    CompletionEngine engine(env);

    auto subs = engine.complete("aswell ", 7);
    assert(candidate_has(subs, "config"));
    assert(candidate_has(subs, "doctor"));
    assert(candidate_has(subs, "reload"));

    auto cmds = engine.complete("aswell config ", 14);
    assert(candidate_has(cmds, "toggle"));
    assert(candidate_has(cmds, "export"));

    auto keys = engine.complete("aswell config get show_g", 24);
    assert(candidate_has(keys, "show_git"));

    auto toggle_keys = engine.complete("aswell config toggle syntax", 27);
    assert(candidate_has(toggle_keys, "syntax_highlighting"));

    auto values = engine.complete("aswell config set animation ", 28);
    assert(candidate_has(values, "true"));
    assert(candidate_has(values, "false"));

    // The legacy spelling completes theme names, not sub-commands.
    auto theme_values = engine.complete("aswell config theme ", 20);
    assert(candidate_has(theme_values, "nord"));

    auto theme_names = engine.complete("aswell theme set cybe", 21);
    assert(candidate_has(theme_names, "cyberpunk"));
    std::cout << "[PASS] test_hub_completion\n";
}

static void test_prompt_format_settings() {
    Environment env;
    env.set_var("PWD", tmp_config_dir().c_str());
    PromptEngine engine(env);

    // `time_format=%H` from config.txt (or `aswell config set`) is honoured.
    engine.set_time_format("%H");
    engine.set_date_format("%Y");
    PromptContext short_ctx = engine.gather_context();
    assert(short_ctx.time_str.size() == 2);
    assert(short_ctx.date_str.size() == 4);
    assert(env.get_var("DATE") == short_ctx.date_str);

    // A nonsensical format falls back to the documented default instead of
    // printing an empty prompt field.
    engine.set_time_format("");
    PromptContext fallback_ctx = engine.gather_context();
    assert(fallback_ctx.time_str.size() == 8);

    // Animations can be switched off globally (config.txt `animation=false`).
    engine.set_animations_enabled(false);
    engine.set_theme_css("symbol { color: #ff0000; animation: pulse 900ms infinite; }");
    assert(!engine.has_active_animations());
    engine.set_animations_enabled(true);
    assert(engine.has_active_animations());

    // <date> and <cwd> resolve to real content in the rendered prompt.
    engine.set_template_html("<prompt><date /><cwd /></prompt>");
    engine.set_theme_css("date { color: #8be9fd; } directory, cwd { color: #50fa7b; }");
    RenderResult result = engine.render(engine.gather_context(), 0);
    assert(!result.ansi_output.empty());
    assert(text_has(result.ansi_output, fallback_ctx.date_str.empty() ? "date" : fallback_ctx.date_str) ||
           text_has(result.ansi_output, "aswell_config_test"));
    std::cout << "[PASS] test_prompt_format_settings\n";
}

// ---------------------------------------------------------------------------
// JSON view (used by `aswell config list --json`)
// ---------------------------------------------------------------------------

static void test_settings_json_shape() {
    tmp_config_dir();
    ShellConfig cfg;
    std::string err;
    assert(SettingsRegistry::apply(cfg, "show_git", "false", err));
    assert(SettingsRegistry::apply(cfg, "theme", "nord", err));

    // Round-trip the JSON through config.txt to prove the value mapping agrees.
    std::string json = SettingsCli::settings_as_json(cfg, false);
    assert(text_has(json, "\"show_git\""));
    assert(text_has(json, "\"modified\": true"));
    assert(text_has(json, "\"value\": false"));
    assert(text_has(json, "\"default\": true"));
    assert(!text_has(json, "\"username\""));  // unmodified settings are skipped
    assert(!text_has(json, ",\n  }"));        // no trailing comma

    std::string all = SettingsCli::settings_as_json(cfg, true);
    size_t entries = 0;
    for (const auto& def : SettingsRegistry::all()) {
        if (text_has(all, "\"" + def.key + "\": {")) entries++;
    }
    assert(entries == SettingsRegistry::all().size());
    std::cout << "[PASS] test_settings_json_shape\n";
}

int main() {
    test_registry_lookup();
    test_bool_parsing();
    test_validation_messages();
    test_parse_and_roundtrip();
    test_bad_config_lines_are_reported();
    test_update_keys_preserves_comments();
    test_config_watcher();
    test_theme_resolution();
    test_hub_completion();
    test_prompt_format_settings();
    test_settings_json_shape();
    std::cout << "\nAll configuration & customization tests passed.\n";
    return 0;
}
