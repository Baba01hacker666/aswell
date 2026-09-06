#include "aswell/editor/editor.hpp"
#include "aswell/ui/animation.hpp"

namespace aswell {

LineEditor::LineEditor(Environment& env, PromptEngine& prompt_engine)
    : env_(env),
      prompt_engine_(prompt_engine),
      history_(),
      highlighter_(env),
      suggestions_(history_),
      completion_(env) {
    vi_mode_ = env_.opt_vi_mode;
}

bool LineEditor::is_command_complete(const std::string& text) const {
    bool in_single = false;
    bool in_double = false;
    int paren_depth = 0;
    int brace_depth = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\\' && !in_single) {
            i++;
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '\"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double) {
            if (c == '(') paren_depth++;
            else if (c == ')') paren_depth--;
            else if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
        }
    }

    if (in_single || in_double || paren_depth > 0 || brace_depth > 0) {
        return false;
    }

    // Check unclosed keywords like then, do
    auto tokens = str_util::split(text, ' ');
    int do_count = 0, done_count = 0;
    int if_count = 0, fi_count = 0;
    int case_count = 0, esac_count = 0;

    for (const auto& raw_t : tokens) {
        std::string t = str_util::trim(raw_t);
        if (t == "do") do_count++;
        else if (t == "done") done_count++;
        else if (t == "if") if_count++;
        else if (t == "fi") fi_count++;
        else if (t == "case") case_count++;
        else if (t == "esac") esac_count++;
    }

    if (do_count > done_count || if_count > fi_count || case_count > esac_count) {
        return false;
    }

    return true;
}

void LineEditor::refresh_line(const std::string& prompt_ansi, int prompt_visual_width, uint64_t /*timestamp_ms*/) {
    // Clear current line only
    std::cout << "\r\033[K";
    std::cout << prompt_ansi;

    // Syntax highlighted buffer
    std::string hl = highlighter_.highlight(buffer_);
    std::cout << hl;

    // Autosuggestion in muted gray if at the end of the buffer
    std::string sugg_suffix = suggestions_.get_suggestion_suffix(buffer_);
    if (!sugg_suffix.empty() && cursor_pos_ == buffer_.size()) {
        std::cout << "\033[90m" << sugg_suffix << "\033[0m";
    }

    // Calculate cursor position
    size_t cursor_visual_offset = str_util::visual_width(buffer_.substr(0, cursor_pos_));
    int target_col = prompt_visual_width + static_cast<int>(cursor_visual_offset) + 1;

    // Reposition cursor
    std::cout << "\r";
    Terminal::cursor_right(target_col - 1);
    std::cout.flush();
}

bool LineEditor::handle_reverse_search() {
    std::string query;
    std::string matched_cmd;

    while (true) {
        std::cout << "\r\033[K(reverse-i-search)`" << query << "': \033[1;36m" << matched_cmd << "\033[0m";
        std::cout.flush();

        KeyEvent ev = Terminal::read_key();
        if (ev.key == Key::ESC || ev.key == Key::CTRL_C) {
            return false;
        } else if (ev.key == Key::ENTER) {
            if (!matched_cmd.empty()) {
                buffer_ = matched_cmd;
                cursor_pos_ = buffer_.size();
            }
            return true;
        } else if (ev.key == Key::BACKSPACE) {
            if (!query.empty()) query.pop_back();
        } else if (ev.key == Key::CHAR) {
            query += ev.ch;
        } else if (ev.key == Key::CTRL_R) {
            // cycle next
        } else {
            break;
        }

        auto matches = history_.search(query);
        if (!matches.empty()) {
            matched_cmd = matches[0];
        } else {
            matched_cmd = "";
        }
    }
    return false;
}

