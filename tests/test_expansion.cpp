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

int main() {
    std::cout << "--- Running Expansion Tests ---\n";
    test_parameter_expansions();
    test_arithmetic_expansion();
    test_quote_removal();
    std::cout << "All Expansion Tests Passed!\n";
    return 0;
}
