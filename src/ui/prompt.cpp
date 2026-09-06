#include "aswell/ui/prompt.hpp"
#include "aswell/ui/terminal.hpp"
#include <ctime>
#include <iomanip>
#include <fstream>

namespace aswell {

PromptEngine::PromptEngine(Environment& env) : env_(env) {
    // Default elegant template
    template_html_ = R"(
<prompt class="main">
  <segment class="top-line">
    <text class="bracket">╭─</text>
    <user />
    <text class="at">@</text>
    <hostname />
    <text class="sep"> in </text>
    <directory />
    <git />
    <runtime />
    <status />
    <jobs />
  </segment>
  <newline />
  <segment class="bottom-line">
    <text class="bracket">╰─</text>
    <mode />
    <symbol />
    <text> </text>
  </segment>
</prompt>
)";

    // Default clean theme CSS
    set_theme_css(R"(
prompt.main {
    display: block;
}
.bracket {
    color: #6272a4;
}
user {
    color: #8be9fd;
    font-weight: bold;
}
.at {
    color: #6272a4;
}
hostname {
    color: #bd93f9;
}
.sep {
    color: #6272a4;
}
directory {
    color: #50fa7b;
    font-weight: bold;
}
git {
    color: #f1fa8c;
    margin-left: 1;
}
git.dirty {
    color: #ffb86c;
}
runtime {
    color: #f1fa8c;
    margin-left: 1;
}
status.error {
    color: #ff5555;
    margin-left: 1;
    font-weight: bold;
}
status.success {
    color: #50fa7b;
}
jobs {
    color: #ff79c6;
    margin-left: 1;
}
mode.normal {
    color: #ffb86c;
    font-weight: bold;
    margin-right: 1;
}
mode.insert {
    color: #8be9fd;
    margin-right: 1;
}
symbol {
    color: #ff79c6;
    animation: pulse 1500ms infinite;
}
symbol.error {
    color: #ff5555;
}
)");
}

void PromptEngine::set_theme_css(std::string_view css) {
    stylesheet_ = CSSParser::parse(css);
    has_animations_ = false;
    for (const auto& rule : stylesheet_.rules()) {
        if (rule.style.animation.type != AnimationType::NONE) {
            has_animations_ = true;
            break;
        }
    }
}

void PromptEngine::set_template_html(std::string_view html) {
    template_html_ = std::string(html);
}

std::string PromptEngine::get_git_info(bool& out_dirty) {
    out_dirty = false;
    std::string cur_pwd = env_.get_var("PWD");
    if (cur_pwd.empty()) cur_pwd = ".";

    std::string check_dir = cur_pwd;
    std::string git_dir;

    for (int depth = 0; depth < 8; ++depth) {
        std::string candidate = check_dir + "/.git";
        struct stat st;
        if (stat(candidate.c_str(), &st) == 0) {
            git_dir = candidate;
            break;
        }
        size_t slash = check_dir.find_last_of('/');
        if (slash == std::string::npos || slash == 0) break;
        check_dir = check_dir.substr(0, slash);
    }

    if (git_dir.empty()) return "";

    // Read HEAD
    std::ifstream head_file(git_dir + "/HEAD");
    if (!head_file) return "";

    std::string head_line;
    std::getline(head_file, head_line);
    head_line = str_util::trim(head_line);

    std::string branch;
    if (str_util::starts_with(head_line, "ref: refs/heads/")) {
        branch = head_line.substr(16);
    } else if (head_line.size() >= 7) {
        branch = head_line.substr(0, 7); // Detached HEAD SHA
    } else {
        branch = "git";
    }

    return branch;
}

std::string PromptEngine::get_shortened_path(const std::string& path) {
    std::string home = env_.get_var("HOME");
    std::string res = path;

    if (!home.empty() && str_util::starts_with(res, home)) {
        res = "~" + res.substr(home.size());
    }

    return res;
}

