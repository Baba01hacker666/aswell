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

void History::pop_last() {
    if (!entries_.empty()) {
        entries_.pop_back();
        save();
    }
}

void History::replace_last(const std::string& line) {
    if (!entries_.empty()) {
        entries_.back() = line;
        save();
    }
}

void History::clear() {
    entries_.clear();
    save();
}

bool History::remove_at(size_t index) {
    if (index < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
        save();
        return true;
    }
    return false;
}

std::string History::get(size_t index) const {
    if (index < entries_.size()) {
        return entries_[index];
    }
    return "";
}

static std::vector<std::string> split_history_words(const std::string& cmd) {
    std::vector<std::string> words;
    size_t i = 0;
    size_t n = cmd.size();

    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(cmd[i]))) {
            i++;
        }
        if (i >= n) break;

        std::string word;
        while (i < n && !std::isspace(static_cast<unsigned char>(cmd[i]))) {
            char c = cmd[i];
            if (c == '\\' && i + 1 < n) {
                word += cmd[i];
                word += cmd[i + 1];
                i += 2;
            } else if (c == '\'') {
                word += c;
                i++;
                while (i < n && cmd[i] != '\'') {
                    word += cmd[i++];
                }
                if (i < n) word += cmd[i++];
            } else if (c == '"') {
                word += c;
                i++;
                while (i < n && cmd[i] != '"') {
                    if (cmd[i] == '\\' && i + 1 < n) {
                        word += cmd[i++];
                        word += cmd[i++];
                    } else {
                        word += cmd[i++];
                    }
                }
                if (i < n) word += cmd[i++];
            } else {
                word += c;
                i++;
            }
        }
        words.push_back(word);
    }
    return words;
}

