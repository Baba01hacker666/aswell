#include <cassert>
#include <iostream>
#include "aswell/shell/environment.hpp"
#include "aswell/shell/expansion.hpp"

using namespace aswell;

void test_parameter_expansions() {
    Environment env;
    Expansion exp(env);

    env.set_var("FOO", "hello_world");
    assert(exp.expand_word_single("$FOO") == "hello_world");
    assert(exp.expand_word_single("${FOO}") == "hello_world");
    assert(exp.expand_word_single("${#FOO}") == "11");

    // Default values
    assert(exp.expand_word_single("${UNSET:-default}") == "default");
    assert(exp.expand_word_single("${FOO:-default}") == "hello_world");

    // Substring & Pattern strip
    assert(exp.expand_word_single("${FOO#hello_}") == "world");
    assert(exp.expand_word_single("${FOO%_world}") == "hello");

    env.set_var("PATH_VAR", "/home/user/code/file.txt");
    assert(exp.expand_word_single("${PATH_VAR##*/}") == "file.txt");
    assert(exp.expand_word_single("${PATH_VAR%/*}") == "/home/user/code");

    // Pattern replacement
    assert(exp.expand_word_single("${FOO/world/universe}") == "hello_universe");

    std::cout << "[PASS] test_parameter_expansions\n";
}

void test_arithmetic_expansion() {
    Environment env;
    Expansion exp(env);

    assert(exp.evaluate_arithmetic("1 + 2 * 3") == 7);
    assert(exp.evaluate_arithmetic("(1 + 2) * 3") == 9);
    assert(exp.evaluate_arithmetic("2 ** 4") == 16);
    assert(exp.evaluate_arithmetic("10 / 2 + 5 % 3") == 7);
    assert(exp.evaluate_arithmetic("10 > 5 ? 42 : 99") == 42);
    assert(exp.evaluate_arithmetic("10 < 5 ? 42 : 99") == 99);
    assert(exp.evaluate_arithmetic("1 << 4") == 16);

    env.set_var("X", "50");
    assert(exp.evaluate_arithmetic("X * 2") == 100);

    std::cout << "[PASS] test_arithmetic_expansion\n";
}

void test_quote_removal() {
    Environment env;
    Expansion exp(env);

    assert(exp.expand_word_single("\"hello world\"") == "hello world");
    assert(exp.expand_word_single("'literal $dollar'") == "literal $dollar");
    assert(exp.expand_word_single("escaped\\ space") == "escaped space");

    std::cout << "[PASS] test_quote_removal\n";
}

void test_brace_expansion() {
    Environment env;
    Expansion exp(env);

    // Simple comma expansion
    auto res1 = exp.expand_braces("{a,b,c}");
    assert((res1 == std::vector<std::string>{"a", "b", "c"}));

    // Prefix and suffix
    auto res2 = exp.expand_braces("pre_{a,b}_post");
    assert((res2 == std::vector<std::string>{"pre_a_post", "pre_b_post"}));

    // Numeric range
    auto res3 = exp.expand_braces("{1..5}");
    assert((res3 == std::vector<std::string>{"1", "2", "3", "4", "5"}));

    // Reverse numeric range
    auto res4 = exp.expand_braces("{3..1}");
    assert((res4 == std::vector<std::string>{"3", "2", "1"}));

    // Zero-padded range
    auto res5 = exp.expand_braces("{01..05}");
    assert((res5 == std::vector<std::string>{"01", "02", "03", "04", "05"}));

    // Step range
    auto res6 = exp.expand_braces("{1..9..2}");
    assert((res6 == std::vector<std::string>{"1", "3", "5", "7", "9"}));

    // Char range
    auto res7 = exp.expand_braces("{a..e}");
    assert((res7 == std::vector<std::string>{"a", "b", "c", "d", "e"}));

    // Cartesian product
    auto res8 = exp.expand_braces("{1,2}_{3,4}");
    assert((res8 == std::vector<std::string>{"1_3", "1_4", "2_3", "2_4"}));

    // Nested braces
    auto res9 = exp.expand_braces("a{b,c{1,2}}d");
    assert((res9 == std::vector<std::string>{"abd", "ac1d", "ac2d"}));

    // Non-expandable braces
    auto res10 = exp.expand_braces("{foo}");
    assert((res10 == std::vector<std::string>{"{foo}"}));

    // Quoted braces preserved as-is
    auto res11 = exp.expand_braces("\"{a,b}\"");
    assert((res11 == std::vector<std::string>{"\"{a,b}\""}));

    // Full expand_words integration
    auto full = exp.expand_words({"echo", "file_{1..3}.txt"});
    assert((full == std::vector<std::string>{"echo", "file_1.txt", "file_2.txt", "file_3.txt"}));

    std::cout << "[PASS] test_brace_expansion\n";
}

int main() {
    std::cout << "--- Running Expansion Tests ---\n";
    test_parameter_expansions();
    test_arithmetic_expansion();
    test_quote_removal();
    test_brace_expansion();
    std::cout << "All Expansion Tests Passed!\n";
    return 0;
}
