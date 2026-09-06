#include "aswell/ui/template_engine.hpp"
#include "aswell/ui/terminal.hpp"
#include "aswell/common.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

namespace aswell {

// ============================================================================
// Mathematical & Expression Parser
// ============================================================================

class MathParser {
public:
    MathParser(std::string_view src, const TemplateContext& ctx)
        : src_(src), ctx_(ctx), pos_(0) {}

    double parse() {
        skip_ws();
        if (pos_ >= src_.size()) return 0.0;
        double res = parse_lor();
        return res;
    }

private:
    std::string_view src_;
    const TemplateContext& ctx_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
    }

    char peek() {
        skip_ws();
        return (pos_ < src_.size()) ? src_[pos_] : '\0';
    }

    char get() {
        skip_ws();
        return (pos_ < src_.size()) ? src_[pos_++] : '\0';
    }

    bool match(std::string_view s) {
        skip_ws();
        if (src_.substr(pos_).rfind(s, 0) == 0) {
            pos_ += s.size();
            return true;
        }
        return false;
    }

    double parse_lor() {
        double left = parse_land();
        while (match("||")) {
            double right = parse_land();
            left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
        }
        return left;
    }

    double parse_land() {
        double left = parse_rel();
        while (match("&&")) {
            double right = parse_rel();
            left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
        }
        return left;
    }

    double parse_rel() {
        double left = parse_add();
        while (true) {
            if (match("==")) left = (left == parse_add()) ? 1.0 : 0.0;
            else if (match("!=")) left = (left != parse_add()) ? 1.0 : 0.0;
            else if (match("<=")) left = (left <= parse_add()) ? 1.0 : 0.0;
            else if (match(">=")) left = (left >= parse_add()) ? 1.0 : 0.0;
            else if (match("<")) left = (left < parse_add()) ? 1.0 : 0.0;
            else if (match(">")) left = (left > parse_add()) ? 1.0 : 0.0;
            else break;
        }
        return left;
    }

    double parse_add() {
        double left = parse_mul();
        while (true) {
            if (match("+")) left += parse_mul();
            else if (match("-")) left -= parse_mul();
            else break;
        }
        return left;
    }

    double parse_mul() {
        double left = parse_unary();
        while (true) {
            if (match("*")) left *= parse_unary();
            else if (match("/")) {
                double r = parse_unary();
                left = (r != 0.0) ? left / r : 0.0;
            } else if (match("%")) {
                double r = parse_unary();
                left = (r != 0.0) ? std::fmod(left, r) : 0.0;
            } else break;
        }
        return left;
    }

    double parse_unary() {
        if (match("+")) return parse_unary();
        if (match("-")) return -parse_unary();
        if (match("!")) return (parse_unary() == 0.0) ? 1.0 : 0.0;
        return parse_primary();
    }

    double parse_primary() {
        skip_ws();
        char c = peek();

        if (c == '(') {
            get(); // eat '('
            double v = parse_lor();
            match(")");
            return v;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = pos_;
            while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
                pos_++;
            }
            try {
                return std::stod(std::string(src_.substr(start, pos_ - start)));
            } catch (...) {
                return 0.0;
            }
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            size_t start = pos_;
            while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_' || src_[pos_] == '$')) {
                pos_++;
            }
            std::string id = std::string(src_.substr(start, pos_ - start));

            // Function calls
            if (peek() == '(') {
                get(); // eat '('
                std::vector<double> args;
                if (peek() != ')') {
                    while (true) {
                        args.push_back(parse_lor());
                        if (match(",")) continue;
                        break;
                    }
                }
                match(")");

                if (id == "sin" && !args.empty()) return std::sin(args[0]);
                if (id == "cos" && !args.empty()) return std::cos(args[0]);
                if (id == "abs" && !args.empty()) return std::fabs(args[0]);
                if (id == "round" && !args.empty()) return std::round(args[0]);
                if (id == "floor" && !args.empty()) return std::floor(args[0]);
                if (id == "min" && args.size() >= 2) return std::min(args[0], args[1]);
                if (id == "max" && args.size() >= 2) return std::max(args[0], args[1]);
                if (id == "clamp" && args.size() >= 3) return std::clamp(args[0], args[1], args[2]);
                if (id == "if" && args.size() >= 3) return (args[0] != 0.0) ? args[1] : args[2];
                return 0.0;
            }

