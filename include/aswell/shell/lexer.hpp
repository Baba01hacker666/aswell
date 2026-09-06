#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/token.hpp"
#include "aswell/shell/ast.hpp"

namespace aswell {

class Lexer {
public:
    explicit Lexer(std::string input);

    Token next_token();
    Token peek_token();
    bool has_more() const;
    void reset(std::string new_input);

    // Collect heredoc contents for queued heredocs
    void process_heredocs(std::vector<Redirection>& redirections);

    size_t get_line() const { return line_; }
    size_t get_column() const { return column_; }

private:
    char peek_char() const;
    char next_char();
    bool match_char(char expected);
    void skip_whitespace(bool skip_newlines = false);
    void skip_comment();

    Token read_operator();
    Token read_word();

    std::string input_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;

    std::optional<Token> peeked_token_;
    bool at_command_start_ = true;
};

} // namespace aswell
