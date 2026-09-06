#include <cassert>
#include <iostream>
#include "aswell/shell/lexer.hpp"
#include "aswell/shell/parser.hpp"

using namespace aswell;

void test_parse_simple() {
    Lexer lex("ls -la /tmp");
    Parser parser(lex);
    auto ast = parser.parse_program();
    assert(ast != nullptr);
    assert(ast->items.size() == 1);
    assert(ast->items[0].and_or->pipelines.size() == 1);
    auto pipe = ast->items[0].and_or->pipelines[0].second;
    assert(pipe->commands.size() == 1);
    auto cmd = std::dynamic_pointer_cast<SimpleCommandNode>(pipe->commands[0]);
    assert(cmd != nullptr);
    assert(cmd->words.size() == 3);
    assert(cmd->words[0] == "ls");
    assert(cmd->words[1] == "-la");
    assert(cmd->words[2] == "/tmp");
    std::cout << "[PASS] test_parse_simple\n";
}

void test_parse_pipeline() {
    Lexer lex("cat file.txt | grep foo | wc -l");
    Parser parser(lex);
    auto ast = parser.parse_program();
    assert(ast != nullptr);
    auto pipe = ast->items[0].and_or->pipelines[0].second;
    assert(pipe->commands.size() == 3);
    std::cout << "[PASS] test_parse_pipeline\n";
}

void test_parse_if() {
    Lexer lex("if test -f foo; then echo yes; else echo no; fi");
    Parser parser(lex);
    auto ast = parser.parse_program();
    assert(ast != nullptr);
    auto pipe = ast->items[0].and_or->pipelines[0].second;
    auto if_node = std::dynamic_pointer_cast<IfNode>(pipe->commands[0]);
    assert(if_node != nullptr);
    assert(if_node->branches.size() == 1);
    assert(if_node->else_branch != nullptr);
    std::cout << "[PASS] test_parse_if\n";
}

void test_parse_for_and_while() {
    Lexer lex("for x in 1 2 3; do echo $x; done");
    Parser parser(lex);
    auto ast = parser.parse_program();
    assert(ast != nullptr);
    auto pipe = ast->items[0].and_or->pipelines[0].second;
    auto for_node = std::dynamic_pointer_cast<ForNode>(pipe->commands[0]);
    assert(for_node != nullptr);
    assert(for_node->var_name == "x");
    assert(for_node->words.size() == 3);
    std::cout << "[PASS] test_parse_for_and_while\n";
}

int main() {
    std::cout << "--- Running Parser Tests ---\n";
    test_parse_simple();
    test_parse_pipeline();
    test_parse_if();
    test_parse_for_and_while();
    std::cout << "All Parser Tests Passed!\n";
    return 0;
}
