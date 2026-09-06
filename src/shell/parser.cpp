#include "aswell/shell/parser.hpp"

namespace aswell {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {}

Token Parser::peek() {
    return lexer_.peek_token();
}

Token Parser::advance() {
    return lexer_.next_token();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) {
    return peek().type == type;
}

void Parser::consume_newlines() {
    while (check(TokenType::TOKEN_NEWLINE)) {
        advance();
    }
}

void Parser::set_error(const std::string& msg) {
    if (error_message_.empty()) {
        std::ostringstream oss;
        oss << "syntax error near line " << peek().line << ": " << msg;
        error_message_ = oss.str();
    }
}

bool Parser::is_redirection_token(TokenType type) {
    switch (type) {
        case TokenType::TOKEN_LESS:
        case TokenType::TOKEN_GREAT:
        case TokenType::TOKEN_DGREAT:
        case TokenType::TOKEN_LESSAND:
        case TokenType::TOKEN_GREATAND:
        case TokenType::TOKEN_LESSGREAT:
        case TokenType::TOKEN_CLOBBER:
        case TokenType::TOKEN_DLESS:
        case TokenType::TOKEN_DLESSDASH:
        case TokenType::TOKEN_HERESTRING:
        case TokenType::TOKEN_IO_NUMBER:
            return true;
        default:
            return false;
    }
}

bool Parser::parse_redirection(std::vector<Redirection>& redirs) {
    int io_num = -1;
    if (check(TokenType::TOKEN_IO_NUMBER)) {
        Token io_tok = advance();
        try {
            io_num = std::stoi(io_tok.value);
        } catch (...) {
            io_num = -1;
        }
    }

    TokenType op_type = peek().type;
    RedirType rtype;

    switch (op_type) {
        case TokenType::TOKEN_LESS: rtype = RedirType::LESS; break;
        case TokenType::TOKEN_GREAT: rtype = RedirType::GREAT; break;
        case TokenType::TOKEN_DGREAT: rtype = RedirType::DGREAT; break;
        case TokenType::TOKEN_LESSAND: rtype = RedirType::LESSAND; break;
        case TokenType::TOKEN_GREATAND: rtype = RedirType::GREATAND; break;
        case TokenType::TOKEN_LESSGREAT: rtype = RedirType::LESSGREAT; break;
        case TokenType::TOKEN_CLOBBER: rtype = RedirType::CLOBBER; break;
        case TokenType::TOKEN_DLESS: rtype = RedirType::DLESS; break;
        case TokenType::TOKEN_DLESSDASH: rtype = RedirType::DLESSDASH; break;
        case TokenType::TOKEN_HERESTRING: rtype = RedirType::HERESTRING; break;
        default:
            return false;
    }
    advance();

    if (!check(TokenType::TOKEN_WORD)) {
        set_error("expected target after redirection operator");
        return false;
    }
    Token target_tok = advance();

    Redirection redir;
    redir.type = rtype;
    redir.io_number = io_num;
    redir.target = target_tok.value;

    redirs.push_back(redir);
    return true;
}

static bool is_valid_name(const std::string& s) {
    if (s.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(s[i])) && s[i] != '_') return false;
    }
    return true;
}

std::shared_ptr<CommandListNode> Parser::parse_program() {
    consume_newlines();
    if (check(TokenType::TOKEN_EOF)) {
        return std::make_shared<CommandListNode>();
    }
    auto list = parse_command_list();
    consume_newlines();
    return list;
}

