#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

struct CompletionCandidate {
    std::string text;
    std::string display_name;
    std::string description;
    bool is_directory = false;
};

class CompletionEngine {
public:
    explicit CompletionEngine(Environment& env);

    std::vector<CompletionCandidate> complete(const std::string& buffer, size_t cursor_pos);

private:
    std::vector<CompletionCandidate> complete_command(const std::string& prefix);
    std::vector<CompletionCandidate> complete_path(const std::string& prefix);
    std::vector<CompletionCandidate> complete_variable(const std::string& prefix);
    std::vector<CompletionCandidate> complete_git(const std::string& prefix);

    Environment& env_;
};

} // namespace aswell
