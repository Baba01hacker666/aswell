#include "aswell/config/settings_cli.hpp"
#include "aswell/config/config.hpp"
#include "aswell/config/settings.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/config/config_editor.hpp"
#include "aswell/ui/animation.hpp"
#include "aswell/ui/css_parser.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/ui/terminal.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

namespace aswell {
namespace {

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[90m";
constexpr const char* kGreen = "\033[1;32m";
constexpr const char* kRed = "\033[1;31m";
constexpr const char* kYellow = "\033[1;33m";
constexpr const char* kCyan = "\033[1;36m";
constexpr const char* kMagenta = "\033[1;35m";

size_t width_of(const std::string& s) {
    return str_util::visual_width(s);
}

std::string pad_right(const std::string& s, size_t width) {
    if (width_of(s) >= width) return s;
    return s + std::string(width - width_of(s), ' ');
}

std::string read_text(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Absolute paths of the non-hidden entries in `dir` matching `suffix`
// (empty suffix = every regular file).
std::vector<std::string> list_directory(const std::string& dir, const std::string& suffix) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name.empty() || name.front() == '.') continue;
        if (!suffix.empty() && !str_util::ends_with(name, suffix)) continue;
        std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        out.push_back(full);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

std::string current_executable_path() {
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf, static_cast<size_t>(n));
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out;
}

const char* type_name(SettingType type) {
    switch (type) {
        case SettingType::BOOL: return "bool";
        case SettingType::INT: return "int";
        case SettingType::ENUM: return "enum";
        case SettingType::STRING: return "text";
    }
    return "text";
}

std::string quoted_list(const std::vector<std::string>& items, const char* separator = ", ") {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += separator;
        out += items[i];
    }
    return out;
}

// Prints a value the way it should appear in a table (empty strings get a
// visible placeholder so "unset" never looks like a rendering bug).
std::string printable_value(const SettingDef& def, const std::string& value) {
    if (value.empty()) {
        return def.type == SettingType::STRING ? "(default)" : "(empty)";
    }
    return value;
}

// The theme the *next prompt* will use: an ASWELL_THEME export overrides the
// stored setting for one session, and `theme list`/`preview` must agree with it.
std::string active_theme_name(const Environment& env) {
    std::string override_value = str_util::to_lower(str_util::trim(env.get_var("ASWELL_THEME")));
    if (!override_value.empty()) return override_value;
    return ConfigManager::load().theme_name;
}

void report_error(const std::string& message) {
    std::cerr << "aswell: " << message << "\n";
}

// `aswell config username` / `aswell config hostname` without a value keep the
// older, friendlier wording: show the effective identity and its source.
int print_effective_identity(const std::string& key, const Environment& env) {
    ShellConfig cfg = ConfigManager::load();
    const SettingDef* def = SettingsRegistry::find(key);
    if (!def) return 1;
    std::string custom = def->get(cfg);
    std::string fallback = (key == "username") ? env.get_var("USER") : env.get_var("HOSTNAME");
    if (fallback.empty()) fallback = (key == "username") ? "user" : "localhost";

    std::cout << "Current " << ((key == "username") ? "username" : "hostname") << ": " << kBold
              << (custom.empty() ? fallback : custom) << kReset;
    if (custom.empty()) std::cout << kDim << " (from the system, not overridden)" << kReset;
    std::cout << "\n  change: aswell config set " << key << " <name>\n";
    if (!custom.empty()) std::cout << "  revert: aswell config unset " << key << "\n";
    return 0;
}

bool write_key(const std::vector<std::pair<std::string, std::string>>& updates, std::string& err) {
    if (!ConfigManager::update_keys(updates)) {
        err = "could not write " + ConfigManager::get_config_dir() + "/config.txt";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// shared: mutate one setting, persist it, and push it into the live shell
// ---------------------------------------------------------------------------
// `aswell config username default` has always meant "stop overriding"; keep
// that behaviour for free-text settings.
bool is_reset_word(const std::string& value) {
    std::string v = str_util::to_lower(str_util::trim(value));
    return v == "default" || v == "reset" || v == "none" || v == "\"\"" || v == "''";
}

int set_and_save(const std::string& key, const std::string& value, Environment& env, bool quiet) {
    ShellConfig cfg = ConfigManager::load();
    std::string err;
    const SettingDef* probe = SettingsRegistry::find(key);
    std::string requested = (probe && probe->type == SettingType::STRING && is_reset_word(value)) ? "" : value;
    if (!SettingsRegistry::apply(cfg, key, requested, err)) {
        report_error(err);
        return 1;
    }

    const SettingDef* def = SettingsRegistry::find(key);
    if (!def) {
        report_error("unknown setting '" + key + "'");
        return 1;
    }
    std::string canonical = def->get(cfg);

    if (!write_key({{def->key, canonical}}, err)) {
        report_error(err);
        return 1;
    }

    if (env.config_reload) env.config_reload(false);

    if (!quiet) {
        std::cout << kGreen << "\xe2\x9c\x93" << kReset << " " << def->key << " = "
                  << kBold << (canonical.empty() ? "(default)" : canonical) << kReset;
        if (env.config_reload) {
            std::cout << "  " << kDim << "(applied to this session and saved)" << kReset;
        } else {
            std::cout << "  " << kDim << "(saved)" << kReset;
        }
        std::cout << "\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// aswell config
// ---------------------------------------------------------------------------
void print_list(bool show_advanced, bool show_all_values) {
    ShellConfig cfg = ConfigManager::load();
    std::cout << kCyan << "Aswell settings" << kReset << "  " << kDim << ConfigManager::get_config_dir()
              << "/config.txt" << kReset << "\n";

    size_t modified = 0;
    for (const auto& def : SettingsRegistry::all()) {
        std::string value = def.get(cfg);
        bool changed = SettingsRegistry::is_modified(def, cfg);
        if (!show_all_values && !changed) continue;
        if (!show_advanced && def.advanced && !changed) continue;
        if (changed) modified++;

        std::string line = "  ";
        line += changed ? (kYellow + std::string("*") + kReset) : " ";
        line += " " + pad_right(def.key, 23);
        line += kBold + pad_right(printable_value(def, value), 16) + kReset;
        line += kDim + def.description + kReset;
        if (changed && show_all_values) {
            line += std::string("  ") + kDim + "(default: " + printable_value(def, SettingsRegistry::default_value(def)) + ")" + kReset;
        }
        std::cout << line << "\n";
    }

    if (modified == 0 && !show_all_values) {
        std::cout << "  " << kDim << "everything is at its default value" << kReset << "\n";
    }
    std::cout << "\n"
              << kDim << "  change: aswell config set <key> <value>   toggle: aswell config toggle <key>"
                 "\n  docs:   aswell config help          or:      aswell config            (visual editor)"
              << kReset << "\n";
}

void print_help_for(const std::string& key) {
    const SettingDef* def = SettingsRegistry::find(key);
    if (!def) {
        report_error("unknown setting '" + key + "'");
        std::string hint = SettingsRegistry::suggest(key);
        if (!hint.empty()) std::cout << "  did you mean 'aswell config help " << hint << "'?\n";
        return;
    }

    ShellConfig cfg = ConfigManager::load();
    std::cout << kBold << def->key << kReset << "  " << kDim << "[" << def->category << "]" << kReset << "\n";
    std::cout << "  " << def->description << "\n";
    std::cout << "  type:     " << type_name(def->type) << "\n";
    std::cout << "  default:  " << printable_value(*def, SettingsRegistry::default_value(*def)) << "\n";
    std::cout << "  current:  " << printable_value(*def, def->get(cfg)) << "\n";
    if (!def->aliases.empty()) {
        std::cout << "  aliases:  " << quoted_list(def->aliases) << " (both accepted in config.txt)\n";
    }
    if (def->type == SettingType::BOOL) {
        std::cout << "  values:   true | false | yes | no | 1 | 0 | on | off\n";
    } else if (def->type == SettingType::INT) {
        std::cout << "  values:   " << def->min_value << ".." << def->max_value;
        if (!def->units.empty()) std::cout << " " << def->units;
        std::cout << "\n";
    } else {
        std::vector<std::string> options = SettingsRegistry::choices(*def);
        if (!options.empty()) {
            std::cout << "  values:   " << quoted_list(options) << "\n";
        }
    }
    std::cout << "  set with: aswell config set " << def->key << " <value>\n";
    std::cout << "  in file:  " << def->key << "=<value>  in ~/.config/aswell/config.txt\n";
}

void print_full_help() {
    std::cout << kBold << "aswell config" << kReset << " — every customization knob of the shell, in one place\n\n";
    std::cout << "Usage:\n"
              << "  aswell config                       open the visual editor (TUI)\n"
              << "  aswell config list [--all] [--json]   show current settings\n"
              << "  aswell config set <key> <value>       change one setting (or: set key=value ...)\n"
              << "  aswell config get <key>               print just the value (script friendly)\n"
              << "  aswell config toggle <key>            flip a boolean\n"
              << "  aswell config unset <key>             restore one default\n"
              << "  aswell config reset                   restore every default\n"
              << "  aswell config show                    print config.txt as it is on disk\n"
              << "  aswell config export [--to FILE]      dump a shareable config file\n"
              << "  aswell config import FILE             validate + install a config file\n"
              << "  aswell config path                    print the config directory\n"
              << "  aswell config help [key]              document all settings, or one of them\n\n";

    for (const auto& category : SettingsRegistry::categories()) {
        std::cout << kCyan << category << kReset << "\n";
        for (const auto& def : SettingsRegistry::all()) {
            if (def.category != category) continue;
            std::cout << "  " << pad_right(def.key, 23) << kDim
                      << printable_value(def, SettingsRegistry::default_value(def)) << kReset << "  "
                      << pad_right("", 1) << def.description << "\n";
        }
        std::cout << "\n";
    }
}

int handle_config_show() {
    std::string dir = ConfigManager::get_config_dir();
    std::string path = dir + "/config.txt";
    std::ifstream f(path);
    if (!f) {
        std::cout << kDim << "no config file yet: " << path
                  << "\nAswell runs on defaults; create one with `aswell config set <key> <value>`." << kReset << "\n";
        return 0;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::cout << kDim << path << kReset << "\n";
    std::cout << ss.str();
    return 0;
}

int handle_config_export(const std::vector<std::string>& args, Environment& env) {
    (void)env;
    ShellConfig cfg = ConfigManager::load();
    std::string text = ConfigManager::serialize(cfg);

    std::string target;
    for (size_t i = 2; i < args.size(); ++i) {   // args: config export [flags]
        if ((args[i] == "--to" || args[i] == "-o") && i + 1 < args.size()) target = args[++i];
    }
    if (!target.empty()) {
        std::ofstream out(target, std::ios::trunc);
        if (!out) {
            report_error("could not write " + target);
            return 1;
        }
        out << text;
        std::cout << kGreen << "\xe2\x9c\x93" << kReset << " wrote " << target << "\n";
        return 0;
    }
    std::cout << text;
    return 0;
}

int handle_config_import(const std::vector<std::string>& args, Environment& env) {
    if (args.size() < 3) {
        report_error("config import needs a file: aswell config import <file>");
        return 2;
    }
    std::string source = args[2];
    std::ifstream in(source);
    if (!in) {
        report_error("cannot read " + source);
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();

    ShellConfig cfg;
    ConfigReport report;
    ConfigManager::parse_into(ss.str(), cfg, &report);

    for (const auto& issue : report.issues) {
        std::cout << kYellow << "!" << kReset << " " << source << ":" << issue.line << ": " << issue.key
                  << " — " << issue.message;
        if (!issue.suggestion.empty()) std::cout << kDim << " (" << issue.suggestion << ")" << kReset;
        std::cout << "\n";
    }
    if (report.applied == 0) {
        report_error("no valid settings found in " + source);
        return 1;
    }

    ConfigManager::save(cfg);
    if (env.config_reload) env.config_reload(false);
    std::cout << kGreen << "\xe2\x9c\x93" << kReset << " imported " << report.applied << " setting"
              << (report.applied == 1 ? "" : "s") << " from " << source << " into "
              << ConfigManager::get_config_dir() << "/config.txt";
    if (!report.issues.empty()) {
        std::cout << kDim << " (" << report.issues.size() << " line(s) skipped)" << kReset;
    }
    std::cout << "\n";
    return report.issues.empty() ? 0 : 1;
}

int handle_reset(Environment& env, const std::string& key) {
    if (!key.empty()) {
        std::string err;
        ShellConfig cfg = ConfigManager::load();
        if (!SettingsRegistry::reset(cfg, key, err)) {
            report_error(err);
            return 1;
        }
        const SettingDef* def = SettingsRegistry::find(key);
        if (!def) return 1;
        std::string canonical = def->get(cfg);
        if (!write_key({{def->key, canonical}}, err)) {
            report_error(err);
            return 1;
        }
        if (env.config_reload) env.config_reload(false);
        std::cout << kGreen << "\xe2\x9c\x93" << kReset << " " << def->key << " restored to its default: "
                  << kBold << printable_value(*def, canonical) << kReset << "\n";
        return 0;
    }

    ShellConfig fresh;
    ConfigManager::save(fresh);
    if (env.config_reload) env.config_reload(false);
    std::cout << kGreen << "\xe2\x9c\x93" << kReset << " all settings restored to Aswell defaults\n";
    return 0;
}

} // namespace

// args follows argv style: args[0] == "config", args[1] == sub-command.
int SettingsCli::handle_config(const std::vector<std::string>& args, Environment& env) {
    std::string sub = args.size() >= 2 ? args[1] : "";

    if (sub.empty()) {
        // `aswell config` alone: open the visual editor when a terminal is
        // attached (interactive shell or direct CLI run), otherwise summarise.
        if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
            int rc = ConfigEditor::run_interactive(env);
            if (env.config_reload) env.config_reload(false);
            return rc;
        }
        print_list(false, false);
        return 0;
    }

    if (sub == "list" || sub == "ls" || sub == "status") {
        bool show_all = false;
        bool as_json = false;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--all" || args[i] == "-a") show_all = true;
            else if (args[i] == "--json") as_json = true;
        }
        if (as_json) {
            std::cout << settings_as_json(ConfigManager::load(), true) << "\n";
            return 0;
        }
        print_list(show_all, show_all);
        return 0;
    }

    if (sub == "get") {
        if (args.size() < 3) {
            report_error("config get needs a setting name: aswell config get <key>");
            return 2;
        }
        const SettingDef* def = SettingsRegistry::find(args[2]);
        if (!def) {
            report_error("unknown setting '" + args[2] + "'");
            std::string hint = SettingsRegistry::suggest(args[2]);
            if (!hint.empty()) std::cout << "  did you mean '" << hint << "'?\n";
            return 1;
        }
        ShellConfig cfg = ConfigManager::load();
        if (args.size() == 3) {
            std::cout << def->get(cfg) << "\n";
            return 0;
        }
        // Several keys at once: emit `key=value` lines (eval / grep friendly).
        for (size_t i = 2; i < args.size(); ++i) {
            const SettingDef* one = SettingsRegistry::find(args[i]);
            if (!one) {
                report_error("unknown setting '" + args[i] + "'");
                return 1;
            }
            std::cout << one->key << "=" << one->get(cfg) << "\n";
        }
        return 0;
    }

    if (sub == "set" || sub == "assign") {
        if (args.size() < 3) {
            report_error("config set needs a value: aswell config set <key> <value>");
            return 2;
        }
        // Two spellings: `set <key> <value>` and `set key=value [key=value...]`.
        std::vector<std::pair<std::string, std::string>> pairs;
        if (args[2].find('=') != std::string::npos) {
            for (size_t i = 2; i < args.size(); ++i) {
                size_t eq = args[i].find('=');
                pairs.emplace_back(args[i].substr(0, eq), args[i].substr(eq + 1));
            }
        } else if (args.size() >= 4) {
            std::string value = args[3];
            for (size_t i = 4; i < args.size(); ++i) value += " " + args[i];
            pairs.emplace_back(args[2], value);
        } else {
            report_error("config set needs a value: aswell config set " + args[2] + " <value>");
            return 2;
        }

        if (pairs.size() == 1) {
            return set_and_save(pairs[0].first, pairs[0].second, env, false);
        }

        // Several keys at once: validate everything first, then write once, so a
        // bad value in the middle cannot leave a half-applied configuration.
        ShellConfig cfg = ConfigManager::load();
        std::vector<std::pair<std::string, std::string>> updates;
        for (const auto& pair : pairs) {
            std::string err;
            if (!SettingsRegistry::apply(cfg, pair.first, pair.second, err)) {
                report_error(err);
                return 1;
            }
            const SettingDef* def = SettingsRegistry::find(pair.first);
            if (!def) return 1;
            updates.emplace_back(def->key, def->get(cfg));
        }
        std::string err;
        if (!write_key(updates, err)) {
            report_error(err);
            return 1;
        }
        if (env.config_reload) env.config_reload(false);
        std::cout << kGreen << "\xe2\x9c\x93" << kReset << " saved " << updates.size() << " settings\n";
        for (const auto& update : updates) {
            std::cout << "    " << pad_right(update.first, 23) << kBold << update.second << kReset << "\n";
        }
        return 0;
    }

    if (sub == "toggle" || sub == "flip") {
        if (args.size() < 3) {
            report_error("config toggle needs a setting name: aswell config toggle <key>");
            return 2;
        }
        const SettingDef* def = SettingsRegistry::find(args[2]);
        if (!def) {
            report_error("unknown setting '" + args[2] + "'");
            std::string hint = SettingsRegistry::suggest(args[2]);
            if (!hint.empty()) std::cout << "  did you mean '" << hint << "'?\n";
            return 1;
        }
        if (def->type != SettingType::BOOL) {
            report_error(def->key + " is not a boolean setting (use: aswell config set " + def->key + " <value>)");
            return 1;
        }
        ShellConfig cfg = ConfigManager::load();
        bool on = (def->get(cfg) == "true");
        return set_and_save(def->key, on ? "false" : "true", env, false);
    }

    if (sub == "unset" || sub == "reset") {
        std::string key = (args.size() >= 3) ? args[2] : "";
        return handle_reset(env, key);
    }

    if (sub == "show") return handle_config_show();
    if (sub == "export" || sub == "dump") return handle_config_export(args, env);
    if (sub == "import") return handle_config_import(args, env);

    if (sub == "help" || sub == "--help" || sub == "-h") {
        if (args.size() >= 3) print_help_for(args[2]);
        else print_full_help();
        return 0;
    }

    if (sub == "edit" || sub == "tui" || sub == "editor") {
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            std::cout << "The visual editor needs a terminal. Edit the file directly:\n"
                      << "  " << ConfigManager::get_config_dir() << "/config.txt\n"
                      << "or use: aswell config set <key> <value>\n";
            return 0;
        }
        int rc = ConfigEditor::run_interactive(env);
        if (env.config_reload) env.config_reload(false);
        return rc;
    }

    if (sub == "path" || sub == "dir") {
        std::cout << ConfigManager::get_config_dir() << "\n";
        return 0;
    }

    // Legacy / convenience forms kept working: username, hostname, theme.
    if (sub == "username" || sub == "user") {
        if (args.size() < 3) return print_effective_identity("username", env);
        return set_and_save("username", args[2], env, false);
    }
    if (sub == "hostname" || sub == "host") {
        if (args.size() < 3) return print_effective_identity("hostname", env);
        return set_and_save("hostname", args[2], env, false);
    }
    if (sub == "theme") {
        // Legacy shapes kept working: `aswell config theme`,
        // `aswell config theme <name>` and `aswell config theme set <name>`.
        size_t name_idx = (args.size() >= 4 && args[3] == "set") ? 4 : 2;
        if (args.size() <= name_idx) return SettingsCli::handle_theme({"theme", "list"}, env);
        return SettingsCli::handle_theme({"theme", "set", args[name_idx]}, env);
    }

    report_error("unknown config command '" + sub + "'");
    std::cout << "Try: aswell config list | help | set <key> <value> | get <key> | toggle <key> | path\n";
    return 1;
}

// ---------------------------------------------------------------------------
// aswell theme
// ---------------------------------------------------------------------------
namespace {

// A believable prompt for previews: the user's real identity and configured
// time format, with a demo directory, a dirty branch and (optionally) a failed,
// slow command so `status.error`, `git.dirty` and `runtime` styling all show.
PromptContext sample_context(PromptEngine& engine, bool failure) {
    PromptContext ctx = engine.gather_context(failure ? 1875.0 : 12.0, failure ? 1 : 0, false);
    ctx.cwd = "~/projects/aswell";
    ctx.git_branch = "feat/customization";
    ctx.git_dirty = true;
    if (failure) ctx.last_status = 1;
    return ctx;
}

std::string preview_template_html(const std::string& config_dir, const ShellConfig& cfg) {
    std::ifstream f(config_dir + "/prompt.html");
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        std::string text = ss.str();
        if (text.find("<prompt") != std::string::npos) return text;
    }
    return ConfigManager::build_template(cfg);
}

void render_static_preview(PromptEngine& engine, const PromptContext& ctx, const std::string& label) {
    RenderResult result = engine.render(ctx, AnimationEngine::now_ms());
    if (!result.statusbar_ansi.empty()) std::cout << result.statusbar_ansi << "\n";
    std::cout << result.ansi_output << "$ ls --color=auto\n";
    if (!label.empty()) std::cout << kDim << "  " << label << kReset << "\n";
}

int theme_preview(const std::vector<std::string>& args, Environment& env) {
    std::string config_dir = ConfigManager::get_config_dir();
    ShellConfig cfg = ConfigManager::load();

    bool all = false;
    bool animate = false;
    std::string name;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--all" || args[i] == "-a") all = true;
        else if (args[i] == "--animate" || args[i] == "--live") animate = true;
        else if (!args[i].empty() && args[i][0] != '-') name = args[i];
    }

    std::vector<std::string> wanted;
    if (all) {
        wanted = ThemeManager::theme_names(config_dir);
    } else {
        if (name.empty()) name = active_theme_name(env);
        if (!ThemeManager::exists(config_dir, name)) {
            report_error("unknown theme '" + name + "'");
            std::string hint = ThemeManager::suggest(config_dir, name);
            if (!hint.empty()) std::cout << "  did you mean '" << hint << "'?";
            std::cout << kDim << "  list them with: aswell theme list" << kReset << "\n";
            return 1;
        }
        wanted.push_back(name);
    }

    std::cout << kDim << "Live preview — your current prompt template, styled per theme" << kReset << "\n";
    for (const auto& theme_name : wanted) {
        std::string warning;
        ThemeInfo info = ThemeManager::resolve(config_dir, theme_name, &warning);
        PromptEngine engine(env);
        engine.set_animations_enabled(cfg.enable_animation);
        engine.set_time_format(cfg.time_format);
        engine.set_theme_css(info.css_content);
        engine.set_template_html(preview_template_html(config_dir, cfg));
        if (!cfg.custom_username.empty()) engine.set_custom_user(cfg.custom_username);
        if (!cfg.custom_hostname.empty()) engine.set_custom_hostname(cfg.custom_hostname);

        std::cout << "\n" << kMagenta << "\xe2\x96\xa0 " << info.name << kReset
                  << kDim << "  " << info.description << kReset;
        std::cout << "\n";
        if (!warning.empty()) std::cout << kYellow << "! " << warning << kReset << "\n";

        if (animate && isatty(STDOUT_FILENO)) {
            auto start = std::chrono::steady_clock::now();
            int lines = 2;
            bool first = true;
            while (true) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
                if (elapsed > 2600) break;
                RenderResult result = engine.render(sample_context(engine, false),
                                                     static_cast<uint64_t>(elapsed));
                lines = result.total_lines > 0 ? result.total_lines : 1;
                if (!first) std::cout << "\r\033[" << lines << "A";
                first = false;
                std::cout << "\033[J" << result.ansi_output << "$ ls --color=auto\n";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(45));
            }
            std::cout << "\n";
        } else {
            render_static_preview(engine, sample_context(engine, false),
                                  animate ? "animations need a terminal — run inside a shell" : "");
            if (all) {
                render_static_preview(engine, sample_context(engine, true), "");
            }
        }
    }

