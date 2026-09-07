#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/ui/dom.hpp"
#include "aswell/ui/css_parser.hpp"
#include "aswell/ui/layout.hpp"
#include "aswell/ui/render.hpp"

namespace aswell {

struct PromptContext {
    std::string user;
    std::string hostname;
    std::string cwd;
    std::string git_branch;
    bool git_dirty = false;
    int last_status = 0;
    double last_duration_ms = 0.0;
    size_t active_jobs = 0;
    bool vi_normal_mode = false;
    std::string time_str;
};

class PromptEngine {
public:
    explicit PromptEngine(Environment& env);

    void set_theme_css(std::string_view css);
    void set_template_html(std::string_view html);
    void set_custom_user(const std::string& user) { custom_user_ = user; }
    void set_custom_hostname(const std::string& host) { custom_hostname_ = host; }
    const std::string& custom_user() const { return custom_user_; }
    const std::string& custom_hostname() const { return custom_hostname_; }

    RenderResult render(const PromptContext& ctx, uint64_t timestamp_ms);
    PromptContext gather_context(double last_duration_ms = 0.0, size_t active_jobs = 0, bool vi_normal = false);

    bool has_active_animations() const { return has_animations_; }
    const StyleSheet& stylesheet() const { return stylesheet_; }

private:
    std::string get_git_info(bool& out_dirty);
    std::string get_shortened_path(const std::string& path);

    Environment& env_;
    StyleSheet stylesheet_;
    std::string template_html_;
    std::string custom_user_;
    std::string custom_hostname_;
    bool has_animations_ = false;
};

} // namespace aswell
