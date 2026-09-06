#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/style.hpp"

namespace aswell {

struct Selector {
    std::string tag;
    std::string class_name;
    std::string id;
    std::string pseudo;

    int specificity() const;
    bool matches(const std::string& elem_tag,
                 const std::set<std::string>& elem_classes,
                 const std::string& elem_id,
                 const std::set<std::string>& elem_pseudos) const;
};

struct Rule {
    Selector selector;
    Style style;
};

class StyleSheet {
public:
    void add_rule(const Rule& rule);
    Style compute_style(const std::string& tag,
                        const std::set<std::string>& classes = {},
                        const std::string& id = "",
                        const std::set<std::string>& pseudos = {}) const;

    void clear() { rules_.clear(); }
    const std::vector<Rule>& rules() const { return rules_; }

private:
    std::vector<Rule> rules_;
};

class CSSParser {
public:
    static StyleSheet parse(std::string_view css_text);

private:
    static void parse_declaration(Style& style, std::string_view prop, std::string_view val);
    static BoxSpacing parse_box_spacing(std::string_view val);
    static AnimationConfig parse_animation(std::string_view val);
};

} // namespace aswell