    if (!all) {
        std::cout << "\n" << kDim << "  use it: aswell theme set " << (name.empty() ? cfg.theme_name : name)
                  << "\n  tweak it: " << (config_dir + "/themes/" + (name.empty() ? cfg.theme_name : name) + ".css")
                  << "   (created by: aswell theme new " << (name.empty() ? cfg.theme_name : name) << ")"
                  << kReset << "\n";
    }
    return 0;
}

} // namespace

int SettingsCli::handle_theme(const std::vector<std::string>& args, Environment& env) {
    std::string config_dir = ConfigManager::get_config_dir();
    std::string sub = args.size() >= 2 ? args[1] : "list";

    if (sub == "list" || sub == "ls") {
        auto entries = ThemeManager::list_themes(config_dir, active_theme_name(env));
        std::cout << kBold << "Aswell themes" << kReset << kDim << "  (" << entries.size() << " available)" << kReset << "\n";
        for (const auto& e : entries) {
            std::cout << (e.is_active ? std::string(kGreen) + "\xe2\x9c\x93 " + kReset : std::string("  "))
                      << (e.is_active ? kBold : "") << pad_right(e.name, 15) << (e.is_active ? kReset : "")
                      << kDim << pad_right(e.is_user ? "custom" : "builtin", 9) << kReset << e.description << "\n";
        }
        std::cout << "\n" << kDim
                  << "  try: aswell theme preview <name>      switch: aswell theme set <name>\n"
                     "  make your own: aswell theme new <name>"
                  << kReset << "\n";
        return 0;
    }

    if (sub == "preview" || sub == "demo") {
        return theme_preview(args, env);
    }

    if (sub == "show") {
        std::string name = args.size() >= 3 ? args[2] : active_theme_name(env);
        ThemeInfo info = ThemeManager::resolve(config_dir, name, nullptr);
        std::cout << kDim << "theme '" << info.name << "'  source: " << info.source << kReset << "\n\n";
        std::cout << info.css_content;
        if (!info.css_content.empty() && info.css_content.back() != '\n') std::cout << "\n";
        return 0;
    }

    if (sub == "new" || sub == "create") {
        if (args.size() < 3) {
            report_error("theme new needs a name: aswell theme new <name> [--from <theme>] [--force]");
            return 2;
        }
        std::string name = args[2];
        std::string from;
        bool force = false;
        for (size_t i = 3; i < args.size(); ++i) {
            if (args[i] == "--force" || args[i] == "-f") force = true;
            else if ((args[i] == "--from" || args[i] == "--from-theme") && i + 1 < args.size()) from = args[++i];
        }
        std::string path;
        std::string err;
        if (force) {
            std::string candidate = config_dir + "/themes/" + str_util::to_lower(name) + ".css";
            ::unlink(candidate.c_str());
        }
        if (!ThemeManager::create_theme(config_dir, name, from, path, err)) {
            report_error(err);
            return 1;
        }
        std::cout << kGreen << "\xe2\x9c\x93" << kReset << " created " << path
                  << (from.empty() ? "" : (" (from '" + from + "')")) << "\n";
        std::cout << "  edit it:   " << kDim << "$EDITOR " << path << kReset << "\n";
        std::cout << "  it becomes active with: aswell theme set " << str_util::to_lower(name) << "\n";
        std::cout << kDim << "  with auto_reload=true the shell re-reads it on your very next prompt" << kReset << "\n";
        return 0;
    }

    if (sub == "reset") {
        return set_and_save("theme", "modern", env, false);
    }

    std::string name = sub;
    if (sub == "set") {
        if (args.size() < 3) {
            report_error("theme set needs a name: aswell theme set <name>");
            return 2;
        }
        name = args[2];
    }
    if (name.empty() || name == "help" || name == "--help") {
        std::cout << "Usage: aswell theme [list | set <name> | preview [name|--all] [--animate] | show [name]"
                     " | new <name> [--from t] | reset]\n";
        return 0;
    }

    std::string normalized = str_util::to_lower(str_util::trim(name));
    if (!ThemeManager::exists(config_dir, normalized)) {
        report_error("unknown theme '" + normalized + "'");
        std::string hint = ThemeManager::suggest(config_dir, normalized);
        if (!hint.empty()) std::cout << "  did you mean 'aswell theme set " << hint << "'?\n";
        std::cout << kDim << "  available: " << quoted_list(ThemeManager::theme_names(config_dir), ", ") << kReset << "\n";
        return 1;
    }
    return set_and_save("theme", normalized, env, false);
}

