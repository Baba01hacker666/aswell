// The curated alias/function library that ships with Aswell.
//
// Same philosophy as SettingsRegistry: one table, many consumers. `aswell
// aliases list|show|install|remove|search`, the `curated_aliases` setting
// (applied by apply_config on startup and every reload) and the
// ~/.config/aswell/aliases file (written by `install`, sourced at startup, and
// free for the user to edit by hand) are all generated from `entries()` below.
//
// Rules for adding an entry:
//   * never shadow an existing command outside the `safe` category;
//   * keep alias bodies to a command plus flags, or use the re-parse-friendly
//     characters we support (`|`, `&&`, quotes, $(...) now work in aliases);
//   * set `needs` when the entry is useless without a binary, so installing
//     it on a machine without that tool is a no-op instead of a broken alias;
//   * functions must not assume GNU-only flags when a fallback is easy.
#include "aswell/config/alias_library.hpp"
#include "aswell/config/config.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/executor.hpp"
#include <fstream>
#include <cctype>
#include <cstdio>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

namespace aswell {
namespace {

const std::vector<AliasDef>& table() {
    static const std::vector<AliasDef> entries = {
        // --- files -------------------------------------------------------
        {"ll", AliasKind::ALIAS, "files", "ls -lh", "long listing, human readable sizes", "ls", false},
        {"la", AliasKind::ALIAS, "files", "ls -lAh", "long listing including dotfiles", "ls", false},
        {"lt", AliasKind::ALIAS, "files", "ls -lht", "long listing, newest first", "ls", false},
        {"l", AliasKind::ALIAS, "files", "ls -F", "compact listing with type markers", "ls", false},
        {"sz", AliasKind::ALIAS, "files", "du -hs", "size of each argument on disk", "du", false},
        {"nl", AliasKind::ALIAS, "files", "cat -n", "print a file numbered", "cat", false},

        // --- nav ---------------------------------------------------------
        {"..", AliasKind::ALIAS, "nav", "cd ..", "one directory up", "", false},
        {"...", AliasKind::ALIAS, "nav", "cd ../..", "two directories up", "", false},
        {"....", AliasKind::ALIAS, "nav", "cd ../../..", "three directories up", "", false},
        {"home", AliasKind::ALIAS, "nav", "cd", "go home", "", false},
        {"back", AliasKind::ALIAS, "nav", "cd -", "previous directory", "", false},
        {"up", AliasKind::FUNCTION, "nav",
         "up() {\n  local n=\"${1:-1}\"\n  while [ \"$n\" -gt 0 ]; do cd .. || return 1; n=$((n - 1)); done\n}",
         "go up N directories (default 1)", "", false},
        {"mkcd", AliasKind::FUNCTION, "nav",
         "mkcd() {\n  if [ -z \"$1\" ]; then echo \"usage: mkcd DIR\" >&2; return 2; fi\n"
         "  mkdir -p \"$1\" && cd \"$1\"\n}",
         "create DIR and enter it", "mkdir", false},
        {"cdto", AliasKind::FUNCTION, "nav",
         "cdto() {\n  if [ -z \"$1\" ]; then echo \"usage: cdto FILE\" >&2; return 2; fi\n"
         "  cd \"$(dirname \"$1\")\" || return 1\n}",
         "cd into the directory holding FILE", "dirname", false},

        // --- git ---------------------------------------------------------
        {"gs", AliasKind::ALIAS, "git", "git status -sb", "short status with branch", "git", false},
        {"ga", AliasKind::ALIAS, "git", "git add", "stage files", "git", false},
        {"gcm", AliasKind::ALIAS, "git", "git commit -m", "commit with a message", "git", false},
        {"gd", AliasKind::ALIAS, "git", "git diff", "unstaged changes", "git", false},
        {"gds", AliasKind::ALIAS, "git", "git diff --staged", "staged changes", "git", false},
        {"gl", AliasKind::ALIAS, "git", "git log --oneline --graph --decorate -20", "last 20 commits as a graph", "git", false},
        {"gco", AliasKind::ALIAS, "git", "git checkout", "switch branch or restore file", "git", false},
        {"gcb", AliasKind::ALIAS, "git", "git checkout -b", "create and switch to a branch", "git", false},
        {"gp", AliasKind::ALIAS, "git", "git pull --rebase", "pull with rebase", "git", false},
        {"gf", AliasKind::ALIAS, "git", "git fetch --all --prune", "fetch every remote", "git", false},
        {"gignored", AliasKind::FUNCTION, "git",
         "gignored() {\n  git ls-files --others --ignored --exclude-standard\n}",
         "list files ignored by .gitignore", "git", false},

        // --- sys ---------------------------------------------------------
        {"dfh", AliasKind::ALIAS, "sys", "df -h", "disk usage, human readable", "df", false},
        {"duh", AliasKind::ALIAS, "sys", "du -sh *", "size of everything here", "du", false},
        {"psu", AliasKind::ALIAS, "sys", "ps aux", "all processes", "ps", false},
        {"mem", AliasKind::ALIAS, "sys", "free -h", "memory usage", "free", false},
        {"disks", AliasKind::ALIAS, "sys", "lsblk", "block devices", "lsblk", false},
        {"ports", AliasKind::FUNCTION, "sys",
         "ports() {\n  if command -v ss >/dev/null 2>&1; then ss -tulnp \"$@\"\n"
         "  elif command -v netstat >/dev/null 2>&1; then netstat -tulnp \"$@\"\n"
         "  else echo \"ports: need ss or netstat\" >&2; return 1; fi\n}",
         "listening sockets (ss, else netstat)", "", false},
        {"path", AliasKind::FUNCTION, "sys",
         "path() {\n  local IFS=:\n  for p in $PATH; do echo \"$p\"; done\n}",
         "print $PATH one entry per line", "", false},
        {"procs", AliasKind::FUNCTION, "sys",
         "procs() {\n  ps aux | awk -v pat=\"$1\" 'NR==1 || $0 ~ pat' | sort -k3 -rn | head -15\n}",
         "top processes matching a pattern (by CPU)", "ps", false},

        // --- net ---------------------------------------------------------
        {"ifc", AliasKind::ALIAS, "net", "ip addr show", "interfaces and addresses", "ip", false},
        {"routes", AliasKind::ALIAS, "net", "ip route show", "routing table", "ip", false},
        {"pingf", AliasKind::ALIAS, "net", "ping -c 4", "four pings then stop", "ping", false},
        {"dnstest", AliasKind::FUNCTION, "net",
         "dnstest() {\n  if [ -z \"$1\" ]; then echo \"usage: dnstest HOST\" >&2; return 2; fi\n"
         "  if command -v dig >/dev/null 2>&1; then dig +short \"$1\"; else getent hosts \"$1\"; fi\n}",
         "resolve a hostname (dig, else getent)", "", false},

        // --- dev ---------------------------------------------------------
        {"serve", AliasKind::FUNCTION, "dev",
         "serve() {\n  local port=\"${1:-8000}\"\n"
         "  if command -v python3 >/dev/null 2>&1; then python3 -m http.server \"$port\"\n"
         "  elif command -v python >/dev/null 2>&1; then python -m SimpleHTTPServer \"$port\"\n"
         "  else echo \"serve: need python3\" >&2; return 1; fi\n}",
         "serve the current directory over HTTP", "", false},
        {"json", AliasKind::ALIAS, "dev", "python3 -m json.tool", "pretty-print JSON", "python3", false},
        {"findpy", AliasKind::ALIAS, "dev", "python3 -c", "run inline python", "python3", false},
        {"gitstat", AliasKind::FUNCTION, "dev",
         "gitstat() {\n  git diff --stat \"$@\"\n  echo\n  git diff --cached --stat\n}",
         "diffstat for both the worktree and the index", "git", false},

        // --- aswell (hub shortcuts) --------------------------------------
        {"acfg", AliasKind::ALIAS, "aswell", "aswell config", "settings hub", "", false},
        {"atheme", AliasKind::ALIAS, "aswell", "aswell theme", "theme hub", "", false},
        {"apreview", AliasKind::ALIAS, "aswell", "aswell theme preview", "render a theme preview", "", false},
        {"adoctor", AliasKind::ALIAS, "aswell", "aswell doctor", "validate my configuration", "", false},
        {"areload", AliasKind::ALIAS, "aswell", "aswell reload", "re-read config, theme and prompt", "", false},
        {"colors", AliasKind::FUNCTION, "aswell",
         "colors() {\n  aswell theme preview --all\n}",
         "flip through every theme with one command", "", false},

        // --- safe (intentionally shadows commands; opt-in per category) --
        {"rm", AliasKind::ALIAS, "safe", "rm -i", "ask before removing", "rm", true},
        {"mv", AliasKind::ALIAS, "safe", "mv -i", "ask before overwriting", "mv", true},
        {"cp", AliasKind::ALIAS, "safe", "cp -i", "ask before overwriting", "cp", true},
        {"ln", AliasKind::ALIAS, "safe", "ln -si", "ask before relinking", "ln", true},
        {"truncate", AliasKind::ALIAS, "safe", "truncate --size 0", "empty a file", "truncate", true},

        // --- docker ------------------------------------------------------
        {"dps", AliasKind::ALIAS, "docker", "docker ps", "running containers", "docker", false},
        {"dpsa", AliasKind::ALIAS, "docker", "docker ps -a", "all containers", "docker", false},
        {"dim", AliasKind::ALIAS, "docker", "docker images", "local images", "docker", false},
        {"dex", AliasKind::ALIAS, "docker", "docker exec -it", "shell into a container", "docker", false},
        {"dlogs", AliasKind::ALIAS, "docker", "docker logs -f --tail 100", "follow recent logs", "docker", false},
        {"dprune", AliasKind::FUNCTION, "docker",
         "dprune() {\n  echo \"removing stopped containers and dangling images\"\n"
         "  docker container prune -f\n  docker image prune -f\n}",
         "prune stopped containers and dangling images", "docker", false},
    };
    return entries;
}

bool ci_equal(const std::string& a, const std::string& b) {
    return str_util::to_lower(a) == str_util::to_lower(b);
}

} // namespace

const std::vector<AliasDef>& AliasLibrary::all() {
    return table();
}

std::vector<std::string> AliasLibrary::categories() {
    std::vector<std::string> out;
    for (const auto& def : table()) {
        if (std::find(out.begin(), out.end(), def.category) == out.end()) {
            out.push_back(def.category);
        }
    }
    return out;
}

std::vector<const AliasDef*> AliasLibrary::for_category(const std::string& category) {
    std::vector<const AliasDef*> out;
    for (const auto& def : table()) {
        if (ci_equal(def.category, category)) out.push_back(&def);
    }
    return out;
}

const AliasDef* AliasLibrary::find(const std::string& name) {
    for (const auto& def : table()) {
        if (name == def.name) return &def;
    }
    for (const auto& def : table()) {
        if (ci_equal(name, def.name)) return &def;
    }
    return nullptr;
}

std::vector<const AliasDef*> AliasLibrary::expand(const std::vector<std::string>& selectors,
                                                 std::vector<std::string>& unknown) {
    std::vector<const AliasDef*> picked;
    auto pick = [&](const std::string& word) {
        if (word.empty()) return;
        if (word == "all" || word == "*") {
            for (const auto& def : table()) {
                if (std::find(picked.begin(), picked.end(), &def) == picked.end()) {
                    picked.push_back(&def);
                }
            }
            return;
        }
        if (const AliasDef* def = find(word)) {
            if (std::find(picked.begin(), picked.end(), def) == picked.end()) picked.push_back(def);
            return;
        }
        auto by_cat = for_category(word);
        if (!by_cat.empty()) {
            for (const AliasDef* def : by_cat) {
                if (std::find(picked.begin(), picked.end(), def) == picked.end()) picked.push_back(def);
            }
            return;
        }
        unknown.push_back(word);
    };

    for (const auto& sel : selectors) {
        for (const auto& word : str_util::split(sel, ',')) {
            pick(str_util::trim(word));
        }
    }
    return picked;
}

std::vector<std::string> AliasLibrary::install(Environment& env, Executor* executor,
                                               const std::vector<const AliasDef*>& defs, bool force,
                                               std::vector<std::string>& skipped) {
    std::vector<std::string> installed;
    for (const AliasDef* def : defs) {
        const std::string name = def->name;
        if (def->needs && *def->needs) {
            if (env.find_in_path(def->needs).empty()) {
                skipped.push_back(name + " (needs " + def->needs + ")");
                continue;
            }
        }
        std::string existing;
        const bool defined = env.get_alias(name, existing) || env.has_function(name);
        if (defined && !force) {
            skipped.push_back(name + " (already defined)");
            continue;
        }
        if (def->kind == AliasKind::ALIAS) {
            env.set_alias(name, def->definition);
            installed.push_back(name);
            continue;
        }
        if (!executor) {
            // Functions need parsing; the caller only has a live environment.
            skipped.push_back(name + " (function: run it from ~/.config/aswell/aliases)");
            continue;
        }
        if (executor->execute_string(def->definition) != 0) {
            skipped.push_back(name + " (definition failed to parse)");
            continue;
        }
        installed.push_back(name);
    }
    return installed;
}

void AliasLibrary::uninstall(Environment& env, const std::vector<const AliasDef*>& defs) {
    for (const AliasDef* def : defs) {
        if (def->kind == AliasKind::ALIAS) {
            env.remove_alias(def->name);
        } else {
            env.remove_function(def->name);
        }
    }
}

std::string AliasLibrary::render(const std::vector<const AliasDef*>& defs) {
    std::string out;
    std::string category;
    for (const AliasDef* def : defs) {
        if (!category.empty() && category != def->category) out += "\n";
        if (category != def->category) {
            category = def->category;
            out += "# " + category + "\n";
        }
        if (def->kind == AliasKind::ALIAS) {
            out += std::string("alias ") + def->name + "=" + str_util::escape_shell(def->definition);
            out += "   # " + std::string(def->description) + "\n";
        } else {
            // The description goes on its own line: a trailing "# ..." after the
            // closing brace would stop remove_names_from_text() from ever seeing a
            // bare "}" and would make uninstalling one function eat the rest of the
            // file.
            out += def->definition;
            if (!out.empty() && out.back() != '\n') out += "\n";
            out += "# " + std::string(def->description) + "\n";
        }
    }
    return out;
}

size_t AliasLibrary::source_into(Executor& executor) {
    std::ifstream in(file_path());
    if (!in) return 0;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    if (text.empty()) return 0;
    executor.execute_script(text);
    size_t lines = 0;
    for (char c : text)
        if (c == '\n') lines++;
    return lines;
}

std::string AliasLibrary::file_path() {
    return ConfigManager::get_config_dir() + "/aliases";
}

namespace {
// mkdir -p for the directory holding the alias file, so `aswell aliases install`
// works even before anything else has created ~/.config/aswell.
bool ensure_parent_dir(const std::string& path, std::string& err) {
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return true;
    std::string dir = path.substr(0, slash);
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    if (dir.empty() || dir[0] != '/') {
        if (!dir.empty() && mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            err = "cannot create " + dir + ": " + std::strerror(errno);
            return false;
        }
        return true;
    }
    std::string built;
    for (const auto& part : str_util::split(dir, '/')) {
        if (part.empty()) continue;
        built += "/" + part;
        if (struct stat st; stat(built.c_str(), &st) == 0) continue;   // exists (or is a file)
        if (mkdir(built.c_str(), 0755) != 0 && errno != EEXIST) {
            err = "cannot create " + built + ": " + std::strerror(errno);
            return false;
        }
    }
    return true;
}
} // namespace

namespace {
// The alias file is sourced on every shell start, so it must never be left
// half-written. Write beside it and rename.
bool write_atomically(const std::string& path, const std::string& body) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << body;
        out.flush();
        if (!out.good()) return false;
    }
    if (::rename(tmp.c_str(), path.c_str()) != 0) {
        ::unlink(tmp.c_str());
        return false;
    }
    return true;
}
} // namespace

