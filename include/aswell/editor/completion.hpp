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

    // Test hooks
    void invalidate_cache() { path_cache_valid_ = false; }
    size_t cache_size() const { return path_cache_.size(); }

private:
    // Scoring: 0 exact, 1 prefix, 2 case-insensitive prefix, 3 substring,
    // 4 subsequence, 100 no match. Lower is better.
    static int match_score(const std::string& pattern, const std::string& target);
    static bool subsequence_match(std::string_view pattern, std::string_view target);

    void refresh_path_cache();
    std::vector<CompletionCandidate> complete_command(const std::string& prefix);
    std::vector<CompletionCandidate> complete_path(const std::string& prefix, bool dirs_only = false);
    std::vector<CompletionCandidate> complete_variable(const std::string& prefix, bool bare = false);
    std::vector<CompletionCandidate> complete_git(const std::string& prefix);
    std::vector<CompletionCandidate> complete_option_flags(const std::string& cmd,
                                                           const std::string& prefix);
    std::vector<CompletionCandidate> complete_git_args(const std::vector<std::string>& args,
                                                       const std::string& prefix);
    std::vector<std::string> tokenize(const std::string& s);

    static void rank_and_truncate(std::vector<std::pair<int, CompletionCandidate>>& scored,
                                  std::vector<CompletionCandidate>& out,
                                  const std::string& prefix, size_t limit = 50);

    Environment& env_;

    // PATH executable cache: refreshed at most every 5s or when PATH changes.
    std::vector<std::pair<std::string, std::string>> path_cache_; // (name, dir)
    std::string path_cache_key_;
    int64_t path_cache_time_ms_ = 0;
    bool path_cache_valid_ = false;
};

} // namespace aswell
