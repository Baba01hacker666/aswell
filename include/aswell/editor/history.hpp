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

    // Anon profile: history lives only for the life of the shell. Nothing is read
    // from or written to disk, so the shell leaves no trace of what was typed.
    void set_persistent(bool persistent);

    // Policy knobs, driven by ~/.config/aswell/config.txt (history_size,
    // history_ignore_dups) so behaviour is configurable without recompiling.
    void set_max_entries(size_t max_entries);
    size_t max_entries() const { return max_entries_; }
    void set_ignore_dups(bool ignore) { ignore_dups_ = ignore; }
    bool ignore_dups() const { return ignore_dups_; }

private:
    std::string history_file_;
    bool persistent_ = true;
    std::vector<std::string> entries_;
    size_t max_entries_ = 10000;
    bool ignore_dups_ = false;
};

} // namespace aswell
