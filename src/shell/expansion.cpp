#include "aswell/shell/expansion.hpp"
#include <fnmatch.h>
#include <glob.h>
#include <pwd.h>
#include <cmath>
#include <iomanip>

namespace aswell {

namespace {

bool is_valid_integer_str(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
    }
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

bool parse_brace_range(const std::string& inner, std::vector<std::string>& items) {
    size_t dot1 = inner.find("..");
    if (dot1 == std::string::npos || dot1 == 0) return false;
    std::string part1 = inner.substr(0, dot1);
    size_t dot2 = inner.find("..", dot1 + 2);
    std::string part2, part3;
    if (dot2 != std::string::npos) {
        part2 = inner.substr(dot1 + 2, dot2 - (dot1 + 2));
        part3 = inner.substr(dot2 + 2);
        if (part3.find("..") != std::string::npos) return false;
    } else {
        part2 = inner.substr(dot1 + 2);
    }
    if (part2.empty()) return false;

    // Check numeric range
    if (is_valid_integer_str(part1) && is_valid_integer_str(part2) && (part3.empty() || is_valid_integer_str(part3))) {
        long long start_val = 0;
        long long end_val = 0;
        long long step_val = 1;
        try {
            start_val = std::stoll(part1);
            end_val = std::stoll(part2);
            if (!part3.empty()) {
                step_val = std::stoll(part3);
            }
        } catch (...) {
            return false;
        }
        if (step_val <= 0) step_val = 1;

        bool pad = (part1.size() > 1 && (part1[0] == '0' || (part1[0] == '-' && part1[1] == '0'))) ||
                   (part2.size() > 1 && (part2[0] == '0' || (part2[0] == '-' && part2[1] == '0')));
        size_t width = std::max(part1.size(), part2.size());

        if (start_val <= end_val) {
            for (long long v = start_val; v <= end_val; v += step_val) {
                if (pad) {
                    std::ostringstream ss;
                    if (v < 0) {
                        size_t num_w = (width > 1) ? (width - 1) : 1;
                        ss << "-" << std::setw(static_cast<int>(num_w)) << std::setfill('0') << -v;
                    } else {
                        ss << std::setw(static_cast<int>(width)) << std::setfill('0') << v;
                    }
                    items.push_back(ss.str());
                } else {
                    items.push_back(std::to_string(v));
                }
            }
        } else {
            for (long long v = start_val; v >= end_val; v -= step_val) {
                if (pad) {
                    std::ostringstream ss;
                    if (v < 0) {
                        size_t num_w = (width > 1) ? (width - 1) : 1;
                        ss << "-" << std::setw(static_cast<int>(num_w)) << std::setfill('0') << -v;
                    } else {
                        ss << std::setw(static_cast<int>(width)) << std::setfill('0') << v;
                    }
                    items.push_back(ss.str());
                } else {
                    items.push_back(std::to_string(v));
                }
            }
        }
        return true;
    }

    // Check char range
    if (part1.size() == 1 && part2.size() == 1 && (part3.empty() || is_valid_integer_str(part3))) {
        char c1 = part1[0];
        char c2 = part2[0];
        if ((std::islower(static_cast<unsigned char>(c1)) && std::islower(static_cast<unsigned char>(c2))) ||
            (std::isupper(static_cast<unsigned char>(c1)) && std::isupper(static_cast<unsigned char>(c2)))) {
            int step = 1;
            if (!part3.empty()) {
                try {
                    step = std::stoi(part3);
                } catch (...) {
                    return false;
                }
            }
            if (step <= 0) step = 1;

            if (c1 <= c2) {
                for (int c = c1; c <= c2; c += step) {
                    items.push_back(std::string(1, static_cast<char>(c)));
                }
            } else {
                for (int c = c1; c >= c2; c -= step) {
                    items.push_back(std::string(1, static_cast<char>(c)));
                }
            }
            return true;
        }
    }

    return false;
}

// Arithmetic expression parser
class ArithmeticParser {
public:
    explicit ArithmeticParser(std::string expr, Environment& env)
        : input_(std::move(expr)), env_(env) {}