std::shared_ptr<CommandListNode> Parser::parse_command_list() {
    auto list = std::make_shared<CommandListNode>();

    while (!check(TokenType::TOKEN_EOF) &&
           !check(TokenType::KEYWORD_THEN) &&
           !check(TokenType::KEYWORD_ELSE) &&
           !check(TokenType::KEYWORD_ELIF) &&
           !check(TokenType::KEYWORD_FI) &&
           !check(TokenType::KEYWORD_DO) &&
           !check(TokenType::KEYWORD_DONE) &&
           !check(TokenType::KEYWORD_ESAC) &&
           !check(TokenType::KEYWORD_RBRACE) &&
           !check(TokenType::TOKEN_RPAREN) &&
           !check(TokenType::TOKEN_DSEMI)) {

        consume_newlines();
        if (check(TokenType::TOKEN_EOF) ||
            check(TokenType::KEYWORD_THEN) ||
            check(TokenType::KEYWORD_ELSE) ||
            check(TokenType::KEYWORD_ELIF) ||
            check(TokenType::KEYWORD_FI) ||
            check(TokenType::KEYWORD_DO) ||
            check(TokenType::KEYWORD_DONE) ||
            check(TokenType::KEYWORD_ESAC) ||
            check(TokenType::KEYWORD_RBRACE) ||
            check(TokenType::TOKEN_RPAREN) ||
            check(TokenType::TOKEN_DSEMI)) {
            break;
        }

        auto and_or = parse_and_or();
        if (!and_or) {
            break;
        }

        bool async = false;
        if (match(TokenType::TOKEN_AMP)) {
            async = true;
        } else if (match(TokenType::TOKEN_SEMI)) {
            async = false;
        } else if (match(TokenType::TOKEN_NEWLINE)) {
            async = false;
        }

        list->items.push_back({and_or, async});
    }

    return list;
}

std::shared_ptr<AndOrNode> Parser::parse_and_or() {
    auto first_pipe = parse_pipeline();
    if (!first_pipe) return nullptr;

    auto and_or = std::make_shared<AndOrNode>();
    and_or->pipelines.push_back({AndOrNode::Op::NONE, first_pipe});

    while (check(TokenType::TOKEN_AND_IF) || check(TokenType::TOKEN_OR_IF)) {
        TokenType op_type = advance().type;
        consume_newlines();
        auto next_pipe = parse_pipeline();
        if (!next_pipe) {
            set_error("expected pipeline after && or ||");
            return nullptr;
        }
        and_or->pipelines.push_back({
            op_type == TokenType::TOKEN_AND_IF ? AndOrNode::Op::AND : AndOrNode::Op::OR,
            next_pipe
        });
    }

    return and_or;
}

std::shared_ptr<PipelineNode> Parser::parse_pipeline() {
    bool negated = false;
    if (match(TokenType::TOKEN_BANG)) {
        negated = true;
    }

    auto first_cmd = parse_command();
    if (!first_cmd) {
        return nullptr;
    }

    auto pipeline = std::make_shared<PipelineNode>();
    pipeline->negated = negated;
    pipeline->commands.push_back(first_cmd);

    while (match(TokenType::TOKEN_PIPE)) {
        consume_newlines();
        auto next_cmd = parse_command();
        if (!next_cmd) {
            set_error("expected command after pipe |");
            return nullptr;
        }
        pipeline->commands.push_back(next_cmd);
    }

    return pipeline;
}

std::shared_ptr<CommandNode> Parser::parse_command() {
    if (check(TokenType::KEYWORD_IF)) {
        return parse_if();
    }
    if (check(TokenType::KEYWORD_FOR)) {
        return parse_for();
    }
    if (check(TokenType::KEYWORD_WHILE)) {
        return parse_while(false);
    }
    if (check(TokenType::KEYWORD_UNTIL)) {
        return parse_while(true);
    }
    if (check(TokenType::KEYWORD_CASE)) {
        return parse_case();
    }
    if (check(TokenType::TOKEN_LPAREN)) {
        return parse_subshell();
    }
    if (check(TokenType::KEYWORD_LBRACE)) {
        return parse_grouping();
    }
    if (check(TokenType::KEYWORD_FUNCTION)) {
        return parse_function();
    }

    // Check for function name() { ... }
    if (check(TokenType::TOKEN_WORD)) {
        Token w = peek();
        if (is_valid_name(w.value)) {
            // Tentatively inspect if next token after w is '('
            // We can advance and check
            Token saved_w = advance();
            if (check(TokenType::TOKEN_LPAREN)) {
                advance(); // consume '('
                if (match(TokenType::TOKEN_RPAREN)) {
                    consume_newlines();
                    auto body = parse_command();
                    if (!body) {
                        set_error("expected function body");
                        return nullptr;
                    }
                    auto fdef = std::make_shared<FunctionDefNode>();
                    fdef->name = saved_w.value;
                    fdef->body = body;
                    while (is_redirection_token(peek().type)) {
                        parse_redirection(fdef->redirections);
                    }
                    lexer_.process_heredocs(fdef->redirections);
                    return fdef;
                } else {
                    set_error("expected ')' after '(' in function definition");
                    return nullptr;
                }
            } else {
                // Not a function, put saved_w into a simple command
                auto cmd = std::make_shared<SimpleCommandNode>();
                size_t eq_pos = saved_w.value.find('=');
                if (eq_pos != std::string::npos && eq_pos > 0 && is_valid_name(saved_w.value.substr(0, eq_pos))) {
                    cmd->assignments.emplace_back(saved_w.value.substr(0, eq_pos), saved_w.value.substr(eq_pos + 1));
                } else {
                    cmd->words.push_back(saved_w.value);
                }

                // Continue reading simple command
                bool parsing_prefix_assignments = cmd->words.empty();
                while (!check(TokenType::TOKEN_EOF)) {
                    if (is_redirection_token(peek().type)) {
                        if (!parse_redirection(cmd->redirections)) return nullptr;
                        continue;
                    }
                    if (check(TokenType::TOKEN_WORD)) {
                        Token tok = advance();
                        if (parsing_prefix_assignments) {
                            size_t eq = tok.value.find('=');
                            if (eq != std::string::npos && eq > 0 && is_valid_name(tok.value.substr(0, eq))) {
                                cmd->assignments.emplace_back(tok.value.substr(0, eq), tok.value.substr(eq + 1));
                                continue;
                            }
                        }
                        parsing_prefix_assignments = false;
                        cmd->words.push_back(tok.value);
                        continue;
                    }
                    break;
                }
                lexer_.process_heredocs(cmd->redirections);
                return cmd;
            }
        }
    }

    return parse_simple_command();
}

