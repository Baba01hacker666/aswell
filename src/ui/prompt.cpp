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
    theme_css_ = std::string(css);
    apply_theme_css();
}

// Re-parses the stored stylesheet, honouring the `animation` config switch:
// when animations are disabled every rule is imported with its animation
// stripped, so no per-frame redraw of the prompt is ever triggered.
void PromptEngine::apply_theme_css() {
    StyleSheet parsed = CSSParser::parse(theme_css_);
    if (animations_enabled_) {
        stylesheet_ = std::move(parsed);
    } else {
        StyleSheet filtered;
        for (auto rule : parsed.rules()) {
            rule.style.animation.type = AnimationType::NONE;
            rule.style.animation.duration_ms = 0;
            filtered.add_rule(rule);
        }
        stylesheet_ = std::move(filtered);
    }

    has_animations_ = false;
    for (const auto& rule : stylesheet_.rules()) {
        if (rule.style.animation.type != AnimationType::NONE) {
            has_animations_ = true;
            break;
        }
    }
}

void PromptEngine::set_animations_enabled(bool enabled) {
    if (animations_enabled_ == enabled) return;
    animations_enabled_ = enabled;
    apply_theme_css();
}

const std::vector<std::string>& PromptEngine::known_tags() {
    static const std::vector<std::string> kTags = {
        "prompt", "rprompt", "statusbar", "segment", "text", "newline", "br",
        "user", "hostname", "directory", "cwd", "git", "runtime", "status",
        "jobs", "mode", "symbol", "time", "date",
    };
    return kTags;
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

// Formats a broken-down time with a small, explicit subset of strftime
// specifiers. Implemented by hand instead of calling strftime(): the format
// comes from user config, so a bounded, warning-free and locale-stable
// formatter is the safer primitive here. Unknown sequences pass through
// verbatim, and an empty format falls back to the documented default.
static std::string pad2(int value) {
    std::string out = std::to_string(value);
    if (out.size() < 2) out = std::string(2 - out.size(), '0') + out;
    return out;
}

static std::string format_time(const std::tm* tm_info, const std::string& fmt_in, const char* fallback) {
    static const char* kDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char* kMonths[] = {"January", "February", "March", "April", "May", "June", "July",
                                    "August", "September", "October", "November", "December"};
    if (!tm_info) return "";

    std::string fmt = fmt_in.empty() ? std::string(fallback) : fmt_in;
    std::string out;
    out.reserve(fmt.size() + 8);

    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 >= fmt.size()) {
            out += fmt[i];
            continue;
        }
        const char spec = fmt[++i];
        switch (spec) {
            case 'H': out += pad2(tm_info->tm_hour); break;
            case 'M': out += pad2(tm_info->tm_min); break;
            case 'S': out += pad2(tm_info->tm_sec); break;
            case 'I': {
                int hour12 = tm_info->tm_hour % 12;
                out += pad2(hour12 == 0 ? 12 : hour12);
                break;
            }
            case 'p': out += (tm_info->tm_hour < 12) ? "AM" : "PM"; break;
            case 'd': out += pad2(tm_info->tm_mday); break;
            case 'e': out += (tm_info->tm_mday < 10 ? " " : "") + std::to_string(tm_info->tm_mday); break;
            case 'm': out += pad2(tm_info->tm_mon + 1); break;
            case 'Y': out += std::to_string(tm_info->tm_year + 1900); break;
            case 'y': out += pad2((tm_info->tm_year + 1900) % 100); break;
            case 'j': {
                int doy = tm_info->tm_yday + 1;
                std::string digits = std::to_string(doy);
                out += std::string(3 - digits.size(), '0') + digits;
                break;
            }
            case 'a': out += std::string(kDays[tm_info->tm_wday % 7]).substr(0, 3); break;
            case 'A': out += kDays[tm_info->tm_wday % 7]; break;
            case 'b': out += std::string(kMonths[tm_info->tm_mon % 12]).substr(0, 3); break;
            case 'B': out += kMonths[tm_info->tm_mon % 12]; break;
            case '%': out += '%'; break;
            default:
                out += '%';
                out += spec;
                break;
        }
    }
    return out;
}