void LineEditor::show_completion_menu(const std::vector<CompletionCandidate>& candidates,
                                      const std::string& current_word,
                                      size_t token_start) {
    if (candidates.empty()) return;

    if (candidates.size() == 1) {
        std::string replacement = candidates[0].text;
        buffer_.replace(token_start, current_word.size(), replacement);
        cursor_pos_ = token_start + replacement.size();
        return;
    }

    std::string common = candidates[0].text;
    for (size_t i = 1; i < candidates.size(); ++i) {
        size_t match_len = 0;
        while (match_len < common.size() && match_len < candidates[i].text.size() &&
               common[match_len] == candidates[i].text[match_len]) {
            match_len++;
        }
        common = common.substr(0, match_len);
    }

    if (common.size() > current_word.size()) {
        buffer_.replace(token_start, current_word.size(), common);
        cursor_pos_ = token_start + common.size();
        return;
    }

    std::cout << "\n";
    size_t max_disp = std::min<size_t>(candidates.size(), 12);
    for (size_t i = 0; i < max_disp; ++i) {
        std::cout << "  \033[36m" << candidates[i].display_name << "\033[0m";
        if (!candidates[i].description.empty()) {
            std::cout << " \033[90m(" << candidates[i].description << ")\033[0m";
        }
        std::cout << "\n";
    }
    if (candidates.size() > 12) {
        std::cout << "  \033[90m... and " << (candidates.size() - 12) << " more\033[0m\n";
    }
}