std::shared_ptr<SimpleCommandNode> Parser::parse_simple_command() {
    auto cmd = std::make_shared<SimpleCommandNode>();
    bool parsing_prefix_assignments = true;

    while (!check(TokenType::TOKEN_EOF)) {
        if (is_redirection_token(peek().type)) {
            if (!parse_redirection(cmd->redirections)) {
                return nullptr;
            }
            continue;
        }

        if (check(TokenType::TOKEN_WORD)) {
            Token tok = advance();

            if (parsing_prefix_assignments) {
                size_t eq_pos = tok.value.find('=');
                if (eq_pos != std::string::npos && eq_pos > 0) {
                    std::string var_name = tok.value.substr(0, eq_pos);
                    if (is_valid_name(var_name)) {
                        std::string var_val = tok.value.substr(eq_pos + 1);
                        cmd->assignments.emplace_back(var_name, var_val);
                        continue;
                    }
                }
            }

            parsing_prefix_assignments = false;
            cmd->words.push_back(tok.value);
            continue;
        }

        break;
    }

    if (cmd->words.empty() && cmd->assignments.empty() && cmd->redirections.empty()) {
        return nullptr;
    }

    lexer_.process_heredocs(cmd->redirections);
    return cmd;
}

std::shared_ptr<SubshellNode> Parser::parse_subshell() {
    advance(); // consume '('
    auto body = parse_command_list();
    if (!match(TokenType::TOKEN_RPAREN)) {
        set_error("expected ')' to close subshell");
        return nullptr;
    }
    auto node = std::make_shared<SubshellNode>();
    node->body = body;
    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<GroupingNode> Parser::parse_grouping() {
    advance(); // consume '{'
    auto body = parse_command_list();
    if (!match(TokenType::KEYWORD_RBRACE)) {
        set_error("expected '}' to close grouping");
        return nullptr;
    }
    auto node = std::make_shared<GroupingNode>();
    node->body = body;
    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<IfNode> Parser::parse_if() {
    advance(); // consume 'if'
    auto node = std::make_shared<IfNode>();

    auto cond = parse_command_list();
    if (!match(TokenType::KEYWORD_THEN)) {
        set_error("expected 'then' after if condition");
        return nullptr;
    }
    auto body = parse_command_list();
    node->branches.push_back({cond, body});

    while (match(TokenType::KEYWORD_ELIF)) {
        auto elif_cond = parse_command_list();
        if (!match(TokenType::KEYWORD_THEN)) {
            set_error("expected 'then' after elif condition");
            return nullptr;
        }
        auto elif_body = parse_command_list();
        node->branches.push_back({elif_cond, elif_body});
    }

    if (match(TokenType::KEYWORD_ELSE)) {
        node->else_branch = parse_command_list();
    }

    if (!match(TokenType::KEYWORD_FI)) {
        set_error("expected 'fi' to close if statement");
        return nullptr;
    }

    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<ForNode> Parser::parse_for() {
    advance(); // consume 'for'
    if (!check(TokenType::TOKEN_WORD)) {
        set_error("expected variable name after 'for'");
        return nullptr;
    }
    std::string var_name = advance().value;
    consume_newlines();

    auto node = std::make_shared<ForNode>();
    node->var_name = var_name;

    if (match(TokenType::KEYWORD_IN)) {
        node->explicit_words = true;
        while (!check(TokenType::TOKEN_SEMI) && !check(TokenType::TOKEN_NEWLINE) && !check(TokenType::KEYWORD_DO) && !check(TokenType::TOKEN_EOF)) {
            if (check(TokenType::TOKEN_WORD)) {
                node->words.push_back(advance().value);
            } else {
                break;
            }
        }
        if (check(TokenType::TOKEN_SEMI) || check(TokenType::TOKEN_NEWLINE)) {
            advance();
        }
    } else {
        node->explicit_words = false;
        if (check(TokenType::TOKEN_SEMI) || check(TokenType::TOKEN_NEWLINE)) {
            advance();
        }
    }

    consume_newlines();
    if (!match(TokenType::KEYWORD_DO)) {
        set_error("expected 'do' in for loop");
        return nullptr;
    }

    node->body = parse_command_list();
    if (!match(TokenType::KEYWORD_DONE)) {
        set_error("expected 'done' to close for loop");
        return nullptr;
    }

    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<WhileNode> Parser::parse_while(bool is_until) {
    advance(); // consume 'while' or 'until'
    auto node = std::make_shared<WhileNode>();
    node->is_until = is_until;

    node->condition = parse_command_list();
    if (!match(TokenType::KEYWORD_DO)) {
        set_error("expected 'do' after while/until condition");
        return nullptr;
    }

    node->body = parse_command_list();
    if (!match(TokenType::KEYWORD_DONE)) {
        set_error("expected 'done' to close while/until loop");
        return nullptr;
    }

    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<CaseNode> Parser::parse_case() {
    advance(); // consume 'case'
    if (!check(TokenType::TOKEN_WORD)) {
        set_error("expected word after 'case'");
        return nullptr;
    }
    std::string word = advance().value;
    consume_newlines();

    if (!match(TokenType::KEYWORD_IN)) {
        set_error("expected 'in' after case word");
        return nullptr;
    }

    auto node = std::make_shared<CaseNode>();
    node->word = word;

    while (!check(TokenType::KEYWORD_ESAC) && !check(TokenType::TOKEN_EOF)) {
        consume_newlines();
        if (check(TokenType::KEYWORD_ESAC) || check(TokenType::TOKEN_EOF)) break;

        match(TokenType::TOKEN_LPAREN);

        CaseItem item;
        while (check(TokenType::TOKEN_WORD)) {
            item.patterns.push_back(advance().value);
            if (match(TokenType::TOKEN_PIPE)) {
                continue;
            }
            break;
        }

        if (!match(TokenType::TOKEN_RPAREN)) {
            set_error("expected ')' after case pattern");
            return nullptr;
        }

        item.body = parse_command_list();
        match(TokenType::TOKEN_DSEMI); // consume ';;'
        node->items.push_back(item);
    }

    if (!match(TokenType::KEYWORD_ESAC)) {
        set_error("expected 'esac' to close case statement");
        return nullptr;
    }

    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

std::shared_ptr<FunctionDefNode> Parser::parse_function() {
    advance(); // consume 'function'
    if (!check(TokenType::TOKEN_WORD)) {
        set_error("expected function name after 'function'");
        return nullptr;
    }
    std::string fname = advance().value;
    if (match(TokenType::TOKEN_LPAREN)) {
        if (!match(TokenType::TOKEN_RPAREN)) {
            set_error("expected ')' after '(' in function definition");
            return nullptr;
        }
    }

    consume_newlines();
    auto body = parse_command();
    if (!body) {
        set_error("expected function body");
        return nullptr;
    }

    auto node = std::make_shared<FunctionDefNode>();
    node->name = fname;
    node->body = body;

    while (is_redirection_token(peek().type)) {
        parse_redirection(node->redirections);
    }
    lexer_.process_heredocs(node->redirections);
    return node;
}

} // namespace aswell
