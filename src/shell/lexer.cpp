#include "aswell/shell/ast.hpp"
#include "aswell/shell/lexer.hpp"

namespace aswell {

std::string Token::type_name(TokenType type) {
    switch (type) {
        case TokenType::TOKEN_EOF: return "EOF";
        case TokenType::TOKEN_NEWLINE: return "NEWLINE";
        case TokenType::TOKEN_WORD: return "WORD";
        case TokenType::TOKEN_ASSIGNMENT: return "ASSIGNMENT";
        case TokenType::TOKEN_IO_NUMBER: return "IO_NUMBER";
        case TokenType::TOKEN_PIPE: return "|";
        case TokenType::TOKEN_AMP: return "&";
        case TokenType::TOKEN_SEMI: return ";";
        case TokenType::TOKEN_AND_IF: return "&&";
        case TokenType::TOKEN_OR_IF: return "||";
        case TokenType::TOKEN_DSEMI: return ";;";
        case TokenType::TOKEN_DLESS: return "<<";
        case TokenType::TOKEN_DGREAT: return ">>";
        case TokenType::TOKEN_LESSAND: return "<&";
        case TokenType::TOKEN_GREATAND: return ">&";
        case TokenType::TOKEN_LESSGREAT: return "<>";
        case TokenType::TOKEN_DLESSDASH: return "<<-";
        case TokenType::TOKEN_CLOBBER: return ">|";
        case TokenType::TOKEN_HERESTRING: return "<<<";
        case TokenType::TOKEN_LESS: return "<";
        case TokenType::TOKEN_GREAT: return ">";
        case TokenType::TOKEN_LPAREN: return "(";
        case TokenType::TOKEN_RPAREN: return ")";
        case TokenType::TOKEN_BANG: return "!";
        case TokenType::KEYWORD_IF: return "if";
        case TokenType::KEYWORD_THEN: return "then";
        case TokenType::KEYWORD_ELSE: return "else";
        case TokenType::KEYWORD_ELIF: return "elif";
        case TokenType::KEYWORD_FI: return "fi";
        case TokenType::KEYWORD_DO: return "do";
        case TokenType::KEYWORD_DONE: return "done";
        case TokenType::KEYWORD_CASE: return "case";
        case TokenType::KEYWORD_ESAC: return "esac";
        case TokenType::KEYWORD_WHILE: return "while";
        case TokenType::KEYWORD_UNTIL: return "until";
        case TokenType::KEYWORD_FOR: return "for";
        case TokenType::KEYWORD_IN: return "in";
        case TokenType::KEYWORD_LBRACE: return "{";
        case TokenType::KEYWORD_RBRACE: return "}";
        case TokenType::KEYWORD_FUNCTION: return "function";
    }
    return "UNKNOWN";
}

std::string Token::to_string() const {
    std::ostringstream oss;
    oss << "Token(" << type_name(type);
    if (!value.empty()) {
        oss << ", \"" << value << "\"";
    }
    oss << " at line " << line << ":" << column << ")";
    return oss.str();
}

std::optional<TokenType> Token::match_keyword(const std::string& word) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"if", TokenType::KEYWORD_IF},
        {"then", TokenType::KEYWORD_THEN},
        {"else", TokenType::KEYWORD_ELSE},
        {"elif", TokenType::KEYWORD_ELIF},
        {"fi", TokenType::KEYWORD_FI},
        {"do", TokenType::KEYWORD_DO},
        {"done", TokenType::KEYWORD_DONE},
        {"case", TokenType::KEYWORD_CASE},
        {"esac", TokenType::KEYWORD_ESAC},
        {"while", TokenType::KEYWORD_WHILE},
        {"until", TokenType::KEYWORD_UNTIL},
        {"for", TokenType::KEYWORD_FOR},
        {"in", TokenType::KEYWORD_IN},
        {"{", TokenType::KEYWORD_LBRACE},
        {"}", TokenType::KEYWORD_RBRACE},
        {"function", TokenType::KEYWORD_FUNCTION}
    };
    auto it = keywords.find(word);
    if (it != keywords.end()) {
        return it->second;
    }
    return std::nullopt;
}

Lexer::Lexer(std::string input) : input_(std::move(input)) {}

void Lexer::reset(std::string new_input) {
    input_ = std::move(new_input);
    pos_ = 0;
    line_ = 1;
    column_ = 1;
    peeked_token_.reset();
    at_command_start_ = true;
}

char Lexer::peek_char() const {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_];
}