    int64_t parse() {
        skip_ws();
        if (pos_ >= input_.size()) return 0;
        int64_t result = parse_ternary();
        return result;
    }

private:
    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            pos_++;
        }
    }

    char peek() const {
        if (pos_ >= input_.size()) return '\0';
        return input_[pos_];
    }

    bool match(std::string_view op) {
        skip_ws();
        if (input_.size() - pos_ >= op.size() && input_.substr(pos_, op.size()) == op) {
            pos_ += op.size();
            return true;
        }
        return false;
    }

    int64_t parse_ternary() {
        int64_t cond = parse_logical_or();
        skip_ws();
        if (match("?")) {
            int64_t true_val = parse_ternary();
            skip_ws();
            if (!match(":")) {
                return true_val;
            }
            int64_t false_val = parse_ternary();
            return cond ? true_val : false_val;
        }
        return cond;
    }

    int64_t parse_logical_or() {
        int64_t left = parse_logical_and();
        while (match("||")) {
            int64_t right = parse_logical_and();
            left = (left || right) ? 1 : 0;
        }
        return left;
    }

    int64_t parse_logical_and() {
        int64_t left = parse_bitwise_or();
        while (match("&&")) {
            int64_t right = parse_bitwise_or();
            left = (left && right) ? 1 : 0;
        }
        return left;
    }

