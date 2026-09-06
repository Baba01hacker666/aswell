#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/style.hpp"
#include "aswell/ui/css_parser.hpp"

namespace aswell {

class UIElement : public std::enable_shared_from_this<UIElement> {
public:
    std::string tag;
    std::set<std::string> classes;
    std::string id;
    std::map<std::string, std::string> attributes;
    std::set<std::string> pseudos;
    std::string text_content;

    std::vector<std::shared_ptr<UIElement>> children;
    std::weak_ptr<UIElement> parent;

    Style computed_style;

    UIElement(std::string t = "") : tag(std::move(t)) {}

    void add_child(std::shared_ptr<UIElement> child);
    void apply_styles(const StyleSheet& sheet);

    std::string get_attribute(const std::string& key) const;
    bool has_class(const std::string& cls) const;
    void add_class(const std::string& cls);
};

class DOMParser {
public:
    static std::shared_ptr<UIElement> parse(std::string_view html);

private:
    static void skip_ws(std::string_view html, size_t& pos);
    static std::shared_ptr<UIElement> parse_element(std::string_view html, size_t& pos);
};

} // namespace aswell
