#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/ast.hpp"
#include "aswell/shell/lexer.hpp"

namespace aswell {

class Parser {
public:
    explicit Parser(Lexer& lexer);

    std::shared_ptr<CommandListNode> parse_program();
    std::shared_ptr<CommandListNode> parse_command_list();

    bool has_error() const { return !error_message_.empty(); }
    const std::string& error_message() const { return error_message_; }

private:
    Token peek();
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type);
    void consume_newlines();
    void set_error(const std::string& msg);

    std::shared_ptr<AndOrNode> parse_and_or();
    std::shared_ptr<PipelineNode> parse_pipeline();
    std::shared_ptr<CommandNode> parse_command();
    std::shared_ptr<SimpleCommandNode> parse_simple_command();
    std::shared_ptr<SubshellNode> parse_subshell();
    std::shared_ptr<GroupingNode> parse_grouping();
    std::shared_ptr<IfNode> parse_if();
    std::shared_ptr<ForNode> parse_for();
    std::shared_ptr<WhileNode> parse_while(bool is_until);
    std::shared_ptr<CaseNode> parse_case();
    std::shared_ptr<FunctionDefNode> parse_function();

    bool parse_redirection(std::vector<Redirection>& redirs);
    bool is_redirection_token(TokenType type);

    Lexer& lexer_;
    std::string error_message_;
};

} // namespace aswell