    int64_t parse_bitwise_or() {
        int64_t left = parse_bitwise_xor();
        while (peek() == '|' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '|')) {
            pos_++;
            int64_t right = parse_bitwise_xor();
            left = left | right;
        }
        return left;
    }

    int64_t parse_bitwise_xor() {
        int64_t left = parse_bitwise_and();
        while (match("^")) {
            int64_t right = parse_bitwise_and();
            left = left ^ right;
        }
        return left;
    }

    int64_t parse_bitwise_and() {
        int64_t left = parse_equality();
        while (peek() == '&' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '&')) {
            pos_++;
            int64_t right = parse_equality();
            left = left & right;
        }
        return left;
    }

    int64_t parse_equality() {
        int64_t left = parse_relational();
        while (true) {
            if (match("==")) {
                int64_t right = parse_relational();
                left = (left == right) ? 1 : 0;
            } else if (match("!=")) {
                int64_t right = parse_relational();
                left = (left != right) ? 1 : 0;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_relational() {
        int64_t left = parse_shift();
        while (true) {
            if (match("<=")) {
                int64_t right = parse_shift();
                left = (left <= right) ? 1 : 0;
            } else if (match(">=")) {
                int64_t right = parse_shift();
                left = (left >= right) ? 1 : 0;
            } else if (peek() == '<' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '<')) {
                pos_++;
                int64_t right = parse_shift();
                left = (left < right) ? 1 : 0;
            } else if (peek() == '>' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '>')) {
                pos_++;
                int64_t right = parse_shift();
                left = (left > right) ? 1 : 0;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_shift() {
        int64_t left = parse_additive();
        while (true) {
            if (match("<<")) {
                int64_t right = parse_additive();
                left = left << right;
            } else if (match(">>")) {
                int64_t right = parse_additive();
                left = left >> right;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_additive() {
        int64_t left = parse_multiplicative();
        while (true) {
            skip_ws();
            if (peek() == '+' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '+')) {
                pos_++;
                int64_t right = parse_multiplicative();
                left += right;
            } else if (peek() == '-' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '-')) {
                pos_++;
                int64_t right = parse_multiplicative();
                left -= right;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_multiplicative() {
        int64_t left = parse_power();
        while (true) {
            skip_ws();
            if (peek() == '*' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '*')) {
                pos_++;
                int64_t right = parse_power();
                left *= right;
            } else if (match("/")) {
                int64_t right = parse_power();
                if (right != 0) left /= right;
                else left = 0;
            } else if (match("%")) {
                int64_t right = parse_power();
                if (right != 0) left %= right;
                else left = 0;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_power() {
        int64_t left = parse_unary();
        if (match("**")) {
            int64_t right = parse_power();
            int64_t res = 1;
            for (int64_t i = 0; i < right; ++i) res *= left;
            return res;
        }
        return left;
    }

    int64_t parse_unary() {
        skip_ws();
        if (match("+")) {
            return parse_unary();
        }
        if (match("-")) {
            return -parse_unary();
        }
        if (match("~")) {
            return ~parse_unary();
        }
        if (match("!")) {
            return !parse_unary();
        }
        return parse_primary();
    }

    int64_t parse_primary() {
        skip_ws();
        if (match("(")) {
            int64_t val = parse_ternary();
            skip_ws();
            match(")");
            return val;
        }

        // Integer literal (dec, hex, octal)
        if (std::isdigit(static_cast<unsigned char>(peek()))) {
            size_t start = pos_;
            if (peek() == '0' && pos_ + 1 < input_.size() && (input_[pos_ + 1] == 'x' || input_[pos_ + 1] == 'X')) {
                pos_ += 2;
                while (pos_ < input_.size() && std::isxdigit(static_cast<unsigned char>(input_[pos_]))) pos_++;
                return std::stoll(input_.substr(start, pos_ - start), nullptr, 16);
            }
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) pos_++;
            return std::stoll(input_.substr(start, pos_ - start), nullptr, 10);
        }

        // Variable name or identifier
        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_') {
            size_t start = pos_;
            while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
                pos_++;
            }
            std::string var_name = input_.substr(start, pos_ - start);
            std::string val_str = env_.get_var(var_name);
            if (val_str.empty()) return 0;
            try {
                // Recursively evaluate variable value as arithmetic
                ArithmeticParser sub_parser(val_str, env_);
                return sub_parser.parse();
            } catch (...) {
                return 0;
            }
        }

        // Advance past unknown char
        if (pos_ < input_.size()) pos_++;
        return 0;
    }

    std::string input_;
    size_t pos_ = 0;
    Environment& env_;
};

} // namespace

Expansion::Expansion(Environment& env, CommandSubstitutionCallback cmd_sub)
    : env_(env), cmd_sub_(cmd_sub) {}

bool Expansion::fnmatch_pattern(const std::string& pattern, const std::string& str) {
    return fnmatch(pattern.c_str(), str.c_str(), 0) == 0;
}

int64_t Expansion::evaluate_arithmetic(const std::string& expr) {
    ArithmeticParser parser(expr, env_);
    return parser.parse();
}

std::string Expansion::expand_tilde(const std::string& word) {
    if (word.empty() || word[0] != '~') {
        return word;
    }

    size_t slash_pos = word.find('/');
    std::string user = (slash_pos == std::string::npos) ? word.substr(1) : word.substr(1, slash_pos - 1);
    std::string rest = (slash_pos == std::string::npos) ? "" : word.substr(slash_pos);

    if (user.empty()) {
        std::string home = env_.get_var("HOME");
        if (home.empty()) {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_dir) home = pw->pw_dir;
        }
        return home + rest;
    } else {
        struct passwd* pw = getpwnam(user.c_str());
        if (pw && pw->pw_dir) {
            return std::string(pw->pw_dir) + rest;
        }
        return word; // Tilde expansion fails if user not found
    }
}

std::string Expansion::handle_parameter_expansion(const std::string& expr) {
    if (expr.empty()) return "";

    // Length: ${#VAR}
    if (expr.size() > 1 && expr[0] == '#' && expr[1] != '?' && expr[1] != '!' && expr[1] != '$') {
        std::string var_name = expr.substr(1);
        std::string val = env_.get_var(var_name);
        return std::to_string(val.size());
    }

    // Prefix strip: ${VAR#pattern} (shortest) and ${VAR##pattern} (longest)
    size_t hash_pos = expr.find('#');
    if (hash_pos != std::string::npos && hash_pos > 0) {
        bool longest = (hash_pos + 1 < expr.size() && expr[hash_pos + 1] == '#');
        std::string var_name = expr.substr(0, hash_pos);
        std::string pat = expr.substr(longest ? hash_pos + 2 : hash_pos + 1);
        pat = expand_word_single(pat);
        std::string val = env_.get_var(var_name);

        if (longest) {
            for (size_t len = val.size(); len > 0; --len) {
                if (fnmatch_pattern(pat, val.substr(0, len))) {
                    return val.substr(len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.size(); ++len) {
                if (fnmatch_pattern(pat, val.substr(0, len))) {
                    return val.substr(len);
                }
            }
        }
        return val;
    }

    // Suffix strip: ${VAR%pattern} (shortest) and ${VAR%%pattern} (longest)
    size_t pct_pos = expr.find('%');
    if (pct_pos != std::string::npos && pct_pos > 0) {
        bool longest = (pct_pos + 1 < expr.size() && expr[pct_pos + 1] == '%');
        std::string var_name = expr.substr(0, pct_pos);
        std::string pat = expr.substr(longest ? pct_pos + 2 : pct_pos + 1);
        pat = expand_word_single(pat);
        std::string val = env_.get_var(var_name);

        if (longest) {
            for (size_t len = val.size(); len > 0; --len) {
                if (fnmatch_pattern(pat, val.substr(val.size() - len))) {
                    return val.substr(0, val.size() - len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.size(); ++len) {
                if (fnmatch_pattern(pat, val.substr(val.size() - len))) {
                    return val.substr(0, val.size() - len);
                }
            }
        }
        return val;
    }

    // Substring: ${VAR:offset} or ${VAR:offset:length}
    size_t colon_pos = expr.find(':');
    if (colon_pos != std::string::npos && colon_pos + 1 < expr.size()) {
        char next_ch = expr[colon_pos + 1];
        if (next_ch != '-' && next_ch != '=' && next_ch != '?' && next_ch != '+') {
            std::string var_name = expr.substr(0, colon_pos);
            std::string rest = expr.substr(colon_pos + 1);
            size_t second_colon = rest.find(':');
            std::string offset_str = (second_colon == std::string::npos) ? rest : rest.substr(0, second_colon);
            std::string len_str = (second_colon == std::string::npos) ? "" : rest.substr(second_colon + 1);

            int64_t offset = evaluate_arithmetic(offset_str);
            std::string val = env_.get_var(var_name);
            if (offset < 0) offset = static_cast<int64_t>(val.size()) + offset;
            if (offset < 0) offset = 0;
            if (static_cast<size_t>(offset) >= val.size()) return "";

            if (!len_str.empty()) {
                int64_t len = evaluate_arithmetic(len_str);
                if (len <= 0) return "";
                return val.substr(static_cast<size_t>(offset), static_cast<size_t>(len));
            }
            return val.substr(static_cast<size_t>(offset));
        }
    }

    // Default / Alternative / Error operators: :-, -, :=, =, :?, ?, :+, +
    for (size_t i = 0; i < expr.size(); ++i) {
        if (expr[i] == ':' && i + 1 < expr.size() && std::strchr("-=?+", expr[i + 1])) {
            std::string var_name = expr.substr(0, i);
            char op = expr[i + 1];
            std::string word = expr.substr(i + 2);
            word = expand_word_single(word);
            std::string val = env_.get_var(var_name);
            bool is_null_or_unset = val.empty();

            if (op == '-') {
                return is_null_or_unset ? word : val;
            } else if (op == '=') {
                if (is_null_or_unset) {
                    env_.set_var(var_name, word);
                    return word;
                }
                return val;
            } else if (op == '?') {
                if (is_null_or_unset) {
                    std::cerr << "aswell: " << var_name << ": " << (word.empty() ? "parameter null or not set" : word) << "\n";
                    return "";
                }
                return val;
            } else if (op == '+') {
                return is_null_or_unset ? "" : word;
            }
        } else if (std::strchr("-=?+", expr[i])) {
            std::string var_name = expr.substr(0, i);
            char op = expr[i];
            std::string word = expr.substr(i + 1);
            word = expand_word_single(word);
            bool has = env_.has_var(var_name);
            std::string val = env_.get_var(var_name);

            if (op == '-') {
                return !has ? word : val;
            } else if (op == '=') {
                if (!has) {
                    env_.set_var(var_name, word);
                    return word;
                }
                return val;
            } else if (op == '?') {
                if (!has) {
                    std::cerr << "aswell: " << var_name << ": " << (word.empty() ? "parameter not set" : word) << "\n";
                    return "";
                }
                return val;
            } else if (op == '+') {
                return has ? word : "";
            }
        }
    }

    // Pattern replacement: ${VAR/pattern/replace} or ${VAR//pattern/replace}
    size_t slash_pos = expr.find('/');
    if (slash_pos != std::string::npos) {
        std::string var_name = expr.substr(0, slash_pos);
        bool replace_all = false;
        size_t pat_start = slash_pos + 1;
        if (pat_start < expr.size() && expr[pat_start] == '/') {
            replace_all = true;
            pat_start++;
        }
        size_t repl_pos = expr.find('/', pat_start);
        std::string pat = (repl_pos == std::string::npos) ? expr.substr(pat_start) : expr.substr(pat_start, repl_pos - pat_start);
        std::string repl = (repl_pos == std::string::npos) ? "" : expr.substr(repl_pos + 1);

        std::string val = env_.get_var(var_name);
        if (!pat.empty()) {
            size_t idx = 0;
            while ((idx = val.find(pat, idx)) != std::string::npos) {
                val.replace(idx, pat.size(), repl);
                idx += repl.size();
                if (!replace_all) break;
            }
        }
        return val;
    }

    // Plain variable
    return env_.get_var(expr);
}

std::string Expansion::expand_parameters_and_commands(const std::string& word, std::vector<bool>& quote_mask) {
    std::string out;
    quote_mask.clear();

    bool in_single_quote = false;
    bool in_double_quote = false;

    for (size_t i = 0; i < word.size(); ) {
        char c = word[i];

        // Single quote toggles if not in double quote
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            out += c;
            quote_mask.push_back(true);
            i++;
            continue;
        }

        // Inside single quotes: completely literal
        if (in_single_quote) {
            out += c;
            quote_mask.push_back(true);
            i++;
            continue;
        }

        // Double quote toggle
        if (c == '\"') {
            in_double_quote = !in_double_quote;
            out += c;
            quote_mask.push_back(true);
            i++;
            continue;
        }

        // Backslash escape
        if (c == '\\') {
            if (in_double_quote) {
                if (i + 1 < word.size() && (word[i + 1] == '$' || word[i + 1] == '`' || word[i + 1] == '\"' || word[i + 1] == '\\')) {
                    out += word[i + 1];
                    quote_mask.push_back(true);
                    i += 2;
                    continue;
                }
            } else {
                if (i + 1 < word.size()) {
                    out += word[i + 1];
                    quote_mask.push_back(true);
                    i += 2;
                    continue;
                }
            }
            out += c;
            quote_mask.push_back(in_double_quote);
            i++;
            continue;
        }

        // Arithmetic expansion: $(( expr ))
        if (c == '$' && i + 2 < word.size() && word[i + 1] == '(' && word[i + 2] == '(') {
            size_t depth = 2;
            size_t start = i + 3;
            size_t cur = start;
            while (cur < word.size() && depth > 0) {
                if (word[cur] == '(') depth++;
                else if (word[cur] == ')') {
                    depth--;
                    if (depth == 0) break;
                }
                cur++;
            }
            if (cur < word.size() && depth == 0) {
                std::string arith_expr = word.substr(start, cur - start);
                i = cur + 1; // skip past the final closing paren
                // Expand any inner vars first
                std::vector<bool> dummy_mask;
                arith_expr = expand_parameters_and_commands(arith_expr, dummy_mask);
                int64_t val = evaluate_arithmetic(arith_expr);
                std::string res = std::to_string(val);
                for (char rc : res) {
                    out += rc;
                    quote_mask.push_back(in_double_quote);
                }
                continue;
            }
        }

        // Command substitution: $( cmd )
        if (c == '$' && i + 1 < word.size() && word[i + 1] == '(') {
            size_t depth = 1;
            size_t start = i + 2;
            size_t cur = start;
            while (cur < word.size() && depth > 0) {
                if (word[cur] == '(') depth++;
                else if (word[cur] == ')') depth--;
                else if (word[cur] == '\'') {
                    cur++;
                    while (cur < word.size() && word[cur] != '\'') cur++;
                } else if (word[cur] == '\"') {
                    cur++;
                    while (cur < word.size() && word[cur] != '\"') {
                        if (word[cur] == '\\') cur++;
                        cur++;
                    }
                }
                if (depth == 0) break;
                cur++;
            }
            std::string sub_cmd = word.substr(start, cur - start);
            i = (cur < word.size()) ? cur + 1 : word.size();

            std::string sub_out;
            if (cmd_sub_) {
                sub_out = cmd_sub_(sub_cmd);
            }
            // Strip trailing newlines
            while (!sub_out.empty() && sub_out.back() == '\n') {
                sub_out.pop_back();
            }
            for (char rc : sub_out) {
                out += rc;
                quote_mask.push_back(in_double_quote);
            }
            continue;
        }

        // Command substitution: `cmd`
        if (c == '`') {
            size_t start = i + 1;
            size_t cur = start;
            while (cur < word.size() && word[cur] != '`') {
                if (word[cur] == '\\') cur++;
                cur++;
            }
            std::string sub_cmd = word.substr(start, cur - start);
            i = (cur < word.size()) ? cur + 1 : word.size();

            std::string sub_out;
            if (cmd_sub_) {
                sub_out = cmd_sub_(sub_cmd);
            }
            while (!sub_out.empty() && sub_out.back() == '\n') {
                sub_out.pop_back();
            }
            for (char rc : sub_out) {
                out += rc;
                quote_mask.push_back(in_double_quote);
            }
            continue;
        }

        // Parameter expansion: ${...} or $VAR
        if (c == '$' && i + 1 < word.size()) {
            if (word[i + 1] == '{') {
                size_t start = i + 2;
                size_t cur = start;
                size_t brace_depth = 1;
                while (cur < word.size() && brace_depth > 0) {
                    if (word[cur] == '{') brace_depth++;
                    else if (word[cur] == '}') brace_depth--;
                    if (brace_depth == 0) break;
                    cur++;
                }
                std::string param_expr = word.substr(start, cur - start);
                i = (cur < word.size()) ? cur + 1 : word.size();
                std::string val = handle_parameter_expansion(param_expr);
                for (char rc : val) {
                    out += rc;
                    quote_mask.push_back(in_double_quote);
                }
                continue;
            } else if (std::isalpha(static_cast<unsigned char>(word[i + 1])) || word[i + 1] == '_') {
                size_t start = i + 1;
                size_t cur = start;
                while (cur < word.size() && (std::isalnum(static_cast<unsigned char>(word[cur])) || word[cur] == '_')) {
                    cur++;
                }
                std::string var_name = word.substr(start, cur - start);
                i = cur;
                std::string val = env_.get_var(var_name);
                for (char rc : val) {
                    out += rc;
                    quote_mask.push_back(in_double_quote);
                }
                continue;
            } else if (std::strchr("?#$!0123456789*@-", word[i + 1])) {
                std::string var_name(1, word[i + 1]);
                i += 2;
                std::string val = env_.get_var(var_name);
                for (char rc : val) {
                    out += rc;
                    quote_mask.push_back(in_double_quote);
                }
                continue;
            }
        }

        out += c;
        quote_mask.push_back(in_double_quote);
        i++;
    }

    return out;
}

std::vector<std::string> Expansion::split_fields(const std::string& expanded, const std::vector<bool>& quote_mask) {
    std::string ifs = env_.get_var("IFS");
    if (ifs.empty()) {
        ifs = " \t\n";
    }

    std::vector<std::string> fields;
    std::string cur;

    for (size_t i = 0; i < expanded.size(); ++i) {
        char c = expanded[i];
        bool is_quoted = (i < quote_mask.size()) ? quote_mask[i] : false;

        if (!is_quoted && ifs.find(c) != std::string::npos) {
            if (!cur.empty()) {
                fields.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }

    if (!cur.empty()) {
        fields.push_back(cur);
    }

    return fields;
}

std::string Expansion::remove_quotes(const std::string& word) {
    std::string res;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < word.size(); ++i) {
        char c = word[i];

        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '\"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (c == '\\' && !in_single) {
            if (in_double) {
                if (i + 1 < word.size() && (word[i + 1] == '$' || word[i + 1] == '`' || word[i + 1] == '\"' || word[i + 1] == '\\')) {
                    res += word[++i];
                    continue;
                }
            } else {
                if (i + 1 < word.size()) {
                    res += word[++i];
                    continue;
                }
            }
        }

        res += c;
    }

    return res;
}

std::vector<std::string> Expansion::expand_glob(const std::string& pattern) {
    // Check if pattern contains wildcard characters
    bool has_wildcard = false;
    for (char c : pattern) {
        if (c == '*' || c == '?' || c == '[') {
            has_wildcard = true;
            break;
        }
    }
    if (!has_wildcard) {
        return {pattern};
    }

    glob_t glob_result;
    std::memset(&glob_result, 0, sizeof(glob_result));

    int ret = glob(pattern.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &glob_result);
    std::vector<std::string> matches;

    if (ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            matches.emplace_back(glob_result.gl_pathv[i]);
        }
    } else {
        matches.push_back(pattern);
    }

    globfree(&glob_result);
    return matches;
}

std::string Expansion::expand_word_single(const std::string& word) {
    std::string tilde_expanded = expand_tilde(word);
    std::vector<bool> quote_mask;
    std::string param_expanded = expand_parameters_and_commands(tilde_expanded, quote_mask);
    return remove_quotes(param_expanded);
}

std::vector<std::string> Expansion::expand_braces(const std::string& word) {
    size_t n = word.size();
    size_t i = 0;

    while (i < n) {
        char c = word[i];
        if (c == '\\') {
            i += 2;
            continue;
        }
        if (c == '\'') {
            i++;
            while (i < n && word[i] != '\'') i++;
            if (i < n) i++;
            continue;
        }
        if (c == '"') {
            i++;
            while (i < n && word[i] != '"') {
                if (word[i] == '\\' && i + 1 < n) i += 2;
                else i++;
            }
            if (i < n) i++;
            continue;
        }
        if (c == '$' && i + 1 < n) {
            if (word[i + 1] == '{') {
                i += 2;
                int bdepth = 1;
                while (i < n && bdepth > 0) {
                    if (word[i] == '\\' && i + 1 < n) i += 2;
                    else {
                        if (word[i] == '{') bdepth++;
                        else if (word[i] == '}') bdepth--;
                        i++;
                    }
                }
                continue;
            } else if (word[i + 1] == '(') {
                i += 2;
                int pdepth = 1;
                while (i < n && pdepth > 0) {
                    if (word[i] == '\\' && i + 1 < n) i += 2;
                    else {
                        if (word[i] == '(') pdepth++;
                        else if (word[i] == ')') pdepth--;
                        i++;
                    }
                }
                continue;
            }
        }

        if (c == '{') {
            size_t open_pos = i;
            size_t j = i + 1;
            int depth = 0;
            std::vector<size_t> comma_positions;
            bool found_close = false;

            while (j < n) {
                if (word[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (word[j] == '\'') {
                    j++;
                    while (j < n && word[j] != '\'') j++;
                    if (j < n) j++;
                    continue;
                }
                if (word[j] == '"') {
                    j++;
                    while (j < n && word[j] != '"') {
                        if (word[j] == '\\' && j + 1 < n) j += 2;
                        else j++;
                    }
                    if (j < n) j++;
                    continue;
                }
                if (word[j] == '$' && j + 1 < n && word[j + 1] == '{') {
                    j += 2;
                    int bdepth = 1;
                    while (j < n && bdepth > 0) {
                        if (word[j] == '\\' && j + 1 < n) j += 2;
                        else {
                            if (word[j] == '{') bdepth++;
                            else if (word[j] == '}') bdepth--;
                            j++;
                        }
                    }
                    continue;
                }
                if (word[j] == '{') {
                    depth++;
                    j++;
                } else if (word[j] == '}') {
                    if (depth > 0) {
                        depth--;
                        j++;
                    } else {
                        found_close = true;
                        break;
                    }
                } else if (word[j] == ',' && depth == 0) {
                    comma_positions.push_back(j);
                    j++;
                } else {
                    j++;
                }
            }

            if (found_close) {
                size_t close_pos = j;
                std::string inner = word.substr(open_pos + 1, close_pos - open_pos - 1);
                std::vector<std::string> items;

                if (!comma_positions.empty()) {
                    size_t cur = open_pos + 1;
                    for (size_t comma_idx : comma_positions) {
                        items.push_back(word.substr(cur, comma_idx - cur));
                        cur = comma_idx + 1;
                    }
                    items.push_back(word.substr(cur, close_pos - cur));
                } else {
                    parse_brace_range(inner, items);
                }

                if (!items.empty()) {
                    std::string prefix = word.substr(0, open_pos);
                    std::string suffix = word.substr(close_pos + 1);
                    std::vector<std::string> result;
                    for (const auto& item : items) {
                        std::string candidate = prefix + item + suffix;
                        auto sub = expand_braces(candidate);
                        result.insert(result.end(), sub.begin(), sub.end());
                    }
                    return result;
                }
            }
            i = open_pos + 1;
            continue;
        }

        i++;
    }

    return {word};
}

std::vector<std::string> Expansion::expand_words(const std::vector<std::string>& words) {
    std::vector<std::string> result;

    // Stage 0: Brace expansion
    std::vector<std::string> brace_expanded;
    for (const auto& w : words) {
        auto b = expand_braces(w);
        brace_expanded.insert(brace_expanded.end(), b.begin(), b.end());
    }

    for (const auto& w : brace_expanded) {
        std::string tilde_expanded = expand_tilde(w);
        std::vector<bool> quote_mask;
        std::string param_expanded = expand_parameters_and_commands(tilde_expanded, quote_mask);

        // Word splitting
        auto fields = split_fields(param_expanded, quote_mask);
        if (fields.empty() && !param_expanded.empty()) {
            fields.push_back("");
        }

        // Pathname expansion (globbing) and quote removal for each field
        for (const auto& f : fields) {
            // Only glob if unquoted wildcards exist
            std::vector<std::string> globbed = expand_glob(f);
            for (const auto& g : globbed) {
                result.push_back(remove_quotes(g));
            }
        }
    }

    return result;
}

} // namespace aswell