PromptContext PromptEngine::gather_context(double last_duration_ms, size_t active_jobs, bool vi_normal) {
    PromptContext ctx;

    // 1. Username resolution
    if (!custom_user_.empty()) {
        ctx.user = custom_user_;
    } else if (env_.has_var("ASWELL_USER") && !env_.get_var("ASWELL_USER").empty()) {
        ctx.user = env_.get_var("ASWELL_USER");
    } else if (env_.has_var("ASWELL_USERNAME") && !env_.get_var("ASWELL_USERNAME").empty()) {
        ctx.user = env_.get_var("ASWELL_USERNAME");
    } else {
        const char* user = std::getenv("USER");
        ctx.user = user ? user : "user";
    }

    // 2. Hostname resolution
    if (!custom_hostname_.empty()) {
        ctx.hostname = custom_hostname_;
    } else if (env_.has_var("ASWELL_HOSTNAME") && !env_.get_var("ASWELL_HOSTNAME").empty()) {
        ctx.hostname = env_.get_var("ASWELL_HOSTNAME");
    } else if (env_.has_var("ASWELL_HOST") && !env_.get_var("ASWELL_HOST").empty()) {
        ctx.hostname = env_.get_var("ASWELL_HOST");
    } else {
        char host[256] = {0};
        if (gethostname(host, sizeof(host)) == 0) {
            ctx.hostname = host;
            size_t dot = ctx.hostname.find('.');
            if (dot != std::string::npos) ctx.hostname = ctx.hostname.substr(0, dot);
        } else {
            ctx.hostname = "localhost";
        }
    }

    ctx.cwd = get_shortened_path(env_.get_var("PWD"));
    ctx.git_branch = get_git_info(ctx.git_dirty);
    ctx.last_status = env_.last_exit_status;
    ctx.last_duration_ms = last_duration_ms;
    ctx.active_jobs = active_jobs;
    ctx.vi_normal_mode = vi_normal;

    // Timestamp & date, honouring the configured strftime formats
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    ctx.time_str = format_time(tm_info, time_format_, "%H:%M:%S");
    ctx.date_str = format_time(tm_info, date_format_, "%Y-%m-%d");

    // Export standard environment variables for dynamic DOM binding
    env_.set_var("USER", ctx.user);
    env_.set_var("HOSTNAME", ctx.hostname);
    env_.set_var("CWD", ctx.cwd);
    env_.set_var("GIT_BRANCH", ctx.git_branch);
    env_.set_var("STATUS", std::to_string(ctx.last_status));
    env_.set_var("JOBS", std::to_string(ctx.active_jobs));
    env_.set_var("MODE", ctx.vi_normal_mode ? "NORMAL" : "INSERT");
    env_.set_var("TIME", ctx.time_str);
    env_.set_var("DATE", ctx.date_str);

    return ctx;
}

static std::string expand_vars(const std::string& input, const Environment& env) {
    if (input.empty() || input.find('$') == std::string::npos) {
        return input;
    }
    std::string result;
    result.reserve(input.size() * 2);
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '$') {
            if (i + 1 < input.size() && input[i + 1] == '{') {
                size_t close = input.find('}', i + 2);
                if (close != std::string::npos) {
                    std::string var_name = input.substr(i + 2, close - (i + 2));
                    result += env.get_var(var_name);
                    i = close;
                    continue;
                }
            } else if (i + 1 < input.size() && (std::isalnum(static_cast<unsigned char>(input[i + 1])) || input[i + 1] == '_')) {
                size_t start = i + 1;
                size_t len = 0;
                while (start + len < input.size() && (std::isalnum(static_cast<unsigned char>(input[start + len])) || input[start + len] == '_')) {
                    len++;
                }
                std::string var_name = input.substr(start, len);
                result += env.get_var(var_name);
                i = start + len - 1;
                continue;
            }
        }
        result += input[i];
    }
    return result;
}

static bool evaluate_condition(const std::string& cond, const Environment& env) {
    std::string expanded = expand_vars(str_util::trim(cond), env);
    if (expanded.empty()) return false;

    // Check !=
    size_t neq = expanded.find("!=");
    if (neq != std::string::npos) {
        std::string lhs = str_util::trim(expanded.substr(0, neq));
        std::string rhs = str_util::trim(expanded.substr(neq + 2));
        if (rhs.size() >= 2 && ((rhs.front() == '"' && rhs.back() == '"') || (rhs.front() == '\'' && rhs.back() == '\''))) {
            rhs = rhs.substr(1, rhs.size() - 2);
        }
        if (lhs.size() >= 2 && ((lhs.front() == '"' && lhs.back() == '"') || (lhs.front() == '\'' && lhs.back() == '\''))) {
            lhs = lhs.substr(1, lhs.size() - 2);
        }
        return lhs != rhs;
    }

    // Check ==
    size_t eq = expanded.find("==");
    if (eq != std::string::npos) {
        std::string lhs = str_util::trim(expanded.substr(0, eq));
        std::string rhs = str_util::trim(expanded.substr(eq + 2));
        if (rhs.size() >= 2 && ((rhs.front() == '"' && rhs.back() == '"') || (rhs.front() == '\'' && rhs.back() == '\''))) {
            rhs = rhs.substr(1, rhs.size() - 2);
        }
        if (lhs.size() >= 2 && ((lhs.front() == '"' && lhs.back() == '"') || (lhs.front() == '\'' && lhs.back() == '\''))) {
            lhs = lhs.substr(1, lhs.size() - 2);
        }
        return lhs == rhs;
    }

    return expanded != "0" && expanded != "false";
}

