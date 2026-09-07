#pragma once

#include "aswell/common.hpp"

namespace aswell {

class History {
public:
    explicit History(std::string history_file = "");

    void add(const std::string& line);
    size_t size() const { return entries_.size(); }
    std::string get(size_t index) const;

    void pop_last();
    void replace_last(const std::string& line);
    void clear();
    bool remove_at(size_t index);
    const std::vector<std::string>& get_entries() const { return entries_; }

    std::optional<std::string> find_prefix(const std::string& prefix) const;
    std::vector<std::string> search(const std::string& query) const;

    // History expansion (csh/bash-style): !!, !$, !^, !*, !-n, !n, !prefix, !?str
    std::optional<std::string> expand_history(const std::string& line, std::string& error_msg) const;

    void save();
    void load();

private:
    std::string history_file_;
    std::vector<std::string> entries_;
    size_t max_entries_ = 10000;
};

} // namespace aswell