            // Variable lookup
            if (id == "true") return 1.0;
            if (id == "false") return 0.0;
            if (id[0] == '$') id = id.substr(1);
            return ctx_.get_num(id, 0.0);
        }

        if (pos_ < src_.size()) pos_++;
        return 0.0;
    }
};

double TemplateEngine::eval_math(const std::string& expr, const TemplateContext& ctx) {
    MathParser parser(expr, ctx);
    return parser.parse();
}

std::string TemplateEngine::eval_expr(const std::string& expr_raw, const TemplateContext& ctx) {
    std::string expr = str_util::trim(expr_raw);
    if (expr.empty()) return "";

    // Check filters: expr | filter
    size_t pipe_pos = expr.find('|');
    std::string base_expr = (pipe_pos != std::string::npos) ? str_util::trim(expr.substr(0, pipe_pos)) : expr;
    std::string filter = (pipe_pos != std::string::npos) ? str_util::trim(expr.substr(pipe_pos + 1)) : "";

    std::string val;
    // Check if base_expr is string literal "..." or '...'
    if (base_expr.size() >= 2 && ((base_expr.front() == '"' && base_expr.back() == '"') ||
                                  (base_expr.front() == '\'' && base_expr.back() == '\''))) {
        val = base_expr.substr(1, base_expr.size() - 2);
    } else if (ctx.has(base_expr)) {
        val = ctx.get(base_expr);
    } else {
        // Evaluate math
        double num = eval_math(base_expr, ctx);
        if (std::floor(num) == num && !std::isinf(num)) {
            val = std::to_string(static_cast<long long>(num));
        } else {
            std::ostringstream oss;
            oss << num;
            val = oss.str();
        }
    }

    if (filter == "upper") {
        for (char& c : val) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    } else if (filter == "lower") {
        for (char& c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else if (filter == "trim") {
        val = str_util::trim(val);
    } else if (filter == "length" || filter == "len") {
        val = std::to_string(val.size());
    }

    return val;
}

// ============================================================================
// Template Parser & Evaluator
// ============================================================================

struct TemplateNode {
    std::string tag;
    std::map<std::string, std::string> attrs;
    std::string text;
    std::vector<std::shared_ptr<TemplateNode>> children;
};

class TemplateParser {
public:
    static std::shared_ptr<TemplateNode> parse(std::string_view src) {
        auto root = std::make_shared<TemplateNode>();
        root->tag = "root";
        size_t pos = 0;
        parse_children(src, pos, root);
        return root;
    }

private:
    static void skip_ws(std::string_view s, size_t& p) {
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
    }

    static void parse_children(std::string_view s, size_t& p, std::shared_ptr<TemplateNode> parent) {
        while (p < s.size()) {
            if (s[p] == '<') {
                // Check comment
                if (s.substr(p, 4) == "<!--") {
                    size_t end = s.find("-->", p);
                    p = (end == std::string_view::npos) ? s.size() : end + 3;
                    continue;
                }
                // Check closing tag
                if (p + 1 < s.size() && s[p + 1] == '/') {
                    break; // parent will close
                }
                auto child = parse_tag(s, p);
                if (child) {
                    parent->children.push_back(child);
                }
            } else {
                size_t next_tag = s.find('<', p);
                if (next_tag == std::string_view::npos) next_tag = s.size();
                std::string t = std::string(s.substr(p, next_tag - p));
                if (!t.empty()) {
                    auto tn = std::make_shared<TemplateNode>();
                    tn->tag = "_text";
                    tn->text = t;
                    parent->children.push_back(tn);
                }
                p = next_tag;
            }
        }
    }

    static std::shared_ptr<TemplateNode> parse_tag(std::string_view s, size_t& p) {
        p++; // eat '<'
        skip_ws(s, p);

        size_t tag_start = p;
        while (p < s.size() && !std::isspace(static_cast<unsigned char>(s[p])) && s[p] != '>' && s[p] != '/') {
            p++;
        }
        std::string tag = std::string(s.substr(tag_start, p - tag_start));
        auto node = std::make_shared<TemplateNode>();
        node->tag = tag;

        // Parse attributes
        bool self_closing = false;
        while (p < s.size()) {
            skip_ws(s, p);
            if (p < s.size() && s[p] == '/') {
                self_closing = true;
                p++;
                continue;
            }
            if (p < s.size() && s[p] == '>') {
                p++;
                break;
            }
            // Attribute name
            size_t k_start = p;
            while (p < s.size() && !std::isspace(static_cast<unsigned char>(s[p])) && s[p] != '=' && s[p] != '>' && s[p] != '/') {
                p++;
            }
            std::string key = std::string(s.substr(k_start, p - k_start));
            skip_ws(s, p);
            std::string val = "true";
            if (p < s.size() && s[p] == '=') {
                p++;
                skip_ws(s, p);
                if (p < s.size() && (s[p] == '"' || s[p] == '\'')) {
                    char quote = s[p++];
                    size_t v_start = p;
                    while (p < s.size() && s[p] != quote) p++;
                    val = std::string(s.substr(v_start, p - v_start));
                    if (p < s.size()) p++; // eat quote
                }
            }
            if (!key.empty()) {
                node->attrs[key] = val;
            }
        }

        if (!self_closing) {
            parse_children(s, p, node);
            // Eat closing tag </tag>
            if (s.substr(p, 2) == "</") {
                size_t close_end = s.find('>', p);
                p = (close_end == std::string_view::npos) ? s.size() : close_end + 1;
            }
        }

        return node;
    }
};

static std::string interpolate_text(const std::string& text, TemplateContext& ctx) {
    std::string res;
    res.reserve(text.size() + 16);
    size_t i = 0;
    while (i < text.size()) {
        if (i + 1 < text.size() && text[i] == '{' && text[i + 1] == '{') {
            size_t end = text.find("}}", i + 2);
            if (end != std::string::npos) {
                std::string expr = text.substr(i + 2, end - (i + 2));
                res += TemplateEngine::eval_expr(expr, ctx);
                i = end + 2;
                continue;
            }
        }
        res.push_back(text[i++]);
    }
    return res;
}

static std::string eval_node(const std::shared_ptr<TemplateNode>& node, TemplateContext& ctx) {
    if (!node) return "";

    if (node->tag == "_text") {
        return interpolate_text(node->text, ctx);
    }

    if (node->tag == "root" || node->tag == "template" || node->tag == "frame" || node->tag == "events") {
        std::string out;
        for (const auto& child : node->children) {
            out += eval_node(child, ctx);
        }
        return out;
    }

    if (node->tag == "let" || node->tag == "set") {
        for (const auto& [k, v] : node->attrs) {
            if (k == "var" && node->attrs.count("val")) {
                double n = TemplateEngine::eval_math(node->attrs.at("val"), ctx);
                ctx.set_num(v, n);
            } else if (k == "var" && node->attrs.count("value")) {
                double n = TemplateEngine::eval_math(node->attrs.at("value"), ctx);
                ctx.set_num(v, n);
            } else {
                double n = TemplateEngine::eval_math(v, ctx);
                ctx.set_num(k, n);
            }
        }
        return "";
    }

    if (node->tag == "if") {
        std::string cond = node->attrs.count("condition") ? node->attrs.at("condition") : (node->attrs.count("test") ? node->attrs.at("test") : "1");
        bool satisfied = (TemplateEngine::eval_math(cond, ctx) != 0.0);
        if (satisfied) {
            std::string out;
            for (const auto& child : node->children) {
                if (child->tag == "elif" || child->tag == "else") break;
                out += eval_node(child, ctx);
            }
            return out;
        } else {
            // Check elif / else
            for (size_t i = 0; i < node->children.size(); ++i) {
                const auto& ch = node->children[i];
                if (ch->tag == "elif") {
                    std::string elif_cond = ch->attrs.count("condition") ? ch->attrs.at("condition") : "1";
                    if (TemplateEngine::eval_math(elif_cond, ctx) != 0.0) {
                        return eval_node(ch, ctx);
                    }
                } else if (ch->tag == "else") {
                    return eval_node(ch, ctx);
                }
            }
        }
        return "";
    }

    if (node->tag == "elif" || node->tag == "else") {
        std::string out;
        for (const auto& child : node->children) {
            out += eval_node(child, ctx);
        }
        return out;
    }

    if (node->tag == "for") {
        std::string var = node->attrs.count("var") ? node->attrs.at("var") : "i";
        double from = node->attrs.count("from") ? TemplateEngine::eval_math(node->attrs.at("from"), ctx) : 0.0;
        double to = node->attrs.count("to") ? TemplateEngine::eval_math(node->attrs.at("to"), ctx) : 0.0;
        double step = node->attrs.count("step") ? TemplateEngine::eval_math(node->attrs.at("step"), ctx) : 1.0;
        if (step == 0.0) step = 1.0;

        std::string out;
        for (double i = from; (step > 0) ? (i <= to) : (i >= to); i += step) {
            ctx.set_num(var, i);
            for (const auto& child : node->children) {
                out += eval_node(child, ctx);
            }
        }
        return out;
    }

    if (node->tag == "repeat") {
        std::string var = node->attrs.count("var") ? node->attrs.at("var") : "idx";
        int count = node->attrs.count("count") ? static_cast<int>(TemplateEngine::eval_math(node->attrs.at("count"), ctx)) : 1;
        std::string out;
        for (int i = 0; i < count; ++i) {
            ctx.set_num(var, i);
            for (const auto& child : node->children) {
                out += eval_node(child, ctx);
            }
        }
        return out;
    }

    if (node->tag == "grid") {
        int rows = node->attrs.count("rows") ? static_cast<int>(TemplateEngine::eval_math(node->attrs.at("rows"), ctx)) : 8;
        int cols = node->attrs.count("cols") ? static_cast<int>(TemplateEngine::eval_math(node->attrs.at("cols"), ctx)) : 54;
        std::string r_var = node->attrs.count("row_var") ? node->attrs.at("row_var") : "y";
        std::string c_var = node->attrs.count("col_var") ? node->attrs.at("col_var") : "x";

        std::string out;
        for (int y = 0; y < rows; ++y) {
            ctx.set_num(r_var, y);
            for (int x = 0; x < cols; ++x) {
                ctx.set_num(c_var, x);
                for (const auto& child : node->children) {
                    out += eval_node(child, ctx);
                }
            }
            out += "\r\n";
        }
        return out;
    }

    if (node->tag == "color") {
        std::string fg = node->attrs.count("fg") ? node->attrs.at("fg") : "";
        std::string bg = node->attrs.count("bg") ? node->attrs.at("bg") : "";
        bool bold = (node->attrs.count("bold") && (node->attrs.at("bold") == "true" || node->attrs.at("bold") == "1"));

        std::string ansi_start;
        if (bold) ansi_start += "\033[1m";
        if (!fg.empty()) {
            Color c = Color::from_hex(fg);
            if (!c.is_none) {
                ansi_start += c.to_fg_ansi(true);
            }
        }
        if (!bg.empty()) {
            Color c = Color::from_hex(bg);
            if (!c.is_none) {
                ansi_start += c.to_bg_ansi(true);
            }
        }

        std::string inner;
        for (const auto& child : node->children) {
            inner += eval_node(child, ctx);
        }

        return ansi_start + inner + "\033[0m";
    }

    if (node->tag == "text" || node->tag == "span") {
        std::string inner;
        for (const auto& child : node->children) {
            inner += eval_node(child, ctx);
        }
        return inner;
    }

    if (node->tag == "cursor") {
        std::string act = node->attrs.count("action") ? node->attrs.at("action") : "";
        int cnt = node->attrs.count("count") ? static_cast<int>(TemplateEngine::eval_math(node->attrs.at("count"), ctx)) : 1;
        if (act == "up") return "\033[" + std::to_string(cnt) + "A\r";
        if (act == "down") return "\033[" + std::to_string(cnt) + "B\r";
        if (act == "clear_line") return "\033[2K\r";
        if (act == "hide") return "\033[?25l";
        if (act == "show") return "\033[?25h";
        return "";
    }

    if (node->tag == "badge") {
        std::string inner;
        for (const auto& child : node->children) {
            inner += eval_node(child, ctx);
        }
        return inner + "\r\n";
    }

    // Default: evaluate children
    std::string out;
    for (const auto& child : node->children) {
        out += eval_node(child, ctx);
    }
    return out;
}

std::string TemplateEngine::render(const std::string& template_str, TemplateContext& ctx) {
    auto root = TemplateParser::parse(template_str);
    return eval_node(root, ctx);
}

std::string TemplateEngine::render_file(const std::string& file_path, TemplateContext& ctx) {
    std::ifstream f(file_path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return render(ss.str(), ctx);
}

void TemplateEngine::play_animation(const std::string& template_str, TemplateContext& ctx, bool force_headless) {
    auto root = TemplateParser::parse(template_str);

    // Find <animation> node
    std::shared_ptr<TemplateNode> anim_node = nullptr;
    std::function<void(std::shared_ptr<TemplateNode>)> find_anim = [&](std::shared_ptr<TemplateNode> n) {
        if (!n) return;
        if (n->tag == "animation") {
            anim_node = n;
            return;
        }
        for (const auto& ch : n->children) {
            find_anim(ch);
            if (anim_node) return;
        }
    };
    find_anim(root);

    if (!anim_node) {
        std::cout << render(template_str, ctx);
        return;
    }

    int frames = anim_node->attrs.count("frames") ? static_cast<int>(eval_math(anim_node->attrs.at("frames"), ctx)) : 20;
    int height = anim_node->attrs.count("height") ? static_cast<int>(eval_math(anim_node->attrs.at("height"), ctx)) : 8;
    int delay_ms = 35;
    if (anim_node->attrs.count("delay")) {
        std::string d = anim_node->attrs.at("delay");
        if (str_util::ends_with(d, "ms")) d = d.substr(0, d.size() - 2);
        delay_ms = std::max(5, static_cast<int>(eval_math(d, ctx)));
    }
    bool clear = (anim_node->attrs.count("clear") && anim_node->attrs.at("clear") == "true");

    bool interactive = Terminal::is_interactive_tty() && !force_headless;

    if (!interactive) {
        // Headless / CI verification mode: render 3 representative keyframes
        std::cout << "\033[1;33m[Template Animation Engine — Headless Keyframes]\033[0m\n";
        int keyframes[] = { 0, frames / 2, frames - 1 };
        for (int kf : keyframes) {
            ctx.set_num("f", kf);
            ctx.set_num("frame", kf);
            ctx.set_num("total_frames", frames);
            std::cout << "--- Frame " << kf << " / " << frames << " ---\n";
            std::cout << eval_node(anim_node, ctx);
        }
        // Render badges or following content
        std::cout << render(template_str, ctx);
        return;
    }

    Terminal::hide_cursor();

    // Allocate space
    for (int i = 0; i < height; ++i) {
        std::cout << "\r\n";
    }
    std::cout.flush();

    for (int f = 0; f < frames; ++f) {
        std::cout << "\033[" << height << "A\r";
        ctx.set_num("f", f);
        ctx.set_num("frame", f);
        ctx.set_num("total_frames", frames);

        std::string frame_output = eval_node(anim_node, ctx);
        std::cout << frame_output;
        std::cout.flush();

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    if (clear) {
        std::cout << "\033[" << height << "A\r";
        for (int i = 0; i < height; ++i) {
            std::cout << "\033[2K\r\n";
        }
        std::cout << "\033[" << height << "A\r";
    }

    Terminal::show_cursor();

    // Render remaining nodes in root that aren't the animation block (like <badge>)
    for (const auto& ch : root->children) {
        if (ch->tag != "animation") {
            std::string rem = eval_node(ch, ctx);
            if (!rem.empty()) {
                std::cout << rem;
            }
        }
    }
    std::cout.flush();
}

std::string TemplateEngine::get_templates_dir() {
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/root";
    return base + "/.config/aswell/templates";
}

void TemplateEngine::ensure_default_templates() {
    std::string tdir = get_templates_dir();
    mkdir(tdir.c_str(), 0755);

    // 1. ~/.config/aswell/templates/events.html
    std::string events_file = tdir + "/events.html";
    std::ifstream check_events(events_file);
    if (!check_events) {
        std::ofstream f(events_file);
        f << R"TMPL(<!-- ~/.config/aswell/templates/events.html -->
<!-- Declarative Event Effects for Aswell Shell -->
<events>
  <!-- File Deletion Event: Fire burning and moving upwards from command line -->
  <event on="rm">
    <animation frames="19" delay="35ms" height="8" width="54" clear="true">
      <frame var="f">
        <grid rows="8" cols="54" row_var="y" col_var="x">
          <!-- Procedural TrueColor heat formula computed directly in template -->
          <let heat="clamp(round((if(f < 13, 245.0 * (1.0 - (7 - y) / 7.5), 230.0 * (1.0 - (f - 12) / 7.0) * max(0.0, 1.0 - abs((7 - y) - (f - 12) * 1.3) / 3.0))) + sin(x * 0.45 + f * 0.85) * 25.0 + cos(x * 0.9 - f * 1.1 + y * 0.7) * 20.0), 0, 255)" />
          <if condition="heat >= 210">
            <color fg="#ffff88" bold="true">█</color>
          <elif condition="heat >= 150">
            <color fg="#ff8800">▓</color>
          <elif condition="heat >= 95">
            <color fg="#dd3300">▒</color>
          <elif condition="heat >= 50">
            <color fg="#991500">░</color>
          <elif condition="heat >= 20">
            <color fg="#661005">·</color>
          <elif condition="heat >= 8">
            <color fg="#444444">.</color>
          <else>
            <text> </text>
          </else>
          </if>
        </grid>
      </frame>
    </animation>
    <badge>🔥 <color fg="#ff3333" bold="true">[INCINERATED]</color> <color fg="#ffaa00" bold="true">{{ target }}</color> <color fg="#777777">— burned into smoke & ash</color> 🔥</badge>
  </event>
</events>
)TMPL";
    }

    // 2. ~/.config/aswell/templates/fire.html (standalone fire effect template)
    std::string fire_file = tdir + "/fire.html";
    std::ifstream check_fire(fire_file);
    if (!check_fire) {
        std::ofstream f(fire_file);
        f << R"TMPL(<!-- Standalone Fire Incineration Template -->
<template name="fire_incinerate">
  <animation frames="19" delay="35ms" height="8" width="54" clear="true">
    <frame var="f">
      <grid rows="8" cols="54" row_var="y" col_var="x">
        <let heat="clamp(round((if(f < 13, 245.0 * (1.0 - (7 - y) / 7.5), 230.0 * (1.0 - (f - 12) / 7.0) * max(0.0, 1.0 - abs((7 - y) - (f - 12) * 1.3) / 3.0))) + sin(x * 0.45 + f * 0.85) * 25.0 + cos(x * 0.9 - f * 1.1 + y * 0.7) * 20.0), 0, 255)" />
        <if condition="heat >= 210">
          <color fg="#ffff88" bold="true">█</color>
        <elif condition="heat >= 150">
          <color fg="#ff8800">▓</color>
        <elif condition="heat >= 95">
          <color fg="#dd3300">▒</color>
        <elif condition="heat >= 50">
          <color fg="#991500">░</color>
        <elif condition="heat >= 20">
          <color fg="#661005">·</color>
        <elif condition="heat >= 8">
          <color fg="#444444">.</color>
        <else>
          <text> </text>
        </else>
        </if>
      </grid>
    </frame>
  </animation>
  <badge>🔥 <color fg="#ff3333" bold="true">[INCINERATED]</color> <color fg="#ffaa00" bold="true">{{ target }}</color> <color fg="#777777">— burned into smoke & ash</color> 🔥</badge>
</template>
)TMPL";
    }
}

bool TemplateEngine::handle_event(const std::string& cmd_line, bool is_interactive) {
    if (!is_interactive) return false;

    std::string trimmed = str_util::trim(cmd_line);
    if (trimmed.empty()) return false;

    auto tokens = str_util::split(trimmed, ' ');
    if (tokens.empty()) return false;

    std::string cmd = tokens[0];
    size_t last_slash = cmd.find_last_of('/');
    if (last_slash != std::string::npos) cmd = cmd.substr(last_slash + 1);

    // Check if it matches an event type
    std::string event_type;
    std::string target;
    if (cmd == "rm" || cmd == "rmdir" || cmd == "unlink" || cmd == "shred") {
        event_type = "rm";
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (!tokens[i].empty() && tokens[i][0] != '-') {
                target = tokens[i];
                break;
            }
        }
        if (target.empty() && tokens.size() > 1) target = tokens[1];
        if (target.empty()) target = "file";
    }

    if (event_type.empty()) return false;

    ensure_default_templates();
    std::string events_file = get_templates_dir() + "/events.html";

    std::ifstream f(events_file);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    auto root = TemplateParser::parse(content);
    // Find <event on="rm">
    std::shared_ptr<TemplateNode> event_node = nullptr;
    for (const auto& ch : root->children) {
        if (ch->tag == "event" && ch->attrs.count("on") && ch->attrs.at("on") == event_type) {
            event_node = ch;
            break;
        }
    }

    if (!event_node) return false;

    TemplateContext ctx;
    ctx.set("target", target);
    ctx.set("cmd", cmd_line);
    ctx.set("event", event_type);

    play_animation(eval_node(event_node, ctx), ctx, !Terminal::is_interactive_tty());
    return true;
}

} // namespace aswell
