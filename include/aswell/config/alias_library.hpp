#pragma once

#include "aswell/common.hpp"

namespace aswell {

class Environment;
class Executor;

// A curated, documented set of aliases and shell functions that ship with
// Aswell. Like SettingsRegistry for knobs, this table is the single source of
// truth: `aswell aliases list|show|install|remove|search`, the startup
// `curated_aliases` setting and `~/.config/aswell/aliases` (which `install`
// writes and which is sourced on every start) all read it.
//
// Everything here is deliberately additive: names are chosen so they do not
// shadow existing commands, except in the `safe` category where overriding a
// command *is* the point — those entries set `shadows = true` so the installer
// can warn about it.
enum class AliasKind {
    ALIAS,
    FUNCTION,
};

struct AliasDef {
    const char* name;
    AliasKind kind = AliasKind::ALIAS;
    const char* category = "files";
    const char* definition = "";     // alias value, or the full function source
    const char* description = "";
    const char* needs = "";           // command that must exist, or "" for none
    bool shadows = false;            // replaces an existing command name
};

class AliasLibrary {
public:
    static const std::vector<AliasDef>& all();
    static std::vector<std::string> categories();
    static std::vector<const AliasDef*> for_category(const std::string& category);

    // Looks up by alias/function name (exact, then case-insensitive).
    static const AliasDef* find(const std::string& name);

    // Accepts `all`, a category (`git`), a list (`git,files`) or single names
    // (`ll gs mkcd`), returning the matching definitions in table order.
    // Unknown words are collected in `unknown` so callers can suggest fixes.
    static std::vector<const AliasDef*> expand(const std::vector<std::string>& selectors,
                                                std::vector<std::string>& unknown);

    // Installs definitions into a running shell. Existing user definitions win
    // unless `force` is set. Returns the names actually installed.
    // `executor` is required to install functions; pass nullptr to skip them.
    static std::vector<std::string> install(Environment& env, Executor* executor,
                                             const std::vector<const AliasDef*>& defs, bool force,
                                             std::vector<std::string>& skipped);

    static void uninstall(Environment& env, const std::vector<const AliasDef*>& defs);

    // Shell source: `alias x='y'` / `name() { … }` lines with a comment each.
    static std::string render(const std::vector<const AliasDef*>& defs);

    // ~/.config/aswell/aliases — appended to by `install`, sourced at startup,
    // and plain shell source so the user can edit it by hand.
    static std::string file_path();

    // Read ~/.config/aswell/aliases and define everything in it. Used at shell
    // start (before and after the bashrc import, so the user's own aliases still
    // win) and by `aswell -c`, where the library must be as available as it is
    // in an interactive shell. Missing file = no-op.
    static size_t source_into(Executor& executor);
    static bool append_to_file(const std::vector<const AliasDef*>& defs, std::string& err);
    static bool remove_from_file(const std::vector<const AliasDef*>& defs, std::string& err);
    // Removes the named entries from shell source text (aliases by line,
    // functions by their whole `name() { … }` block).
    static std::string remove_from_text(const std::string& text,
                                        const std::vector<const AliasDef*>& defs);
    static std::string remove_names_from_text(const std::string& text,
                                               const std::vector<std::string>& names);
    static bool remove_names_from_file(const std::vector<std::string>& names, std::string& err);
    static std::vector<std::string> names_in_file(std::string* content_out = nullptr);
    static std::vector<const AliasDef*> for_names(const std::vector<std::string>& names);
};

} // namespace aswell
