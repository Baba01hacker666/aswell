#pragma once

#include "aswell/common.hpp"
#include "aswell/editor/history.hpp"

namespace aswell {

class AutoSuggestions {
public:
    explicit AutoSuggestions(const History& history) : history_(history) {}

    std::string get_suggestion_suffix(const std::string& current_buffer) const {
        if (current_buffer.empty()) return "";
        auto match = history_.find_prefix(current_buffer);
        if (match.has_value() && match->size() > current_buffer.size()) {
            return match->substr(current_buffer.size());
        }
        return "";
    }

private:
    const History& history_;
};

} // namespace aswell