// ---------------------------------------------------------------------------
// aswell reload
// ---------------------------------------------------------------------------
int SettingsCli::handle_reload(const std::vector<std::string>& args, Environment& env) {
    (void)args;
    if (env.config_reload) {
        env.config_reload(true);
        return 0;
    }
    std::cout << "aswell: settings saved on disk; this session has no live prompt to refresh.\n";
    return 0;
}

// ---------------------------------------------------------------------------
// aswell doctor
// ---------------------------------------------------------------------------
namespace {

struct DoctorNote {
    std::string text;
    std::string fix;
};

struct Doctor {
    std::vector<std::string> ok;
    std::vector<DoctorNote> hints;
    std::vector<DoctorNote> problems;

    void good(const std::string& text) { ok.push_back(text); }
    void hint(const std::string& text, const std::string& fix = "") { hints.push_back({text, fix}); }
    void problem(const std::string& text, const std::string& fix = "") { problems.push_back({text, fix}); }
};

void print_notes(const std::vector<DoctorNote>& notes, const char* glyph, const char* color) {
    for (const auto& n : notes) {
        std::cout << color << glyph << kReset << " " << n.text;
        if (!n.fix.empty()) std::cout << "\n    " << kDim << "\xe2\x86\x92 fix: " << n.fix << kReset;
        std::cout << "\n";
    }
}

// Minimal CSS block scanner used for validation only (the renderer stays
// deliberately forgiving so a bad rule can never take the prompt down).
void validate_css(const std::string& css, const std::string& path, Doctor& doc) {
    int depth = 0;
    size_t line = 1;
    size_t block_start_line = 1;
    std::string selector;

    for (size_t i = 0; i < css.size(); ++i) {
        char c = css[i];
        if (c == '\n') {
            line++;
            continue;
        }
        if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') {
            size_t end = css.find("*/", i + 2);
            if (end == std::string::npos) {
                doc.problem(path + ":" + std::to_string(line) + ": unterminated /* comment", "close it with */");
                return;
            }
            for (size_t j = i; j < end && j < css.size(); ++j) {
                if (css[j] == '\n') line++;
            }
            i = end + 1;
            continue;
        }
        if (c == '{') {
            if (depth > 0) {
                doc.problem(path + ":" + std::to_string(line) + ": nested rule '" + str_util::trim(selector) +
                                "' is not supported",
                            "move it out of the previous block");
            }
            depth++;
            block_start_line = line;
            selector.clear();
            continue;
        }
        if (c == '}') {
            if (depth == 0) {
                doc.problem(path + ":" + std::to_string(line) + ": stray '}' without a matching '{'",
                            "remove it or add the missing selector");
                continue;
            }
            depth--;
            continue;
        }
        if (depth > 0) {
            if (c == ';') {
                std::string decl = str_util::trim(selector);
                selector.clear();
                size_t colon = decl.find(':');
                if (colon == std::string::npos) {
                    if (!decl.empty()) {
                        doc.problem(path + ":" + std::to_string(line) + ": '" + decl + "' is missing a ':' value",
                                    "e.g. color: #8be9fd;");
                    }
                    continue;
                }
                std::string prop = str_util::trim(decl.substr(0, colon));
                if (!CSSParser::is_supported_property(prop)) {
                    std::string best;
                    int best_dist = 4;
                    for (const auto& supported : CSSParser::supported_properties()) {
                        int dist = str_util::levenshtein_distance(str_util::to_lower(prop), supported);
                        if (dist < best_dist) {
                            best_dist = dist;
                            best = supported;
                        }
                    }
                    doc.problem(path + ":" + std::to_string(line) + ": '" + prop + "' is not a property Aswell knows",
                                best.empty() ? "see docs/CSS_REFERENCE.md for the list" : "did you mean '" + best + "'?");
                }
                continue;
            }
            selector += c;
        }
    }

