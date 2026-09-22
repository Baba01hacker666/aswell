#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include "aswell/shell/bash_compat.hpp"

using namespace aswell;

static void test_bash_unquote() {
    assert(BashCompat::bash_unquote("'hello world'") == "hello world");
    assert(BashCompat::bash_unquote("'it'\\''s'") == "it's");
    assert(BashCompat::bash_unquote("\"a\\\"b\"") == "a\"b");
    assert(BashCompat::bash_unquote("\"a\\n\"") == "a\n");
    assert(BashCompat::bash_unquote("$'a\\nb'") == "a\nb");
    assert(BashCompat::bash_unquote("$'\\e[31m'") == "\033[31m");
    assert(BashCompat::bash_unquote("plain") == "plain");
    assert(BashCompat::bash_unquote("  'spaced'  ") == "spaced");
    std::cout << "[PASS] test_bash_unquote\n";
}

static void test_import_file() {
    const std::string rc = "/tmp/aswell_test_bashrc_import";
    {
        std::ofstream f(rc);
        f << "# comment line\n";
        f << "alias ll='ls -alF'\n";
        f << "alias gs=\"git status\"\n";
        f << "export TEST_EDITOR_TEST=nano_test\n";
        f << "TEST_BARE_VAR=bare_value\n";
        f << "export PATH=\"$HOME/.local/bin:/usr/bin:/bin\"\n";
        f << "PS1='should-be-skipped'\n";
        f << "PROMPT_COMMAND='also-skipped'\n";
    }

    Environment env;
    BashImportStats stats = BashCompat::import_file(rc, env, nullptr);

    std::string val;
    assert(env.get_alias("ll", val) && val == "ls -alF");
    assert(env.get_alias("gs", val) && val == "git status");
    assert(env.get_var("TEST_EDITOR_TEST") == "nano_test");
    assert(env.get_var("TEST_BARE_VAR") == "bare_value");
    // Bash internals are never imported.
    assert(!env.has_var("PS1"));
    assert(!env.has_var("PROMPT_COMMAND"));
    // PATH merge keeps aswell custom dirs first.
    std::string path = env.get_var("PATH");
    assert(path.find(".config/aswell/commands") != std::string::npos);
    assert(path.find(".local/bin") != std::string::npos);
    assert(stats.aliases >= 2);
    assert(stats.variables >= 2);
    std::remove(rc.c_str());
    std::cout << "[PASS] test_import_file\n";
}

static void test_import_opt_out() {
    const char* prev = std::getenv("ASWELL_NO_BASHRC");
    std::string saved = prev ? prev : "";
    setenv("ASWELL_NO_BASHRC", "1", 1);

    Environment env;
    BashImportStats stats = BashCompat::import_bashrc(env, nullptr, false);
    assert(stats.skipped);

    if (!saved.empty()) {
        setenv("ASWELL_NO_BASHRC", saved.c_str(), 1);
    } else {
        unsetenv("ASWELL_NO_BASHRC");
    }
    std::cout << "[PASS] test_import_opt_out\n";
}

static void test_import_missing_file_skips() {
    // Point HOME at an empty dir: no .bashrc, and bash dump (if bash exists)
    // sources the real user file — either way this must not crash.
    const char* prev_home = std::getenv("HOME");
    std::string saved_home = prev_home ? prev_home : "";
    (void)mkdir("/tmp/aswell_test_emptyhome", 0755); // may exist; harmless
    setenv("HOME", "/tmp/aswell_test_emptyhome", 1);

    Environment env;
    BashImportStats stats = BashCompat::import_bashrc(env, nullptr, false);
    (void)stats; // skipped or fallback-parsed: must simply not crash

    if (!saved_home.empty()) setenv("HOME", saved_home.c_str(), 1);
    std::cout << "[PASS] test_import_missing_file_skips\n";
}

int main() {
    test_bash_unquote();
    test_import_file();
    test_import_opt_out();
    test_import_missing_file_skips();
    std::cout << "[PASS] bashcompat suite\n";
    return 0;
}