static void populate_dom_data(std::shared_ptr<UIElement> elem, const PromptContext& ctx, const Environment& env) {
    if (!elem) return;

    // 1. Reactive conditional directives (v-if style)
    std::string show_cond = elem->get_attribute("show-if");
    if (!show_cond.empty() && !evaluate_condition(show_cond, env)) {
        elem->computed_style.display = DisplayType::NONE;
    }
    std::string hide_cond = elem->get_attribute("hide-if");
    if (!hide_cond.empty() && evaluate_condition(hide_cond, env)) {
        elem->computed_style.display = DisplayType::NONE;
    }

    // 2. Builtin tags
    if (elem->tag == "user") {
        if (elem->has_attribute("name")) {
            elem->text_content = elem->get_attribute("name");
        } else if (elem->text_content.empty()) {
            elem->text_content = ctx.user;
        }
    } else if (elem->tag == "hostname") {
        if (elem->has_attribute("name")) {
            elem->text_content = elem->get_attribute("name");
        } else if (elem->text_content.empty()) {
            elem->text_content = ctx.hostname;
        }
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
    } else if (elem->tag == "date") {
        elem->text_content = ctx.date_str;
    } else if (elem->tag == "cwd") {
        elem->text_content = ctx.cwd;
    } else {
        // Dynamic variable expansion on text content
        elem->text_content = expand_vars(elem->text_content, env);
    }

    // 3. Dynamic classes with $VAR
    std::set<std::string> updated_classes;
    for (const auto& c : elem->classes) {
        if (c.find('$') != std::string::npos) {
            std::string exp = expand_vars(c, env);
            auto spl = str_util::split(exp, ' ');
            for (const auto& sc : spl) {
                std::string tsc = str_util::trim(sc);
                if (!tsc.empty()) updated_classes.insert(tsc);
            }
        } else {
            updated_classes.insert(c);
        }
    }
    elem->classes = std::move(updated_classes);

    for (auto& child : elem->children) {
        populate_dom_data(child, ctx, env);
    }
}

static std::shared_ptr<UIElement> extract_element_by_tag(std::shared_ptr<UIElement>& root, const std::string& tag) {
    if (!root) return nullptr;
    if (root->tag == tag) {
        auto found = root;
        root = std::make_shared<UIElement>("segment");
        return found;
    }
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->tag == tag) {
            auto found = *it;
            root->children.erase(it);
            return found;
        }
        auto sub = extract_element_by_tag(*it, tag);
        if (sub) return sub;
    }
    return nullptr;
}

RenderResult PromptEngine::render(const PromptContext& ctx, uint64_t timestamp_ms) {
    auto dom = DOMParser::parse(template_html_);
    if (!dom) return RenderResult();

    populate_dom_data(dom, ctx, env_);

    int term_cols = Terminal::get_size().cols;
    if (term_cols < 20) term_cols = 80;

    RenderResult result;
    bool truecolor = Terminal::supports_truecolor();
    bool unicode = Terminal::supports_unicode();

    // 1. Separate Status Bar component (<statusbar>)
    auto statusbar_elem = extract_element_by_tag(dom, "statusbar");
    if (statusbar_elem && statusbar_elem->computed_style.display != DisplayType::NONE) {
        statusbar_elem->apply_styles(stylesheet_);
        auto slayout = LayoutEngine::compute_layout(statusbar_elem, term_cols, timestamp_ms);
        auto sres = TerminalRenderer::render(slayout, timestamp_ms, truecolor, unicode);
        result.statusbar_ansi = sres.ansi_output;
    }

    // 2. Separate Right Prompt component (<rprompt>)
    auto rprompt_elem = extract_element_by_tag(dom, "rprompt");
    if (rprompt_elem && rprompt_elem->computed_style.display != DisplayType::NONE) {
        rprompt_elem->apply_styles(stylesheet_);
        auto rlayout = LayoutEngine::compute_layout(rprompt_elem, term_cols, timestamp_ms);
        auto rres = TerminalRenderer::render(rlayout, timestamp_ms, truecolor, unicode);
        result.rprompt_ansi = rres.ansi_output;
        result.rprompt_width = rres.last_line_width;
    }

    // 3. Render main prompt tree
    dom->apply_styles(stylesheet_);
    auto layout = LayoutEngine::compute_layout(dom, term_cols, timestamp_ms);
    auto main_res = TerminalRenderer::render(layout, timestamp_ms, truecolor, unicode);

    result.ansi_output = main_res.ansi_output;
    result.total_lines = main_res.total_lines;
    result.last_line_width = main_res.last_line_width;

    // If rprompt exists and main prompt has multiple lines, format rprompt right-aligned on line 1
    if (result.rprompt_width > 0 && !result.rprompt_ansi.empty()) {
        size_t nl = result.ansi_output.find('\n');
        if (nl != std::string::npos) {
            std::string line1 = result.ansi_output.substr(0, nl);
            int line1_w = static_cast<int>(str_util::visual_width(line1));
            if (line1_w + result.rprompt_width < term_cols) {
                int pad = term_cols - line1_w - result.rprompt_width;
                std::string padded_line1 = line1 + std::string(static_cast<size_t>(pad), ' ') + result.rprompt_ansi;
                result.ansi_output = padded_line1 + result.ansi_output.substr(nl);
            }
        }
    }

    return result;
}

} // namespace aswell
