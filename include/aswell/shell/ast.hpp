#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/token.hpp"

namespace aswell {

enum class RedirType {
    LESS,           // <
    GREAT,          // >
    DGREAT,         // >>
    LESSAND,        // <&
    GREATAND,       // >&
    LESSGREAT,      // <>
    CLOBBER,        // >|
    DLESS,          // <<
    DLESSDASH,      // <<-
    HERESTRING      // <<<
};

struct Redirection {
    RedirType type;
    int io_number = -1;             // -1 means default: 0 for input, 1 for output
    std::string target;             // target file, fd descriptor (e.g. "1"), or delimiter
    std::string heredoc_content;    // content of heredoc if type is DLESS/DLESSDASH
    bool heredoc_expand = false;    // true if delimiter was unquoted
};

enum class ASTNodeType {
    SIMPLE_COMMAND,
    PIPELINE,
    AND_OR,
    COMMAND_LIST,
    SUBSHELL,
    GROUPING,
    IF,
    FOR,
    WHILE,
    CASE,
    FUNCTION_DEF
};

class CommandNode {
public:
    virtual ~CommandNode() = default;
    virtual ASTNodeType get_type() const = 0;
};

class SimpleCommandNode : public CommandNode {
public:
    std::vector<std::pair<std::string, std::string>> assignments; // VAR=val
    std::vector<std::string> words;                               // cmd name and args
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::SIMPLE_COMMAND; }
};

class PipelineNode : public CommandNode {
public:
    std::vector<std::shared_ptr<CommandNode>> commands;
    bool negated = false; // ! pipeline

    ASTNodeType get_type() const override { return ASTNodeType::PIPELINE; }
};

class AndOrNode : public CommandNode {
public:
    enum class Op { NONE, AND, OR };
    std::vector<std::pair<Op, std::shared_ptr<PipelineNode>>> pipelines;

    ASTNodeType get_type() const override { return ASTNodeType::AND_OR; }
};

class CommandListNode : public CommandNode {
public:
    struct Item {
        std::shared_ptr<AndOrNode> and_or;
        bool async = false; // & or ;
    };
    std::vector<Item> items;

    ASTNodeType get_type() const override { return ASTNodeType::COMMAND_LIST; }
};

class SubshellNode : public CommandNode {
public:
    std::shared_ptr<CommandListNode> body;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::SUBSHELL; }
};

class GroupingNode : public CommandNode {
public:
    std::shared_ptr<CommandListNode> body;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::GROUPING; }
};

class IfNode : public CommandNode {
public:
    struct Branch {
        std::shared_ptr<CommandListNode> condition;
        std::shared_ptr<CommandListNode> body;
    };
    std::vector<Branch> branches; // [0] is 'if', [1..n] are 'elif'
    std::shared_ptr<CommandListNode> else_branch;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::IF; }
};

class ForNode : public CommandNode {
public:
    std::string var_name;
    std::vector<std::string> words;
    bool explicit_words = true; // false means default "$@"
    std::shared_ptr<CommandListNode> body;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::FOR; }
};

class WhileNode : public CommandNode {
public:
    bool is_until = false;
    std::shared_ptr<CommandListNode> condition;
    std::shared_ptr<CommandListNode> body;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::WHILE; }
};

struct CaseItem {
    std::vector<std::string> patterns;
    std::shared_ptr<CommandListNode> body;
};

class CaseNode : public CommandNode {
public:
    std::string word;
    std::vector<CaseItem> items;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::CASE; }
};

class FunctionDefNode : public CommandNode {
public:
    std::string name;
    std::shared_ptr<CommandNode> body;
    std::vector<Redirection> redirections;

    ASTNodeType get_type() const override { return ASTNodeType::FUNCTION_DEF; }
};

} // namespace aswell
