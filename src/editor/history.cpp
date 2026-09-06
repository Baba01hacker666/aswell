#include "aswell/editor/history.hpp"
#include <fstream>

namespace aswell {

History::History(std::string history_file)
    : history_file_(std::move(history_file)) {
    if (history_file_.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            history_file_ = std::string(home) + "/.aswell_history";
        }
    }
    load();
}

void History::add(const std::string& line) {
    std::string trimmed = str_util::trim(line);
    if (trimmed.empty()) return;
    if (!entries_.empty() && entries_.back() == trimmed) return; // Deduplicate consecutive

    entries_.push_back(trimmed);
    if (entries_.size() > max_entries_) {
        entries_.erase(entries_.begin());
    }
    save();
}

std::string History::get(size_t index) const {
    if (index < entries_.size()) {
        return entries_[index];
    }
    return "";
}

std::optional<std::string> History::find_prefix(const std::string& prefix) const {
    if (prefix.empty()) return std::nullopt;

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (str_util::starts_with(*it, prefix) && it->size() > prefix.size()) {
            return *it;
        }
    }
    return std::nullopt;
}

std::vector<std::string> History::search(const std::string& query) const {
    std::vector<std::string> results;
    if (query.empty()) return results;

    std::string lq = str_util::to_lower(query);
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (str_util::to_lower(*it).find(lq) != std::string::npos) {
            if (std::find(results.begin(), results.end(), *it) == results.end()) {
                results.push_back(*it);
                if (results.size() >= 20) break;
            }
        }
    }
    return results;
}

void History::load() {
    if (history_file_.empty()) return;
    std::ifstream file(history_file_);
    if (!file) return;

    entries_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            entries_.push_back(line);
        }
    }
}

void History::save() {
    if (history_file_.empty()) return;
    std::ofstream file(history_file_);
    if (!file) return;

    for (const auto& entry : entries_) {
        file << entry << "\n";
    }
}

} // namespace aswell