std::optional<std::string> History::expand_history(const std::string& line, std::string& error_msg) const {
    std::string result;
    size_t n = line.size();
    size_t i = 0;

    auto apply_word_selector = [](const std::string& event, char selector) -> std::string {
        auto words = split_history_words(event);
        if (selector == '^') {
            return words.size() > 1 ? words[1] : (words.empty() ? "" : words[0]);
        }
        if (selector == '$') {
            return words.empty() ? "" : words.back();
        }
        if (selector == '*') {
            std::string joined;
            for (size_t k = 1; k < words.size(); ++k) {
                if (k > 1) joined += " ";
                joined += words[k];
            }
            return joined;
        }
        return event;
    };

    while (i < n) {
        char c = line[i];

        // 1. Single quotes: preserve verbatim
        if (c == '\'') {
            result += '\'';
            i++;
            while (i < n && line[i] != '\'') {
                result += line[i++];
            }
            if (i < n) result += line[i++];
            continue;
        }

        // 2. Escaped characters: \! becomes !
        if (c == '\\') {
            if (i + 1 < n && line[i + 1] == '!') {
                result += '!';
                i += 2;
                continue;
            }
            result += c;
            i++;
            if (i < n) result += line[i++];
            continue;
        }

        // 3. History expansion character: !
        if (c == '!') {
            if (i + 1 >= n || std::isspace(static_cast<unsigned char>(line[i + 1])) ||
                line[i + 1] == '=' || line[i + 1] == '(') {
                result += '!';
                i++;
                continue;
            }

            // A) !!
            if (line[i + 1] == '!') {
                if (entries_.empty()) {
                    error_msg = "!!: event not found";
                    return std::nullopt;
                }
                std::string event = entries_.back();
                i += 2;
                if (i < n && line[i] == ':' && i + 1 < n && (line[i + 1] == '^' || line[i + 1] == '$' || line[i + 1] == '*')) {
                    result += apply_word_selector(event, line[i + 1]);
                    i += 2;
                } else {
                    result += event;
                }
                continue;
            }

            // B) !$
            if (line[i + 1] == '$') {
                if (entries_.empty()) {
                    error_msg = "!$: event not found";
                    return std::nullopt;
                }
                result += apply_word_selector(entries_.back(), '$');
                i += 2;
                continue;
            }

            // C) !^
            if (line[i + 1] == '^') {
                if (entries_.empty()) {
                    error_msg = "!^: event not found";
                    return std::nullopt;
                }
                result += apply_word_selector(entries_.back(), '^');
                i += 2;
                continue;
            }

            // D) !*
            if (line[i + 1] == '*') {
                if (entries_.empty()) {
                    error_msg = "!*: event not found";
                    return std::nullopt;
                }
                result += apply_word_selector(entries_.back(), '*');
                i += 2;
                continue;
            }

            // E) !:^, !:$, !:*
            if (line[i + 1] == ':' && i + 2 < n && (line[i + 2] == '^' || line[i + 2] == '$' || line[i + 2] == '*')) {
                if (entries_.empty()) {
                    error_msg = line.substr(i, 3) + ": event not found";
                    return std::nullopt;
                }
                result += apply_word_selector(entries_.back(), line[i + 2]);
                i += 3;
                continue;
            }

            // F) !-N (relative offset)
            if (line[i + 1] == '-' && i + 2 < n && std::isdigit(static_cast<unsigned char>(line[i + 2]))) {
                size_t start_d = i + 2;
                size_t end_d = start_d;
                while (end_d < n && std::isdigit(static_cast<unsigned char>(line[end_d]))) end_d++;
                int offset = std::stoi(line.substr(start_d, end_d - start_d));
                if (offset <= 0 || static_cast<size_t>(offset) > entries_.size()) {
                    error_msg = line.substr(i, end_d - i) + ": event not found";
                    return std::nullopt;
                }
                std::string event = entries_[entries_.size() - static_cast<size_t>(offset)];
                i = end_d;
                if (i < n && line[i] == ':' && i + 1 < n && (line[i + 1] == '^' || line[i + 1] == '$' || line[i + 1] == '*')) {
                    result += apply_word_selector(event, line[i + 1]);
                    i += 2;
                } else {
                    result += event;
                }
                continue;
            }

            // G) !N (absolute 1-based index)
            if (std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
                size_t start_d = i + 1;
                size_t end_d = start_d;
                while (end_d < n && std::isdigit(static_cast<unsigned char>(line[end_d]))) end_d++;
                int idx = std::stoi(line.substr(start_d, end_d - start_d));
                if (idx <= 0 || static_cast<size_t>(idx) > entries_.size()) {
                    error_msg = line.substr(i, end_d - i) + ": event not found";
                    return std::nullopt;
                }
                std::string event = entries_[static_cast<size_t>(idx - 1)];
                i = end_d;
                if (i < n && line[i] == ':' && i + 1 < n && (line[i + 1] == '^' || line[i + 1] == '$' || line[i + 1] == '*')) {
                    result += apply_word_selector(event, line[i + 1]);
                    i += 2;
                } else {
                    result += event;
                }
                continue;
            }

            // H) !?string[?]
            if (line[i + 1] == '?') {
                size_t start_s = i + 2;
                size_t end_s = start_s;
                while (end_s < n && line[end_s] != '?' && !std::isspace(static_cast<unsigned char>(line[end_s]))) end_s++;
                std::string query = line.substr(start_s, end_s - start_s);
                if (end_s < n && line[end_s] == '?') end_s++;
                bool found = false;
                std::string matched;
                for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
                    if (it->find(query) != std::string::npos) {
                        matched = *it;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error_msg = line.substr(i, end_s - i) + ": event not found";
                    return std::nullopt;
                }
                i = end_s;
                result += matched;
                continue;
            }

            // I) !prefix
            if (std::isalpha(static_cast<unsigned char>(line[i + 1])) || line[i + 1] == '_' || line[i + 1] == '.') {
                size_t start_p = i + 1;
                size_t end_p = start_p;
                while (end_p < n && !std::isspace(static_cast<unsigned char>(line[end_p])) &&
                       line[end_p] != ':' && line[end_p] != ';' && line[end_p] != '|' && line[end_p] != '&') {
                    end_p++;
                }
                std::string prefix = line.substr(start_p, end_p - start_p);
                bool found = false;
                std::string matched;
                for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
                    if (str_util::starts_with(*it, prefix)) {
                        matched = *it;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error_msg = line.substr(i, end_p - i) + ": event not found";
                    return std::nullopt;
                }
                i = end_p;
                if (i < n && line[i] == ':' && i + 1 < n && (line[i + 1] == '^' || line[i + 1] == '$' || line[i + 1] == '*')) {
                    result += apply_word_selector(matched, line[i + 1]);
                    i += 2;
                } else {
                    result += matched;
                }
                continue;
            }

            result += line[i++];
            continue;
        }

        result += line[i++];
    }

    return result;
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