bool AliasLibrary::append_to_file(const std::vector<const AliasDef*>& defs, std::string& err) {
    if (defs.empty()) {
        err = "nothing selected";
        return false;
    }
    std::string path = file_path();
    if (!ensure_parent_dir(path, err)) return false;
    std::string existing;
    if (struct stat st; stat(path.c_str(), &st) == 0) {
        std::ifstream in(path);
        existing.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    // Drop any previous definition of the same names, then append the new ones:
    // `install` is idempotent, so re-running it after a library update is safe.
    std::string cleaned = remove_from_text(existing, defs);

    std::string body = render(defs);
    std::string header;
    if (cleaned.empty()) {
        header = "# Aswell aliases and functions.\n"
                 "# This file is sourced on every start; edit it freely.\n"
                 "# `aswell aliases install <category|name>` appends here.\n\n";
    }
    if (!cleaned.empty() && cleaned.back() != '\n') cleaned += "\n";

    if (!write_atomically(path, cleaned + header + body)) {
        err = "cannot write " + path + ": " + std::strerror(errno);
        return false;
    }
    return true;
}

namespace {
// A function definition line looks like `name() {` or `name()`.
bool is_definition_header(const std::string& t) {
    size_t paren = t.find("()");
    if (paren == std::string::npos || paren == 0) return false;
    for (size_t i = 0; i < paren; ++i) {
        const char c = t[i];
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}
} // namespace

std::vector<std::string> AliasLibrary::names_in_file(std::string* content_out) {
    std::vector<std::string> names;
    std::ifstream in(file_path());
    if (!in) return names;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (content_out) *content_out = text;

    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::string t = str_util::trim(line);
        if (str_util::starts_with(t, "alias ")) {
            std::string rest = t.substr(6);
            size_t eq = rest.find('=');
            if (eq != std::string::npos) names.push_back(rest.substr(0, eq));
        } else if (!t.empty() && t[0] != '#' && is_definition_header(t)) {
            // `name() {` opens a function; `local files=()` inside a body does not,
            // because what precedes the "()" is not a bare name.
            size_t paren = t.find("()");
            names.push_back(str_util::trim(t.substr(0, paren)));
        }
    }
    return names;
}

std::string AliasLibrary::remove_from_text(const std::string& text, const std::vector<const AliasDef*>& defs) {
    std::vector<std::string> targets;
    for (const AliasDef* def : defs) targets.push_back(def->name);
    return remove_names_from_text(text, targets);
}

std::string AliasLibrary::remove_names_from_text(const std::string& text,
                                                 const std::vector<std::string>& names) {
    std::istringstream lines(text);
    std::string out;
    std::string line;
    while (std::getline(lines, line)) {
        std::string t = str_util::trim(line);
        bool drop = false;
        for (const auto& name : names) {
            const std::string alias_prefix = "alias " + name + "=";
            const std::string fn_prefix = name + "()";
            if (t == alias_prefix || str_util::starts_with(t, alias_prefix)) { drop = true; break; }
            if (str_util::starts_with(t, fn_prefix)) { drop = true; break; }
        }
        if (drop) {
            // A function body spans until its closing brace, which may carry a
            // trailing comment when the file was written by an older release.
            if (t.find("()") != std::string::npos && line.find('}') == std::string::npos) {
                while (std::getline(lines, line)) {
                    const std::string body = str_util::trim(line);
                    if (!body.empty() && body[0] == '}') break;
                }
            }
            continue;
        }
        out += line;
        out += "\n";
    }
    while (str_util::ends_with(out, "\n\n")) out.erase(out.size() - 1);
    return out;
}

bool AliasLibrary::remove_from_file(const std::vector<const AliasDef*>& defs, std::string& err) {
    std::vector<std::string> names;
    for (const AliasDef* def : defs) names.push_back(def->name);
    return remove_names_from_file(names, err);
}

bool AliasLibrary::remove_names_from_file(const std::vector<std::string>& names, std::string& err) {
    std::string path = file_path();
    std::ifstream in(path);
    if (!in) {
        err = "no alias file at " + path;
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string cleaned = remove_names_from_text(text, names);
    if (!write_atomically(path, cleaned)) {
        err = "cannot write " + path;
        return false;
    }
    return true;
}

std::vector<const AliasDef*> AliasLibrary::for_names(const std::vector<std::string>& names) {
    std::vector<const AliasDef*> out;
    for (const auto& name : names) {
        if (const AliasDef* def = find(name)) out.push_back(def);
    }
    return out;
}

} // namespace aswell