std::optional<std::string> LineEditor::read_line(double last_duration_ms, size_t active_jobs) {
    RawModeGuard raw_guard;

    buffer_.clear();
    cursor_pos_ = 0;
    history_index_ = -1;
    saved_current_buffer_.clear();
    vi_insert_mode_ = true;
    vi_mode_ = env_.opt_vi_mode;

    bool has_anim = prompt_engine_.has_active_animations();
    int timeout_ms = has_anim ? 120 : -1;

    // Render initial prompt
    auto initial_ctx = prompt_engine_.gather_context(last_duration_ms, active_jobs, false);
    RenderResult initial_pr = prompt_engine_.render(initial_ctx, AnimationEngine::now_ms());

    std::string header_lines;
    std::string input_prompt = initial_pr.ansi_output;
    int input_prompt_width = initial_pr.last_line_width;

    size_t last_nl = initial_pr.ansi_output.find_last_of('\n');
    if (last_nl != std::string::npos) {
        header_lines = initial_pr.ansi_output.substr(0, last_nl + 1);
        input_prompt = initial_pr.ansi_output.substr(last_nl + 1);
    }

    // Print multi-line header ONCE
    if (!header_lines.empty()) {
        std::cout << header_lines;
        std::cout.flush();
    }

    std::string full_accumulated_input;

    while (true) {
        uint64_t now = AnimationEngine::now_ms();

        // If animated, re-evaluate the input prompt line symbol
        if (has_anim) {
            auto cur_ctx = prompt_engine_.gather_context(last_duration_ms, active_jobs, vi_mode_ && !vi_insert_mode_);
            RenderResult cur_pr = prompt_engine_.render(cur_ctx, now);
            size_t nl = cur_pr.ansi_output.find_last_of('\n');
            if (nl != std::string::npos) {
                input_prompt = cur_pr.ansi_output.substr(nl + 1);
            } else {
                input_prompt = cur_pr.ansi_output;
            }
            input_prompt_width = cur_pr.last_line_width;
        }

        refresh_line(input_prompt, input_prompt_width, now);

        KeyEvent ev = Terminal::read_key(timeout_ms);

        if (ev.key == Key::NONE) {
            // Animation tick - refresh line without scrolling
            continue;
        }

        // Vi mode state machine
        if (vi_mode_ && !vi_insert_mode_) {
            // Vi Normal Mode
            if (ev.key == Key::CHAR) {
                char c = ev.ch.empty() ? 0 : ev.ch[0];
                if (c == 'i') {
                    vi_insert_mode_ = true;
                } else if (c == 'a') {
                    vi_insert_mode_ = true;
                    if (cursor_pos_ < buffer_.size()) cursor_pos_++;
                } else if (c == 'h') {
                    if (cursor_pos_ > 0) cursor_pos_--;
                } else if (c == 'l') {
                    if (cursor_pos_ < buffer_.size()) cursor_pos_++;
                } else if (c == '0') {
                    cursor_pos_ = 0;
                } else if (c == '$') {
                    cursor_pos_ = buffer_.size();
                } else if (c == 'x') {
                    if (cursor_pos_ < buffer_.size()) {
                        buffer_.erase(cursor_pos_, 1);
                    }
                } else if (c == 'w') {
                    size_t next_space = buffer_.find(' ', cursor_pos_);
                    if (next_space != std::string::npos) {
                        while (next_space < buffer_.size() && buffer_[next_space] == ' ') next_space++;
                        cursor_pos_ = next_space;
                    } else {
                        cursor_pos_ = buffer_.size();
                    }
                } else if (c == 'b') {
                    if (cursor_pos_ > 0) {
                        size_t prev = cursor_pos_ - 1;
                        while (prev > 0 && buffer_[prev] == ' ') prev--;
                        while (prev > 0 && buffer_[prev - 1] != ' ') prev--;
                        cursor_pos_ = prev;
                    }
                }
            } else if (ev.key == Key::ENTER) {
                std::cout << "\r\n";
                std::cout.flush();
                if (!buffer_.empty()) history_.add(buffer_);
                return buffer_;
            }
            continue;
        }

        // Insert mode (Emacs / standard)
        switch (ev.key) {
            case Key::CHAR:
                buffer_.insert(cursor_pos_, ev.ch);
                cursor_pos_ += ev.ch.size();
                break;

            case Key::BACKSPACE:
                if (cursor_pos_ > 0) {
                    buffer_.erase(cursor_pos_ - 1, 1);
                    cursor_pos_--;
                }
                break;

            case Key::DELETE:
                if (cursor_pos_ < buffer_.size()) {
                    buffer_.erase(cursor_pos_, 1);
                }
                break;

            case Key::LEFT:
                if (cursor_pos_ > 0) cursor_pos_--;
                break;

            case Key::RIGHT: {
                std::string sugg = suggestions_.get_suggestion_suffix(buffer_);
                if (cursor_pos_ == buffer_.size() && !sugg.empty()) {
                    // Accept autosuggestion!
                    buffer_ += sugg;
                    cursor_pos_ = buffer_.size();
                } else if (cursor_pos_ < buffer_.size()) {
                    cursor_pos_++;
                }
                break;
            }

            case Key::HOME:
            case Key::CTRL_A:
                cursor_pos_ = 0;
                break;

            case Key::END:
            case Key::CTRL_E: {
                std::string sugg = suggestions_.get_suggestion_suffix(buffer_);
                if (cursor_pos_ == buffer_.size() && !sugg.empty()) {
                    buffer_ += sugg;
                }
                cursor_pos_ = buffer_.size();
                break;
            }

            case Key::UP: {
                if (history_.size() == 0) break;
                if (history_index_ == -1) {
                    saved_current_buffer_ = buffer_;
                    history_index_ = static_cast<int>(history_.size()) - 1;
                } else if (history_index_ > 0) {
                    history_index_--;
                }
                buffer_ = history_.get(history_index_);
                cursor_pos_ = buffer_.size();
                break;
            }

            case Key::DOWN: {
                if (history_index_ != -1) {
                    if (static_cast<size_t>(history_index_) + 1 < history_.size()) {
                        history_index_++;
                        buffer_ = history_.get(history_index_);
                    } else {
                        history_index_ = -1;
                        buffer_ = saved_current_buffer_;
                    }
                    cursor_pos_ = buffer_.size();
                }
                break;
            }

            case Key::CTRL_K:
                kill_ring_ = buffer_.substr(cursor_pos_);
                buffer_ = buffer_.substr(0, cursor_pos_);
                break;

            case Key::CTRL_U:
                kill_ring_ = buffer_.substr(0, cursor_pos_);
                buffer_ = buffer_.substr(cursor_pos_);
                cursor_pos_ = 0;
                break;

            case Key::CTRL_W: {
                if (cursor_pos_ > 0) {
                    size_t start = cursor_pos_ - 1;
                    while (start > 0 && std::isspace(static_cast<unsigned char>(buffer_[start]))) start--;
                    while (start > 0 && !std::isspace(static_cast<unsigned char>(buffer_[start - 1]))) start--;
                    kill_ring_ = buffer_.substr(start, cursor_pos_ - start);
                    buffer_.erase(start, cursor_pos_ - start);
                    cursor_pos_ = start;
                }
                break;
            }

            case Key::CTRL_Y:
                buffer_.insert(cursor_pos_, kill_ring_);
                cursor_pos_ += kill_ring_.size();
                break;

            case Key::CTRL_L:
                Terminal::clear_screen();
                break;

            case Key::CTRL_C:
                std::cout << "^C\r\n";
                std::cout.flush();
                buffer_.clear();
                cursor_pos_ = 0;
                return "";

            case Key::CTRL_D:
                if (buffer_.empty()) {
                    std::cout << "exit\r\n";
                    std::cout.flush();
                    return std::nullopt; // EOF
                } else if (cursor_pos_ < buffer_.size()) {
                    buffer_.erase(cursor_pos_, 1);
                }
                break;

            case Key::CTRL_R:
                if (handle_reverse_search()) {
                    // Accepted search result
                }
                break;

            case Key::TAB: {
                // Find token before cursor
                size_t token_start = buffer_.substr(0, cursor_pos_).find_last_of(" \t\n;&|");
                token_start = (token_start == std::string::npos) ? 0 : token_start + 1;
                std::string current_word = buffer_.substr(token_start, cursor_pos_ - token_start);

                auto candidates = completion_.complete(buffer_, cursor_pos_);
                show_completion_menu(candidates, current_word, token_start);
                break;
            }

            case Key::ALT_F: {
                // Accept next word from autosuggestion or move word forward
                std::string sugg = suggestions_.get_suggestion_suffix(buffer_);
                if (!sugg.empty() && cursor_pos_ == buffer_.size()) {
                    size_t space = sugg.find(' ');
                    std::string word = (space == std::string::npos) ? sugg : sugg.substr(0, space + 1);
                    buffer_ += word;
                    cursor_pos_ = buffer_.size();
                } else {
                    size_t next_space = buffer_.find(' ', cursor_pos_);
                    if (next_space != std::string::npos) {
                        while (next_space < buffer_.size() && buffer_[next_space] == ' ') next_space++;
                        cursor_pos_ = next_space;
                    } else {
                        cursor_pos_ = buffer_.size();
                    }
                }
                break;
            }

            case Key::ALT_B: {
                if (cursor_pos_ > 0) {
                    size_t prev = cursor_pos_ - 1;
                    while (prev > 0 && buffer_[prev] == ' ') prev--;
                    while (prev > 0 && buffer_[prev - 1] != ' ') prev--;
                    cursor_pos_ = prev;
                }
                break;
            }

            case Key::ESC:
                if (vi_mode_) {
                    vi_insert_mode_ = false;
                    if (cursor_pos_ > 0) cursor_pos_--;
                }
                break;

            case Key::ALT_ENTER:
                buffer_.insert(cursor_pos_, "\n");
                cursor_pos_++;
                break;

            case Key::ENTER:
                if (!full_accumulated_input.empty()) {
                    full_accumulated_input += "\n" + buffer_;
                } else {
                    full_accumulated_input = buffer_;
                }

                if (!is_command_complete(full_accumulated_input)) {
                    // Continue multiline reading
                    std::cout << "\r\n";
                    std::cout.flush();
                    buffer_.clear();
                    cursor_pos_ = 0;
                    prompt_engine_.set_template_html("<prompt><text class=\"bracket\">>  </text></prompt>");
                    continue;
                }

                std::cout << "\r\n";
                std::cout.flush();
                if (!full_accumulated_input.empty()) {
                    history_.add(full_accumulated_input);
                }
                return full_accumulated_input;

            default:
                break;
        }
    }
}

} // namespace aswell
