#pragma once

#include "aswell/common.hpp"

namespace aswell {

class History {
public:
    explicit History(std::string history_file = "");

    void add(const std::string& line);
    size_t size() const { return entries_.size(); }
    std::string get(size_t index) const;

    std::optional<std::string> find_prefix(const std::string& prefix) const;
    std::vector<std::string> search(const std::string& query) const;

    void save();
    void load();

private:
    std::string history_file_;
    std::vector<std::string> entries_;
    size_t max_entries_ = 10000;
};

} // namespace aswell
