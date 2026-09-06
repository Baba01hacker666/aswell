#include <cassert>
#include <iostream>
#include "aswell/shell/lexer.hpp"

using namespace aswell;

void test_basic_tokens() {
    Lexer lexer("echo hello world");
    Token t1 = lexer.next_token();
    assert(t1.type == TokenType::TOKEN_WORD && t1.value == "echo");
    Token t2 = lexer.next_token();
    assert(t2.type == TokenType::TOKEN_WORD && t2.value == "hello");
    Token t3 = lexer.next_token();
    assert(t3.type == TokenType::TOKEN_WORD && t3.value == "world");
    Token t4 = lexer.next_token();
    assert(t4.type == TokenType::TOKEN_EOF);
    std::cout << "[PASS] test_basic_tokens\n";
}

void test_operators() {
    Lexer lexer("cmd1 | cmd2 && cmd3 || cmd4 ; cmd5 &");
    assert(lexer.next_token().value == "cmd1");
    assert(lexer.next_token().type == TokenType::TOKEN_PIPE);
    assert(lexer.next_token().value == "cmd2");
    assert(lexer.next_token().type == TokenType::TOKEN_AND_IF);
    assert(lexer.next_token().value == "cmd3");
    assert(lexer.next_token().type == TokenType::TOKEN_OR_IF);
    assert(lexer.next_token().value == "cmd4");
    assert(lexer.next_token().type == TokenType::TOKEN_SEMI);
    assert(lexer.next_token().value == "cmd5");
    assert(lexer.next_token().type == TokenType::TOKEN_AMP);
    std::cout << "[PASS] test_operators\n";
}

void test_quotes_and_escapes() {
    Lexer lexer("echo 'single quoted' \"double $var\"");
    assert(lexer.next_token().value == "echo");
    Token t2 = lexer.next_token();
    assert(t2.value == "'single quoted'");
    assert(t2.has_quotes);
    Token t3 = lexer.next_token();
    assert(t3.value == "\"double $var\"");
    assert(t3.has_quotes);
    std::cout << "[PASS] test_quotes_and_escapes\n";
}

void test_keywords() {
    Lexer lexer("if true; then echo yes; elif false; then echo no; else echo maybe; fi");
    assert(lexer.next_token().type == TokenType::KEYWORD_IF);
    assert(lexer.next_token().value == "true");
    assert(lexer.next_token().type == TokenType::TOKEN_SEMI);
    assert(lexer.next_token().type == TokenType::KEYWORD_THEN);
    std::cout << "[PASS] test_keywords\n";
}

int main() {
    std::cout << "--- Running Lexer Tests ---\n";
    test_basic_tokens();
    test_operators();
    test_quotes_and_escapes();
    test_keywords();
    std::cout << "All Lexer Tests Passed!\n";
    return 0;
}
