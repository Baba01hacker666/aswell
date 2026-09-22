#include <cassert>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "aswell/editor/completion.hpp"

using namespace aswell;

static bool contains_text(const std::vector<CompletionCandidate>& v, const std::string& text) {
    for (const auto& c : v) {
        if (c.text == text) return true;
    }
    return false;
}

static void test_command_completion() {
    Environment env;
    CompletionEngine ce(env);

    auto r = ce.complete("ech", 3);
    assert(contains_text(r, "echo"));

    // Exact match ranks first.
    auto r2 = ce.complete("ls", 2);
    assert(!r2.empty() && r2[0].text == "ls");

    // Aliases and functions participate.
    env.set_alias("zzmyalias", "echo hi");
    auto r3 = ce.complete("zzmya", 5);
    assert(contains_text(r3, "zzmyalias"));

    // Results are capped.
    auto r4 = ce.complete("", 0);
    assert(!r4.empty() && r4.size() <= 50);
    std::cout << "[PASS] test_command_completion\n";
}

static void test_flag_and_subcommand_completion() {
    Environment env;
    CompletionEngine ce(env);

    auto flags = ce.complete("ls --col", 8);
    assert(contains_text(flags, "--color=auto"));

    auto git = ce.complete("git ch", 6);
    assert(contains_text(git, "checkout"));

    auto aswell = ce.complete("aswell ba", 9);
    assert(contains_text(aswell, "bash"));
    std::cout << "[PASS] test_flag_and_subcommand_completion\n";
}

static void test_variable_completion() {
    Environment env;
    env.set_var("ASWELL_TEST_COMP_VAR", "hello", false);
    CompletionEngine ce(env);

    auto r = ce.complete("echo $ASWELL_TEST_COMP", 22);
    assert(contains_text(r, "$ASWELL_TEST_COMP_VAR"));
    std::cout << "[PASS] test_variable_completion\n";
}

static void test_path_completion() {
    char cwd_buf[4096];
    assert(getcwd(cwd_buf, sizeof(cwd_buf)) != nullptr);
    std::string saved_cwd = cwd_buf;

    (void)mkdir("/tmp/aswell_test_compdir", 0755);
    (void)mkdir("/tmp/aswell_test_compdir/subalpha", 0755);
    FILE* f = fopen("/tmp/aswell_test_compdir/filebeta.txt", "w");
    if (f) fclose(f);

    assert(chdir("/tmp/aswell_test_compdir") == 0);
    Environment env;
    CompletionEngine ce(env);

    auto files = ce.complete("cat fileb", 9);
    assert(contains_text(files, "filebeta.txt"));

    // cd completes directories only.
    auto dirs = ce.complete("cd sub", 6);
    assert(contains_text(dirs, "subalpha/"));

    assert(chdir(saved_cwd.c_str()) == 0);
    std::cout << "[PASS] test_path_completion\n";
}

static void test_cache() {
    Environment env;
    CompletionEngine ce(env);
    ce.invalidate_cache();
    assert(ce.cache_size() == 0);
    auto r = ce.complete("l", 1);
    (void)r;
    assert(ce.cache_size() > 0);
    std::cout << "[PASS] test_cache\n";
}

int main() {
    test_command_completion();
    test_flag_and_subcommand_completion();
    test_variable_completion();
    test_path_completion();
    test_cache();
    std::cout << "[PASS] completion suite\n";
    return 0;
}