    if (depth != 0) {
        doc.problem(path + ": " + std::to_string(depth) + " unclosed '{' (starting near line " +
                        std::to_string(block_start_line) + ")",
                    "add the missing '}'");
    }
}

void validate_prompt_html(const std::string& html, const std::string& path, Doctor& doc) {
    std::vector<std::string> open_stack;
    size_t line = 1;

    for (size_t i = 0; i < html.size(); ++i) {
        if (html[i] == '\n') {
            line++;
            continue;
        }
        if (html[i] != '<') continue;
        size_t end = html.find('>', i);
        if (end == std::string::npos) {
            doc.problem(path + ":" + std::to_string(line) + ": '<' without a closing '>'", "close the tag");
            return;
        }
        std::string inner = html.substr(i + 1, end - i - 1);
        bool closing = !inner.empty() && inner.front() == '/';
        if (closing) inner = inner.substr(1);
        bool self_closing = !inner.empty() && inner.back() == '/';
        if (self_closing) inner.pop_back();

        size_t name_end = 0;
        while (name_end < inner.size() &&
               (std::isalnum(static_cast<unsigned char>(inner[name_end])) || inner[name_end] == '-' ||
                inner[name_end] == '_')) {
            name_end++;
        }
        std::string tag = str_util::to_lower(inner.substr(0, name_end));
        if (tag.empty()) {
            doc.problem(path + ":" + std::to_string(line) + ": empty '<>'", "remove it");
            i = end;
            continue;
        }

        bool known = false;
        for (const auto& t : PromptEngine::known_tags()) {
            if (t == tag) {
                known = true;
                break;
            }
        }
        if (!known) {
            std::string best;
            int best_dist = 4;
            for (const auto& t : PromptEngine::known_tags()) {
                int dist = str_util::levenshtein_distance(tag, t);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = t;
                }
            }
            doc.hint(path + ":" + std::to_string(line) + ": unknown element <" + tag +
                         "> renders its raw text content",
                     best.empty() ? "known elements: " + quoted_list(PromptEngine::known_tags(), ", ")
                                  : "did you mean <" + best + ">?");
        }

        if (closing) {
            if (open_stack.empty() || open_stack.back() != tag) {
                doc.problem(path + ":" + std::to_string(line) + ": </" + tag + "> does not close <" +
                                (open_stack.empty() ? std::string("nothing") : open_stack.back()) + ">",
                            "match every opening tag with a closing one (or use <" + tag + " />)");
                if (!open_stack.empty()) open_stack.pop_back();
            } else {
                open_stack.pop_back();
            }
        } else if (!self_closing) {
            open_stack.push_back(tag);
        }
        i = end;
    }

    for (const auto& leftover : open_stack) {
        doc.problem(path + ": <" + leftover + "> is never closed", "add </" + leftover + ">");
    }
}

} // namespace

