#pragma once

#include "aswell/common.hpp"

namespace aswell {

enum class TokenType {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_WORD,
    TOKEN_ASSIGNMENT,
    TOKEN_IO_NUMBER,
    TOKEN_PIPE,         // |
    TOKEN_AMP,          // &
    TOKEN_SEMI,         // ;
    TOKEN_AND_IF,       // &&
    TOKEN_OR_IF,        // ||
    TOKEN_DSEMI,        // ;;
    TOKEN_DLESS,        // <<
    TOKEN_DGREAT,       // >>
    TOKEN_LESSAND,      // <&
    TOKEN_GREATAND,     // >&
    TOKEN_LESSGREAT,    // <>
    TOKEN_DLESSDASH,    // <<-
    TOKEN_CLOBBER,      // >|
    TOKEN_HERESTRING,   // <<<
    TOKEN_LESS,         // <
    TOKEN_GREAT,        // >
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_BANG,         // !

    // Reserved words
    KEYWORD_IF,
    KEYWORD_THEN,
    KEYWORD_ELSE,
    KEYWORD_ELIF,
    KEYWORD_FI,
    KEYWORD_DO,
    KEYWORD_DONE,
    KEYWORD_CASE,
    KEYWORD_ESAC,
    KEYWORD_WHILE,
    KEYWORD_UNTIL,
    KEYWORD_FOR,
    KEYWORD_IN,
    KEYWORD_LBRACE,     // {
    KEYWORD_RBRACE,     // }
    KEYWORD_FUNCTION    // function
};

struct Token {
    TokenType type;
    std::string value;
    size_t line = 1;
    size_t column = 1;
    bool has_quotes = false; // Whether the word contained single or double quotes or backslashes

    std::string to_string() const;
    static std::string type_name(TokenType type);
    static std::optional<TokenType> match_keyword(const std::string& word);
};

} // namespace aswell
