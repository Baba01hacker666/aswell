#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <memory>
#include <optional>
#include <unordered_map>
#include <map>
#include <set>
#include <functional>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

namespace aswell {

inline constexpr const char* SHELL_NAME = "aswell";
inline constexpr const char* SHELL_VERSION = "1.0.0";
inline constexpr const char* SHELL_BANNER = "Aswell 1.0.0 — Modern Customizable POSIX Shell";

namespace str_util {

inline std::string trim_left(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    return std::string(s.substr(start));
}

inline std::string trim_right(std::string_view s) {
    if (s.empty()) return "";
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return std::string(s.substr(0, end));
}

inline std::string_view trim_sv(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

inline std::string trim(std::string_view s) {
    return trim_left(trim_right(s));
}

inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

inline std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            result.emplace_back(s.substr(start));
            break;
        }
        result.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return result;
}

inline std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return res;
}

inline int levenshtein_distance(std::string_view s1, std::string_view s2) {
    size_t m = s1.size();
    size_t n = s2.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    return dp[m][n];
}

inline size_t visual_width(std::string_view s) {
    size_t width = 0;
    bool in_ansi = false;
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            in_ansi = true;
            i += 2;
            continue;
        }
        if (in_ansi) {
            if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') || s[i] == '~') {
                in_ansi = false;
            }
            i++;
            continue;
        }

        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            width += (c >= 32 && c != 127) ? 1 : 0;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            width += 1; // 2-byte UTF-8
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8 (box drawing, nerd fonts, symbols)
            width += 1;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            width += 2; // 4-byte UTF-8 (emojis etc)
            i += 4;
        } else {
            i += 1;
        }
    }
    return width;
}

inline std::string strip_ansi(std::string_view s) {
    std::string res;
    bool in_ansi = false;
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            in_ansi = true;
            i += 2;
            continue;
        }
        if (in_ansi) {
            if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') || s[i] == '~') {
                in_ansi = false;
            }
            i++;
            continue;
        }
        res += s[i];
        i++;
    }
    return res;
}

inline std::string escape_shell(std::string_view s) {
    if (s.empty()) return "''";
    bool needs_quotes = false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.' && c != '/' && c != ':') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return std::string(s);

    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

} // namespace str_util

} // namespace aswell
