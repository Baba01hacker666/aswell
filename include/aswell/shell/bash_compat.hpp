#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

class Executor; // forward declaration (defined in executor.hpp)

struct BashImportStats {
    size_t aliases = 0;
    size_t variables = 0;
    size_t functions = 0;
    bool used_bash_dump = false;
    bool skipped = false;
};

// Imports aliases, exported variables and user shell functions from the
// user's bash startup files (~/.bashrc, ~/.bash_aliases) into Aswell.
//
// Strategy:
//  1. Fast path: run `bash -i` (interactive, so bashrc's "not interactive,
//     return" guard is bypassed) and dump `alias -p`, `export -p` and
//     user function bodies. This interprets conditionals, $HOME expansion,
//     PATH manipulation, etc. exactly like bash would.
//  2. Fallback: lightweight direct line parser for alias/export assignments
//     when bash is unavailable or the dump fails.
//
// Safe by design: only data (aliases, VAR=values, function definitions) is
// imported — PROMPT_COMMAND, PS1, completion machinery (_* functions,
// COMPREPLY/compgen bodies) and other bash-internals are skipped. Existing
// Aswell aliases/variables are never overwritten by the import so
// ~/.aswellrc always wins.
class BashCompat {
public:
    static BashImportStats import_bashrc(Environment& env, Executor* exec = nullptr,
                                         bool verbose = false);

    // Direct-file fallback (also used for ~/.bash_aliases).
    static BashImportStats import_file(const std::string& path, Environment& env,
                                       Executor* exec = nullptr);

    static std::string default_bashrc_path();
    static bool bash_available();

    // Decodes a bash-quoted value: 'single', "double" (with \ escapes),
    // $'ansi-c' and bare words. Used for alias/export parsing.
    static std::string bash_unquote(const std::string& raw);

private:
    static bool should_skip_var(const std::string& name);
    static bool should_skip_function(const std::string& name, const std::string& body);
    static void merge_path(Environment& env, const std::string& imported_path);
    static std::string run_bash_dump();
    static BashImportStats import_dump(const std::string& dump, Environment& env,
                                       Executor* exec, bool verbose);
    static std::string expand_simple(const std::string& val, Environment& env);
};

} // namespace aswell
