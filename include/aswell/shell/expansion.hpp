#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

// Forward declaration of command runner callback
using CommandSubstitutionCallback = std::function<std::string(const std::string& script)>;

class Expansion {
public:
    explicit Expansion(Environment& env, CommandSubstitutionCallback cmd_sub = nullptr);

    // Full POSIX expansion of a word list into arguments
    std::vector<std::string> expand_words(const std::vector<std::string>& words);

    // Brace expansion (Stage 0): {a,b,c}, {1..10}, {01..05}, {a..z}
    std::vector<std::string> expand_braces(const std::string& word);

    // Expand a single word without word splitting or globbing (e.g. for variable assignment, case word)
    std::string expand_word_single(const std::string& word);

    // Expand arithmetic expression string: $(( expr ))
    int64_t evaluate_arithmetic(const std::string& expr);

    // Pathname expansion (globbing) on an unquoted word
    std::vector<std::string> expand_glob(const std::string& pattern);

    // Pattern matching helper for case statement or parameter strip (#, %, etc.)
    static bool fnmatch_pattern(const std::string& pattern, const std::string& str);

    void set_command_substitution_callback(CommandSubstitutionCallback cb) {
        cmd_sub_ = cb;
    }

private:
    std::string expand_tilde(const std::string& word);
    std::string expand_parameters_and_commands(const std::string& word, std::vector<bool>& quote_mask);
    std::vector<std::string> split_fields(const std::string& expanded, const std::vector<bool>& quote_mask);
    std::string remove_quotes(const std::string& word);

    std::string handle_parameter_expansion(const std::string& param_expr);

    Environment& env_;
    CommandSubstitutionCallback cmd_sub_;
};

} // namespace aswell
