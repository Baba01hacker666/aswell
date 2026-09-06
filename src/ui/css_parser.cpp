#include "aswell/ui/css_parser.hpp"

namespace aswell {

int Selector::specificity() const {
    int spec = 0;
    if (!id.empty()) spec += 100;
    if (!class_name.empty()) spec += 10;
    if (!pseudo.empty()) spec += 10;
    if (!tag.empty()) spec += 1;
    return spec;
}

bool Selector::matches(const std::string& elem_tag,
                       const std::set<std::string>& elem_classes,
                       const std::string& elem_id,
                       const std::set<std::string>& elem_pseudos) const {
    if (!tag.empty() && tag != "*" && tag != elem_tag) return false;
    if (!id.empty() && id != elem_id) return false;
    if (!class_name.empty() && elem_classes.find(class_name) == elem_classes.end()) return false;
    if (!pseudo.empty() && elem_pseudos.find(pseudo) == elem_pseudos.end()) return false;
    return true;
}

void StyleSheet::add_rule(const Rule& rule) {
    rules_.push_back(rule);
}

Style StyleSheet::compute_style(const std::string& tag,
                                const std::set<std::string>& classes,
                                const std::string& id,
                                const std::set<std::string>& pseudos) const {
    Style computed;
    // Collect all matching rules sorted by specificity
    std::vector<std::pair<int, const Rule*>> matched;

    for (const auto& r : rules_) {
        if (r.selector.matches(tag, classes, id, pseudos)) {
            matched.emplace_back(r.selector.specificity(), &r);
        }
    }

    std::stable_sort(matched.begin(), matched.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    for (const auto& [spec, r] : matched) {
        computed.merge(r->style);
    }

    return computed;
}

static Selector parse_selector(std::string_view s) {
    s = str_util::trim_sv(s);
    Selector sel;

    // Check for pseudo :
    size_t colon = s.find(':');
    if (colon != std::string_view::npos) {
        sel.pseudo = std::string(s.substr(colon + 1));
        s = s.substr(0, colon);
    }

    // Check for class .
    size_t dot = s.find('.');
    if (dot != std::string_view::npos) {
        sel.class_name = std::string(s.substr(dot + 1));
        s = s.substr(0, dot);
    }

    // Check for ID #
    size_t hash = s.find('#');
    if (hash != std::string_view::npos) {
        sel.id = std::string(s.substr(hash + 1));
        s = s.substr(0, hash);
    }

    sel.tag = std::string(s);
    return sel;
}

BoxSpacing CSSParser::parse_box_spacing(std::string_view val) {
    BoxSpacing bs;
    auto parts = str_util::split(val, ' ');
    std::vector<int> nums;
    for (const auto& p : parts) {
        std::string trimmed = str_util::trim(p);
        if (trimmed.empty()) continue;
        // strip px
        if (str_util::ends_with(trimmed, "px")) {
            trimmed = trimmed.substr(0, trimmed.size() - 2);
        }
        try {
            nums.push_back(std::stoi(trimmed));
        } catch (...) {}
    }

    if (nums.size() == 1) {
        bs.top = bs.right = bs.bottom = bs.left = nums[0];
    } else if (nums.size() == 2) {
        bs.top = bs.bottom = nums[0];
        bs.right = bs.left = nums[1];
    } else if (nums.size() == 4) {
        bs.top = nums[0];
        bs.right = nums[1];
        bs.bottom = nums[2];
        bs.left = nums[3];
    }
    return bs;
}

AnimationConfig CSSParser::parse_animation(std::string_view val) {
    AnimationConfig cfg;
    auto parts = str_util::split(val, ' ');
    for (const auto& raw_p : parts) {
        std::string p = str_util::to_lower(str_util::trim(raw_p));
        if (p.empty()) continue;

        if (p == "pulse") cfg.type = AnimationType::PULSE;
        else if (p == "rainbow") cfg.type = AnimationType::RAINBOW;
        else if (p == "fire") cfg.type = AnimationType::FIRE;
        else if (p == "spin") cfg.type = AnimationType::SPIN;
        else if (p == "wave") cfg.type = AnimationType::WAVE;
        else if (p == "scramble" || p == "glitch" || p == "matrix") cfg.type = AnimationType::SCRAMBLE;
        else if (p == "infinite") cfg.infinite = true;
        else if (str_util::ends_with(p, "ms")) {
            try { cfg.duration_ms = std::stoi(p.substr(0, p.size() - 2)); } catch (...) {}
        } else if (str_util::ends_with(p, "s")) {
            try {
                float s = std::stof(p.substr(0, p.size() - 1));
                cfg.duration_ms = static_cast<int>(s * 1000.0f);
            } catch (...) {}
        }
    }
    return cfg;
}

void CSSParser::parse_declaration(Style& style, std::string_view prop, std::string_view val) {
    prop = str_util::trim_sv(prop);
    val = str_util::trim_sv(val);
    std::string lprop = str_util::to_lower(prop);

    if (lprop == "color") {
        style.color = Color::parse(val);
    } else if (lprop == "background" || lprop == "background-color") {
        style.bg_color = Color::parse(val);
    } else if (lprop == "font-weight") {
        style.bold = (val == "bold" || val == "700" || val == "800" || val == "900");
    } else if (lprop == "font-style") {
        style.italic = (val == "italic");
    } else if (lprop == "text-decoration") {
        if (val == "underline") style.underline = true;
        else if (val == "line-through" || val == "strikethrough") style.strikethrough = true;
    } else if (lprop == "display") {
        if (val == "block") style.display = DisplayType::BLOCK;
        else if (val == "flex") style.display = DisplayType::FLEX;
        else if (val == "none") style.display = DisplayType::NONE;
        else style.display = DisplayType::INLINE;
    } else if (lprop == "padding") {
        style.padding = parse_box_spacing(val);
    } else if (lprop == "padding-left") {
        try { style.padding.left = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "padding-right") {
        try { style.padding.right = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "padding-top") {
        try { style.padding.top = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "padding-bottom") {
        try { style.padding.bottom = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "margin") {
        style.margin = parse_box_spacing(val);
    } else if (lprop == "margin-left") {
        try { style.margin.left = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "margin-right") {
        try { style.margin.right = std::stoi(std::string(val)); } catch (...) {}
    } else if (lprop == "border") {
        // e.g. "1px solid #cyan" or "rounded cyan"
        auto parts = str_util::split(val, ' ');
        style.border_type = BorderType::SOLID;
        style.border_width = {1, 1, 1, 1};
        for (const auto& p : parts) {
            std::string lp = str_util::to_lower(str_util::trim(p));
            if (lp == "solid") style.border_type = BorderType::SOLID;
            else if (lp == "rounded") style.border_type = BorderType::ROUNDED;
            else if (lp == "double") style.border_type = BorderType::DOUBLE;
            else if (lp == "dashed") style.border_type = BorderType::DASHED;
            else if (lp == "none") style.border_type = BorderType::NONE;
            else {
                Color c = Color::parse(lp);
                if (!c.is_none) style.border_color = c;
            }
        }
    } else if (lprop == "border-radius") {
        style.border_type = BorderType::ROUNDED;
    } else if (lprop == "border-color") {
        style.border_color = Color::parse(val);
    } else if (lprop == "content") {
        std::string s = std::string(val);
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            s = s.substr(1, s.size() - 2);
        }
        style.content_override = s;
    } else if (lprop == "animation") {
        style.animation = parse_animation(val);
    }
}

StyleSheet CSSParser::parse(std::string_view css_text) {
    StyleSheet sheet;
    size_t pos = 0;

    while (pos < css_text.size()) {
        // Skip whitespace and comments
        while (pos < css_text.size() && (std::isspace(static_cast<unsigned char>(css_text[pos])) || (css_text[pos] == '/' && pos + 1 < css_text.size() && css_text[pos + 1] == '*'))) {
            if (css_text[pos] == '/') {
                pos += 2;
                while (pos + 1 < css_text.size() && !(css_text[pos] == '*' && css_text[pos + 1] == '/')) {
                    pos++;
                }
                pos += 2;
            } else {
                pos++;
            }
        }
        if (pos >= css_text.size()) break;

        // Find selector before '{'
        size_t brace_open = css_text.find('{', pos);
        if (brace_open == std::string_view::npos) break;

        std::string_view sel_str = css_text.substr(pos, brace_open - pos);
        pos = brace_open + 1;

        size_t brace_close = css_text.find('}', pos);
        if (brace_close == std::string_view::npos) break;

        std::string_view decls_str = css_text.substr(pos, brace_close - pos);
        pos = brace_close + 1;

        // Multiple comma-separated selectors possible: "directory, path"
        auto sel_tokens = str_util::split(sel_str, ',');
        Style style;

        // Parse declarations inside { ... }
        auto decl_lines = str_util::split(decls_str, ';');
        for (const auto& decl : decl_lines) {
            size_t colon = decl.find(':');
            if (colon != std::string::npos) {
                std::string prop = decl.substr(0, colon);
                std::string val = decl.substr(colon + 1);
                parse_declaration(style, prop, val);
            }
        }

        for (const auto& st : sel_tokens) {
            Selector sel = parse_selector(st);
            sheet.add_rule({sel, style});
        }
    }

    return sheet;
}

} // namespace aswell
