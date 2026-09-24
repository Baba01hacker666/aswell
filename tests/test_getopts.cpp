#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "aswell/shell/builtins.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/jobs.hpp"

using namespace aswell;

struct Harness {
    Environment env;
    JobManager jobs;
    Executor exec;
    ControlFlow flow;
    Harness() : exec(env, jobs) {}

    int getopts_call(const std::vector<std::string>& args) {
        return Builtins::execute("getopts", args, env, jobs, exec, flow);
    }

    // Run a full `while getopts` loop over positionals; returns seen options.
    std::string parse_all(const std::string& optstring, const std::vector<std::string>& positionals) {
        env.set_positional_params(positionals);
        std::string got;
        std::vector<std::string> args = {"getopts", optstring, "opt"};
        while (getopts_call(args) == 0) {
            got += env.get_var("opt");
            got += '|';
            std::string o = env.get_var("opt");
            if ((o == "b" || o == "o") && env.has_var("OPTARG")) {
                got += env.get_var("OPTARG");
                got += '|';
            }
        }
        return got;
    }
};

static void test_basic_flags() {
    Harness h;
    assert(h.parse_all("ab", {"-a", "-b"}) == "a|b|");
    assert(h.env.get_var("OPTIND") == "3");
    std::cout << "[PASS] test_basic_flags\n";
}

static void test_attached_and_separate_args() {
    Harness h;
    assert(h.parse_all("o:", {"-ofoo", "-o", "bar"}) == "o|foo|o|bar|");
    assert(h.env.get_var("OPTIND") == "4");
    std::cout << "[PASS] test_attached_and_separate_args\n";
}

static void test_cluster() {
    Harness h;
    assert(h.parse_all("abc", {"-abc", "-c"}) == "a|b|c|c|");
    assert(h.env.get_var("OPTIND") == "3");
    std::cout << "[PASS] test_cluster\n";
}

static void test_missing_arg() {
    // Verbose: '?' + diagnostic, OPTARG unset.
    Harness h;
    h.env.set_positional_params({"-o"});
    assert(h.getopts_call({"getopts", "o:", "opt"}) == 0);
    assert(h.env.get_var("opt") == "?");
    assert(!h.env.has_var("OPTARG"));
    assert(h.env.get_var("OPTIND") == "2");
    assert(h.getopts_call({"getopts", "o:", "opt"}) == 1);

    // Silent (leading ':'): ':' + OPTARG=optchar.
    Harness h2;
    h2.env.set_positional_params({"-o"});
    assert(h2.getopts_call({"getopts", ":o:", "opt"}) == 0);
    assert(h2.env.get_var("opt") == ":");
    assert(h2.env.get_var("OPTARG") == "o");
    std::cout << "[PASS] test_missing_arg\n";
}

static void test_invalid_option() {
    Harness h;
    h.env.set_positional_params({"-z"});
    assert(h.getopts_call({"getopts", "ab", "opt"}) == 0);
    assert(h.env.get_var("opt") == "?");
    assert(h.getopts_call({"getopts", "ab", "opt"}) == 1);

    Harness h2;
    h2.env.set_positional_params({"-z"});
    assert(h2.getopts_call({"getopts", ":ab", "opt"}) == 0);
    assert(h2.env.get_var("opt") == "?");
    assert(h2.env.get_var("OPTARG") == "z");
    std::cout << "[PASS] test_invalid_option\n";
}

static void test_terminator_and_nonoption() {
    // "--" ends parsing; the next call resumes after it.
    Harness h;
    h.env.set_positional_params({"--", "-a"});
    assert(h.getopts_call({"getopts", "a", "opt"}) == 1);
    assert(h.env.get_var("OPTIND") == "2");
    assert(h.getopts_call({"getopts", "a", "opt"}) == 0);
    assert(h.env.get_var("opt") == "a");

    // A bare "-" or non-option word stops parsing at that word.
    Harness h2;
    h2.env.set_positional_params({"-", "-a"});
    assert(h2.getopts_call({"getopts", "a", "opt"}) == 1);
    assert(h2.env.get_var("OPTIND") == "1");

    Harness h3;
    h3.env.set_positional_params({"file", "-a"});
    assert(h3.getopts_call({"getopts", "a", "opt"}) == 1);
    assert(h3.env.get_var("OPTIND") == "1");
    std::cout << "[PASS] test_terminator_and_nonoption\n";
}

static void test_explicit_args() {
    // Explicit operands replace the positional parameters.
    Harness h;
    h.env.set_positional_params({"-z"});
    std::vector<std::string> args = {"getopts", "ab", "opt", "-a", "-b"};
    std::string got;
    while (h.getopts_call(args) == 0) got += h.env.get_var("opt");
    assert(got == "ab");
    // Untouched positionals still hold the original words.
    assert(h.env.get_positional_params().size() == 1);
    std::cout << "[PASS] test_explicit_args\n";
}

static void test_optind_reset_and_errors() {
    Harness h;
    h.env.set_positional_params({"-a"});
    assert(h.getopts_call({"getopts", "a", "opt"}) == 0);
    assert(h.getopts_call({"getopts", "a", "opt"}) == 1);
    // User resets OPTIND=1: a fresh pass works (cluster state reset too).
    h.env.set_var("OPTIND", "1");
    assert(h.getopts_call({"getopts", "a", "opt"}) == 0);
    assert(h.env.get_var("opt") == "a");

    // Usage errors.
    assert(h.getopts_call({"getopts"}) == 1);
    assert(h.getopts_call({"getopts", "ab"}) == 1);
    assert(h.getopts_call({"getopts", "ab", "1bad"}) == 1);

    // OPTERR=0 suppresses diagnostics but parsing is identical.
    Harness h2;
    h2.env.set_var("OPTERR", "0");
    h2.env.set_positional_params({"-z"});
    assert(h2.getopts_call({"getopts", "ab", "opt"}) == 0);
    assert(h2.env.get_var("opt") == "?");
    std::cout << "[PASS] test_optind_reset_and_errors\n";
}

int main() {
    test_basic_flags();
    test_attached_and_separate_args();
    test_cluster();
    test_missing_arg();
    test_invalid_option();
    test_terminator_and_nonoption();
    test_explicit_args();
    test_optind_reset_and_errors();
    std::cout << "[PASS] getopts suite\n";
    return 0;
}