char Lexer::next_char() {
    if (pos_ >= input_.size()) return '\0';
    char c = input_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match_char(char expected) {
    if (peek_char() == expected) {
        next_char();
        return true;
    }
    return false;
}

void Lexer::skip_whitespace(bool skip_newlines) {
    while (pos_ < input_.size()) {
        // Line continuation: backslash immediately followed by newline
        if (input_[pos_] == '\\' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '\n') {
            pos_ += 2;
            line_++;
            column_ = 1;
            continue;
        }

        char c = input_[pos_];
        if (c == ' ' || c == '\t' || c == '\r' || (skip_newlines && c == '\n')) {
            next_char();
        } else {
            break;
        }
    }
}

void Lexer::skip_comment() {
    while (pos_ < input_.size() && input_[pos_] != '\n') {
        next_char();
    }
}

bool Lexer::has_more() const {
    return pos_ < input_.size() || peeked_token_.has_value();
}

Token Lexer::peek_token() {
    if (!peeked_token_) {
        peeked_token_ = next_token();
    }
    return *peeked_token_;
}

Token Lexer::next_token() {
    if (peeked_token_) {
        Token tok = *peeked_token_;
        peeked_token_.reset();
        return tok;
    }

    while (pos_ < input_.size()) {
        skip_whitespace(false);
        if (pos_ >= input_.size()) break;

        char c = peek_char();

        if (c == '#') {
            skip_comment();
            continue;
        }

        if (c == '\n') {
            size_t l = line_;
            size_t col = column_;
            next_char();
            at_command_start_ = true;
            return Token{TokenType::TOKEN_NEWLINE, "\n", l, col, false};
        }

        // Check if this could be an IO number (digits immediately followed by < or >)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t save_pos = pos_;
            size_t save_line = line_;
            size_t save_col = column_;
            std::string digits;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(peek_char()))) {
                digits += next_char();
            }
            if (peek_char() == '<' || peek_char() == '>') {
                // It is indeed an IO number!
                return Token{TokenType::TOKEN_IO_NUMBER, digits, save_line, save_col, false};
            }
            // Rewind and treat as regular word
            pos_ = save_pos;
            line_ = save_line;
            column_ = save_col;
        }

        // Check for operators
        if (std::strchr("|&;()<>\n!", c) != nullptr) {
            return read_operator();
        }

        return read_word();
    }

    return Token{TokenType::TOKEN_EOF, "", line_, column_, false};
}

Token Lexer::read_operator() {
    size_t l = line_;
    size_t col = column_;
    char c = next_char();

    switch (c) {
        case '|':
            if (match_char('|')) return Token{TokenType::TOKEN_OR_IF, "||", l, col, false};
            return Token{TokenType::TOKEN_PIPE, "|", l, col, false};
        case '&':
            if (match_char('&')) return Token{TokenType::TOKEN_AND_IF, "&&", l, col, false};
            return Token{TokenType::TOKEN_AMP, "&", l, col, false};
        case ';':
            if (match_char(';')) return Token{TokenType::TOKEN_DSEMI, ";;", l, col, false};
            return Token{TokenType::TOKEN_SEMI, ";", l, col, false};
        case '(':
            return Token{TokenType::TOKEN_LPAREN, "(", l, col, false};
        case ')':
            return Token{TokenType::TOKEN_RPAREN, ")", l, col, false};
        case '!':
            return Token{TokenType::TOKEN_BANG, "!", l, col, false};
        case '<':
            if (match_char('<')) {
                if (match_char('-')) return Token{TokenType::TOKEN_DLESSDASH, "<<-", l, col, false};
                if (match_char('<')) return Token{TokenType::TOKEN_HERESTRING, "<<<", l, col, false};
                return Token{TokenType::TOKEN_DLESS, "<<", l, col, false};
            }
            if (match_char('&')) return Token{TokenType::TOKEN_LESSAND, "<&", l, col, false};
            if (match_char('>')) return Token{TokenType::TOKEN_LESSGREAT, "<>", l, col, false};
            return Token{TokenType::TOKEN_LESS, "<", l, col, false};
        case '>':
            if (match_char('>')) return Token{TokenType::TOKEN_DGREAT, ">>", l, col, false};
            if (match_char('&')) return Token{TokenType::TOKEN_GREATAND, ">&", l, col, false};
            if (match_char('|')) return Token{TokenType::TOKEN_CLOBBER, ">|", l, col, false};
            return Token{TokenType::TOKEN_GREAT, ">", l, col, false};
    }

    return Token{TokenType::TOKEN_WORD, std::string(1, c), l, col, false};
}