PromptContext PromptEngine::gather_context(double last_duration_ms, size_t active_jobs, bool vi_normal) {
    PromptContext ctx;

    const char* user = std::getenv("USER");
    ctx.user = user ? user : "user";

    char host[256] = {0};
    if (gethostname(host, sizeof(host)) == 0) {
        ctx.hostname = host;
        size_t dot = ctx.hostname.find('.');
        if (dot != std::string::npos) ctx.hostname = ctx.hostname.substr(0, dot);
    } else {
        ctx.hostname = "localhost";
    }

    ctx.cwd = get_shortened_path(env_.get_var("PWD"));
    ctx.git_branch = get_git_info(ctx.git_dirty);
    ctx.last_status = env_.last_exit_status;
    ctx.last_duration_ms = last_duration_ms;
    ctx.active_jobs = active_jobs;
    ctx.vi_normal_mode = vi_normal;

    // Timestamp
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char tbuf[64];
    std::strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);
    ctx.time_str = tbuf;

    return ctx;
}

static void populate_dom_data(std::shared_ptr<UIElement> elem, const PromptContext& ctx) {
    if (!elem) return;

    if (elem->tag == "user") {
        elem->text_content = ctx.user;
    } else if (elem->tag == "hostname") {
        elem->text_content = ctx.hostname;
    } else if (elem->tag == "directory") {
        elem->text_content = ctx.cwd;
    } else if (elem->tag == "git") {
        if (!ctx.git_branch.empty()) {
            elem->text_content = "git:(" + ctx.git_branch + (ctx.git_dirty ? "*" : "") + ")";
            elem->classes.insert(ctx.git_dirty ? "dirty" : "clean");
            elem->pseudos.insert(ctx.git_dirty ? "dirty" : "clean");
        } else {
            elem->computed_style.display = DisplayType::NONE;
        }
    } else if (elem->tag == "status") {
        if (ctx.last_status != 0) {
            elem->text_content = "[" + std::to_string(ctx.last_status) + "]";
            elem->classes.insert("error");
            elem->pseudos.insert("error");
        } else {
            elem->computed_style.display = DisplayType::NONE;
        }
    } else if (elem->tag == "runtime") {
        if (ctx.last_duration_ms >= 50.0) {
            if (ctx.last_duration_ms >= 1000.0) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << (ctx.last_duration_ms / 1000.0) << "s";
                elem->text_content = oss.str();
            } else {
                elem->text_content = std::to_string(static_cast<int>(ctx.last_duration_ms)) + "ms";
            }
        } else {
            elem->computed_style.display = DisplayType::NONE;
        }
    } else if (elem->tag == "jobs") {
        if (ctx.active_jobs > 0) {
            elem->text_content = "{" + std::to_string(ctx.active_jobs) + "}";
            elem->classes.insert("active");
        } else {
            elem->computed_style.display = DisplayType::NONE;
        }
    } else if (elem->tag == "mode") {
        if (ctx.vi_normal_mode) {
            elem->text_content = "[NORMAL]";
            elem->classes.insert("normal");
        } else {
            elem->text_content = "";
        }
    } else if (elem->tag == "symbol") {
        if (elem->text_content.empty()) {
            elem->text_content = "❯";
        }
        if (ctx.last_status != 0) {
            elem->classes.insert("error");
            elem->pseudos.insert("error");
        }
    } else if (elem->tag == "time") {
        elem->text_content = ctx.time_str;
    }

    for (auto& child : elem->children) {
        populate_dom_data(child, ctx);
    }
}

RenderResult PromptEngine::render(const PromptContext& ctx, uint64_t timestamp_ms) {
    auto dom = DOMParser::parse(template_html_);
    if (!dom) return RenderResult();

    populate_dom_data(dom, ctx);
    dom->apply_styles(stylesheet_);

    auto layout = LayoutEngine::compute_layout(dom, 80, timestamp_ms);
    bool truecolor = Terminal::supports_truecolor();
    bool unicode = Terminal::supports_unicode();

    return TerminalRenderer::render(layout, timestamp_ms, truecolor, unicode);
}

} // namespace aswell