int SettingsCli::handle_doctor(const std::vector<std::string>& args, Environment& env) {
    bool quiet = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--quiet" || args[i] == "-q") quiet = true;
    }

    Doctor doc;
    std::string config_dir = ConfigManager::get_config_dir();

    // 1. Config directory & file
    struct stat st;
    if (stat(config_dir.c_str(), &st) != 0) {
        doc.hint("no config directory yet (" + config_dir + ") — Aswell is running on defaults",
                 "aswell config set theme nord   (creates the directory and file)");
    } else if (access(config_dir.c_str(), W_OK) != 0) {
        doc.problem(config_dir + " is not writable — 'aswell config set' cannot save",
                    "chmod u+w " + config_dir);
    } else {
        doc.good("config directory is present and writable: " + config_dir);
    }

    const char* sub_dirs[] = {"themes", "plugins", "commands", "templates"};
    for (const char* sub : sub_dirs) {
        std::string path = config_dir + "/" + sub;
        if (stat(path.c_str(), &st) == 0) {
            doc.good(std::string(path) + " exists");
        } else {
            doc.hint("no " + std::string(sub) + "/ directory (" + path + ")",
                     "mkdir -p " + path);
        }
    }

    // 2. config.txt: unknown keys, bad values
    ConfigReport report;
    ShellConfig cfg = ConfigManager::load(&report);
    std::string cfg_path = config_dir + "/config.txt";
    if (stat(cfg_path.c_str(), &st) != 0) {
        doc.hint("no config.txt yet — every setting uses its documented default",
                 "aswell config export --to " + cfg_path);
    } else if (!report.issues.empty()) {
        for (const auto& issue : report.issues) {
            std::string text = cfg_path + ":" + std::to_string(issue.line) + ": " + issue.key;
            if (!issue.value.empty()) text += "=" + issue.value;
            text += " \xe2\x80\x94 " + issue.message;
            std::string fix = issue.fix;
            if (fix.empty()) fix = issue.suggestion;
            doc.problem(text, fix);
        }
    } else {
        doc.good("config.txt parsed cleanly (" + std::to_string(report.applied) + " setting(s) applied)");
    }

    // 3. Theme stylesheet
    std::string theme_warning;
    std::string effective_theme = active_theme_name(env);
    ThemeInfo theme = ThemeManager::resolve(config_dir, effective_theme, &theme_warning);
    if (!theme_warning.empty()) {
        doc.problem(theme_warning, "aswell theme list   then: aswell theme set <name>");
    } else if (theme.css_content.find('{') == std::string::npos) {
        doc.problem("theme '" + theme.name + "' contains no CSS rules (the prompt renders unstyled)",
                    "aswell theme new " + theme.name + " --force");
    } else {
        doc.good("theme '" + theme.name + "' loaded from " + theme.source);
    }
    if (!theme.is_user) {
        std::string user_theme_path = config_dir + "/themes/" + effective_theme + ".css";
        if (stat(user_theme_path.c_str(), &st) == 0) validate_css(read_text(user_theme_path), user_theme_path, doc);
    }
    std::string legacy_css = config_dir + "/theme.css";
    if (stat(legacy_css.c_str(), &st) == 0) {
        validate_css(read_text(legacy_css), legacy_css, doc);
        doc.hint("legacy theme.css overrides your theme name — it wins unless themes/<name>.css or a built-in matches",
                 "mv " + legacy_css + " " + config_dir + "/themes/" + effective_theme + ".css");
    }

    // 4. Prompt template
    std::string prompt_html = config_dir + "/prompt.html";
    if (stat(prompt_html.c_str(), &st) == 0) {
        std::string html = read_text(prompt_html);
        validate_prompt_html(html, prompt_html, doc);
        PromptEngine probe(env);
        probe.set_theme_css(theme.css_content);
        probe.set_template_html(html);
        PromptContext ctx = sample_context(probe, false);
        RenderResult result = probe.render(ctx, 0);
        if (result.ansi_output.empty()) {
            doc.problem(prompt_html + " renders to an empty prompt", "keep a <prompt> root element around your tags");
        } else {
            doc.good("prompt.html renders (" + std::to_string(result.total_lines) + " line(s))");
        }
    } else {
        doc.good("no prompt.html — using the generated prompt for your show_* settings");
    }

    // 5. Plugins & custom commands
    std::string plugin_dir = config_dir + "/plugins";
    size_t plugin_count = 0;
    for (const auto& entry : list_directory(plugin_dir, ".sh")) {
        plugin_count++;
        if (access(entry.c_str(), R_OK) != 0) doc.problem(entry + " is not readable", "chmod u+r " + entry);
    }
    if (plugin_count) doc.good(std::to_string(plugin_count) + " plugin script(s) found in plugins/");

    std::string cmd_dir = config_dir + "/commands";
    size_t cmd_count = 0;
    for (const auto& entry : list_directory(cmd_dir, "")) {
        cmd_count++;
        if (access(entry.c_str(), X_OK) != 0) {
            doc.hint(entry + " is not executable (Aswell still runs it through /bin/sh)",
                     "chmod +x " + entry);
        }
    }
    if (cmd_count) doc.good(std::to_string(cmd_count) + " custom command(s) in commands/");

    std::string path = env.get_var("PATH");
    if (path.find(cmd_dir) == std::string::npos) {
        doc.problem(cmd_dir + " is missing from $PATH — custom commands will not be found",
                     "export PATH=\"" + cmd_dir + ":$PATH\"");
    }

    // 6. Colours & terminal
    std::string colorterm = env.get_var("COLORTERM");
    std::string term = env.get_var("TERM");
    if (!cfg.enable_colored_output) {
        doc.hint("colored_output=false — TrueColor hints and ls/grep aliases are disabled",
                 "aswell config set colored_output true");
    } else if (colorterm != "truecolor" && colorterm != "24bit") {
        doc.hint("COLORTERM is '" + (colorterm.empty() ? "unset" : colorterm) +
                     "' (TERM=" + (term.empty() ? "unset" : term) + "); Aswell exports truecolor for its own session",
                 "set COLORTERM=truecolor in your terminal profile for other programs");
    }

    // 7. Login shell registration
    std::string self = current_executable_path();
    if (!self.empty()) {
        std::ifstream shells("/etc/shells");
        bool registered = false;
        std::string line;
        while (shells && std::getline(shells, line)) {
            if (str_util::trim(line) == self) {
                registered = true;
                break;
            }
        }
        if (!registered) {
            doc.hint(self + " is not listed in /etc/shells, so 'chsh' will refuse it",
                     "echo " + self + " | sudo tee -a /etc/shells && chsh -s " + self);
        } else {
            doc.good("registered in /etc/shells — settable as your login shell");
        }
    }

    // 8. Live reload
    if (cfg.auto_reload) {
        doc.good("auto_reload is on — theme.css, prompt.html and config.txt are picked up live");
    } else {
        doc.hint("auto_reload=false — prompt/theme edits need a restart or `aswell reload`",
                 "aswell config set auto_reload true");
    }

    // Report: problems are red flags, hints are optional polish.
    if (!quiet) {
        std::cout << kBold << "aswell doctor" << kReset << kDim << "  " << config_dir << kReset << "\n";
        for (const auto& line : doc.ok) std::cout << kGreen << "\xe2\x9c\x93" << kReset << " " << line << "\n";
        print_notes(doc.hints, "!", kYellow);
        print_notes(doc.problems, "\xe2\x9c\x97", kRed);
    } else {
        for (const auto& problem : doc.problems) {
            std::cout << "problem: " << problem.text;
            if (!problem.fix.empty()) std::cout << "\n  fix: " << problem.fix;
            std::cout << "\n";
        }
    }

    if (doc.problems.empty()) {
        if (!quiet) {
            std::cout << "\n" << kGreen << "\xe2\x9c\x93 Everything checks out \xe2\x80\x94 " << doc.ok.size()
                      << " check(s) passed";
            if (!doc.hints.empty()) {
                std::cout << kReset << kDim << ", " << doc.hints.size() << " optional hint(s)" << kReset;
            }
            std::cout << kReset << "\n";
        }
    } else {
        std::cout << "\n" << kRed << doc.problems.size() << " problem(s) found" << kReset << kDim
                  << " \xe2\x80\x94 " << doc.hints.size() << " hint(s), " << doc.ok.size()
                  << " check(s) passed" << kReset << "\n";
    }
    return doc.problems.empty() ? 0 : 1;
}

