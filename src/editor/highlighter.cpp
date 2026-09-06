#include "aswell/editor/highlighter.hpp"
#include "aswell/shell/builtins.hpp"

namespace aswell {

SyntaxHighlighter::SyntaxHighlighter(Environment& env) : env_(env) {}

std::string SyntaxHighlighter::highlight(const std::string& line) const {
    if (line.empty()) return "";

    std::string out;
    size_t i = 0;
    bool at_cmd_start = true;

    while (i < line.size()) {
        char c = line[i];

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            out += c;
            i++;
            continue;
        }

        // Comment
        if (c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            out += "\033[90m"; // Dim gray
            while (i < line.size()) {
                out += line[i++];
            }
            out += "\033[0m";
            break;
        }

        // Single quoted string
        if (c == '\'') {
            out += "\033[33m'"; // Yellow
            i++;
            while (i < line.size()) {
                out += line[i];
                if (line[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            out += "\033[0m";
            at_cmd_start = false;
            continue;
        }

        // Double quoted string
        if (c == '\"') {
            out += "\033[33m\""; // Yellow
            i++;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    out += line[i++];
                    out += line[i++];
                    continue;
                }
                out += line[i];
                if (line[i] == '\"') {
                    i++;
                    break;
                }
                i++;
            }
            out += "\033[0m";
            at_cmd_start = false;
            continue;
        }

        // Variable expansion
        if (c == '$') {
            out += "\033[35m"; // Magenta / Purple
            out += c;
            i++;
            if (i < line.size() && line[i] == '{') {
                while (i < line.size()) {
                    out += line[i];
                    if (line[i] == '}') {
                        i++;
                        break;
                    }
                    i++;
                }
            } else {
                while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_' || line[i] == '?' || line[i] == '$' || line[i] == '!')) {
                    out += line[i++];
                }
            }
            out += "\033[0m";
            at_cmd_start = false;
            continue;
        }

        // Operators
        if (c == '|' || c == '&' || c == ';' || c == '<' || c == '>' || c == '(' || c == ')') {
            out += "\033[1;37m"; // Bold white
            out += c;
            if (i + 1 < line.size() && ((c == '|' && line[i + 1] == '|') ||
                                       (c == '&' && line[i + 1] == '&') ||
                                       (c == ';' && line[i + 1] == ';') ||
                                       (c == '<' && line[i + 1] == '<') ||
                                       (c == '>' && line[i + 1] == '>'))) {
                out += line[++i];
            }
            out += "\033[0m";
            if (c == '|' || c == '&' || c == ';' || c == '(') {
                at_cmd_start = true;
            }
            i++;
            continue;
        }

        // Flags / options: -a or --foo
        if (c == '-' && i + 1 < line.size() && !std::isspace(static_cast<unsigned char>(line[i + 1]))) {
            out += "\033[34m"; // Blue
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])) && line[i] != ';' && line[i] != '|' && line[i] != '&') {
                out += line[i++];
            }
            out += "\033[0m";
            at_cmd_start = false;
            continue;
        }

        // Read word
        size_t w_start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])) &&
               std::strchr("|&;()<>\'\"$", line[i]) == nullptr) {
            i++;
        }
        std::string word = line.substr(w_start, i - w_start);

        // Check if word contains '=' (assignment)
        size_t eq_pos = word.find('=');
        if (at_cmd_start && eq_pos != std::string::npos && eq_pos > 0) {
            out += "\033[36m" + word.substr(0, eq_pos) + "\033[0m=";
            out += "\033[32m" + word.substr(eq_pos + 1) + "\033[0m";
            continue;
        }

        if (at_cmd_start) {
            if (Builtins::is_builtin(word) || word == "if" || word == "then" || word == "else" ||
                word == "elif" || word == "fi" || word == "for" || word == "do" || word == "done" ||
                word == "while" || word == "until" || word == "case" || word == "esac" || word == "function") {
                out += "\033[1;36m" + word + "\033[0m"; // Cyan / bold builtin/keyword
            } else if (env_.has_function(word) || env_.get_aliases().find(word) != env_.get_aliases().end()) {
                out += "\033[1;34m" + word + "\033[0m"; // Blue function/alias
            } else {
                std::string path = env_.find_in_path(word);
                if (!path.empty()) {
                    out += "\033[32m" + word + "\033[0m"; // Green external executable
                } else {
                    out += "\033[4;31m" + word + "\033[0m"; // Red underline unknown
                }
            }
            at_cmd_start = false;
        } else {
            // Numbers
            bool is_num = !word.empty();
            for (char wc : word) {
                if (!std::isdigit(static_cast<unsigned char>(wc))) {
                    is_num = false;
                    break;
                }
            }
            if (is_num) {
                out += "\033[35m" + word + "\033[0m"; // Magenta number
            } else {
                out += word;
            }
        }
    }

    return out;
}

} // namespace aswell