Token Lexer::read_word() {
    size_t l = line_;
    size_t col = column_;
    std::string val;
    bool has_quotes = false;

    while (pos_ < input_.size()) {
        char c = peek_char();

        if (std::isspace(static_cast<unsigned char>(c)) || c == '\n') {
            break;
        }

        // If unquoted operator char, stop word
        if (std::strchr("|&;()<>\n", c) != nullptr) {
            break;
        }

        // Single quotes
        if (c == '\'') {
            has_quotes = true;
            val += next_char(); // retain quote for expansion stage
            while (pos_ < input_.size()) {
                char qc = next_char();
                val += qc;
                if (qc == '\'') break;
            }
            continue;
        }

        // Double quotes
        if (c == '\"') {
            has_quotes = true;
            val += next_char(); // retain quote for expansion stage
            while (pos_ < input_.size()) {
                char qc = next_char();
                val += qc;
                if (qc == '\\' && pos_ < input_.size()) {
                    val += next_char();
                } else if (qc == '\"') {
                    break;
                }
            }
            continue;
        }

        // Backslash outside quotes
        if (c == '\\') {
            has_quotes = true;
            val += next_char();
            if (pos_ < input_.size()) {
                val += next_char();
            }
            continue;
        }

        // Command substitution $(...) or arithmetic $((...))
        if (c == '$' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '(') {
            val += next_char(); // '$'
            val += next_char(); // '('
            int paren_depth = 1;
            while (pos_ < input_.size() && paren_depth > 0) {
                char sc = next_char();
                val += sc;
                if (sc == '(') paren_depth++;
                else if (sc == ')') paren_depth--;
                else if (sc == '\'') {
                    while (pos_ < input_.size()) {
                        char sqc = next_char();
                        val += sqc;
                        if (sqc == '\'') break;
                    }
                } else if (sc == '\"') {
                    while (pos_ < input_.size()) {
                        char dqc = next_char();
                        val += dqc;
                        if (dqc == '\\' && pos_ < input_.size()) {
                            val += next_char();
                        } else if (dqc == '\"') break;
                    }
                }
            }
            continue;
        }

        // Parameter expansion ${...}
        if (c == '$' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '{') {
            val += next_char(); // '$'
            val += next_char(); // '{'
            int brace_depth = 1;
            while (pos_ < input_.size() && brace_depth > 0) {
                char sc = next_char();
                val += sc;
                if (sc == '{') brace_depth++;
                else if (sc == '}') brace_depth--;
            }
            continue;
        }

        // Backticks `...`
        if (c == '`') {
            val += next_char();
            while (pos_ < input_.size()) {
                char bc = next_char();
                val += bc;
                if (bc == '\\' && pos_ < input_.size()) {
                    val += next_char();
                } else if (bc == '`') {
                    break;
                }
            }
            continue;
        }

        val += next_char();
    }

    // Check if it's a keyword
    if (!has_quotes) {
        auto kw = Token::match_keyword(val);
        if (kw.has_value()) {
            return Token{*kw, val, l, col, false};
        }
    }

    return Token{TokenType::TOKEN_WORD, val, l, col, has_quotes};
}

void Lexer::process_heredocs(std::vector<Redirection>& redirections) {
    for (auto& redir : redirections) {
        if (redir.type != RedirType::DLESS && redir.type != RedirType::DLESSDASH) {
            continue;
        }

        std::string delim = redir.target;
        bool strip_tabs = (redir.type == RedirType::DLESSDASH);
        bool expand = true;

        // If delimiter has quotes, strip them and mark heredoc unexpanded
        if (!delim.empty() && (delim.front() == '\'' || delim.front() == '\"')) {
            expand = false;
            if (delim.size() >= 2 && delim.front() == delim.back()) {
                delim = delim.substr(1, delim.size() - 2);
            }
        }
        redir.heredoc_expand = expand;

        // Read lines until delim line
        std::string content;
        while (pos_ < input_.size()) {
            std::string line;
            while (pos_ < input_.size()) {
                char c = next_char();
                if (c == '\n') break;
                line += c;
            }

            std::string check_line = line;
            if (strip_tabs) {
                size_t tab_idx = 0;
                while (tab_idx < check_line.size() && check_line[tab_idx] == '\t') {
                    tab_idx++;
                }
                check_line = check_line.substr(tab_idx);
                line = check_line;
            }

            if (check_line == delim) {
                break;
            }
            content += line;
            content += '\n';
        }
        redir.heredoc_content = content;
    }
}

} // namespace aswell