void SettingsCli::print_help() {
    std::cout << "Customization hub:\n"
              << "  aswell config [list|set|get|toggle|unset|show|export|import|help]   all shell settings\n"
              << "  aswell theme  [list|set|preview|show|new|reset]                      prompt themes\n"
              << "  aswell doctor [--quiet]                                              validate config, theme & prompt\n"
              << "  aswell reload                                                         re-read theme/prompt/settings\n";
}

std::string SettingsCli::settings_as_json(const ShellConfig& cfg, bool include_defaults) {
    std::vector<std::string> entries;
    for (const auto& def : SettingsRegistry::all()) {
        std::string value = def.get(cfg);
        bool changed = SettingsRegistry::is_modified(def, cfg);
        if (!include_defaults && !changed) continue;

        auto emit_scalar = [&](std::ostringstream& os, const std::string& raw) {
            if (def.type == SettingType::BOOL) os << (raw == "true" ? "true" : "false");
            else if (def.type == SettingType::INT) os << (raw.empty() ? "0" : raw);
            else os << "\"" << json_escape(raw) << "\"";
        };

        std::ostringstream item;
        item << "    \"" << json_escape(def.key) << "\": {\"value\": ";
        emit_scalar(item, value);
        item << ", \"default\": ";
        emit_scalar(item, SettingsRegistry::default_value(def));
        item << ", \"type\": \"" << type_name(def.type) << "\"";
        if (def.type == SettingType::ENUM) {
            item << ", \"choices\": [";
            std::vector<std::string> options = SettingsRegistry::choices(def);
            for (size_t i = 0; i < options.size(); ++i) {
                item << (i ? ", " : "") << "\"" << json_escape(options[i]) << "\"";
            }
            item << "]";
        }
        item << ", \"category\": \"" << json_escape(def.category) << "\"";
        item << ", \"modified\": " << (changed ? "true" : "false");
        item << ", \"description\": \"" << json_escape(def.description) << "\"}";
        entries.push_back(item.str());
    }

    std::ostringstream out;
    out << "{\n  \"config_dir\": \"" << json_escape(ConfigManager::get_config_dir())
        << "\",\n  \"settings\": {\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << entries[i] << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    out << "  }\n}";
    return out.str();
}

} // namespace aswell
