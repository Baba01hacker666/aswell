#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/ui/terminal.hpp"
#include "aswell/editor/history.hpp"
#include "aswell/editor/highlighter.hpp"
#include "aswell/editor/suggestions.hpp"
#include "aswell/editor/completion.hpp"

namespace aswell {

class LineEditor {
public:
    LineEditor(Environment& env, PromptEngine& prompt_engine);

    std::optional<std::string> read_line(double last_duration_ms = 0.0, size_t active_jobs = 0);

    History& history() { return history_; }
    void set_vi_mode(bool vi) { vi_mode_ = vi; }
    void set_command_animation(bool enable) { command_anim_enabled_ = enable; }
    bool is_command_animation_enabled() const { return command_anim_enabled_; }

private:
    void refresh_line(const std::string& prompt_ansi, int prompt_visual_width, uint64_t timestamp_ms);
    bool handle_reverse_search();
    void show_completion_menu(const std::vector<CompletionCandidate>& candidates,
                              const std::string& current_word,
                              size_t token_start);

    bool is_command_complete(const std::string& text) const;
    std::string render_animated_command(const std::string& buffer, uint64_t timestamp_ms) const;

    Environment& env_;
    PromptEngine& prompt_engine_;
    History history_;
    SyntaxHighlighter highlighter_;
    AutoSuggestions suggestions_;
    CompletionEngine completion_;

    std::string buffer_;
    size_t cursor_pos_ = 0;
    std::string kill_ring_;

    bool vi_mode_ = false;
    bool vi_insert_mode_ = true;

    int history_index_ = -1;
    std::string saved_current_buffer_;
    bool command_anim_enabled_ = false;
    std::string rprompt_ansi_;
    int rprompt_width_ = 0;
};

} // namespace aswell
