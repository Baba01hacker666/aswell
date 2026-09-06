#include "aswell/ui/dom.hpp"

namespace aswell {

void UIElement::add_child(std::shared_ptr<UIElement> child) {
    if (child) {
        child->parent = shared_from_this();
        children.push_back(child);
    }
}

std::string UIElement::get_attribute(const std::string& key) const {
    auto it = attributes.find(key);
    if (it != attributes.end()) return it->second;
    return "";
}

bool UIElement::has_class(const std::string& cls) const {
    return classes.find(cls) != classes.end();
}

void UIElement::add_class(const std::string& cls) {
    classes.insert(cls);
}

void UIElement::apply_styles(const StyleSheet& sheet) {
    computed_style = sheet.compute_style(tag, classes, id, pseudos);

    // Apply content override if present
    if (computed_style.content_override.has_value()) {
        text_content = *computed_style.content_override;
    }

    for (auto& child : children) {
        child->apply_styles(sheet);
    }
}

void DOMParser::skip_ws(std::string_view html, size_t& pos) {
    while (pos < html.size() && std::isspace(static_cast<unsigned char>(html[pos]))) {
        pos++;
    }
}

std::shared_ptr<UIElement> DOMParser::parse_element(std::string_view html, size_t& pos) {
    skip_ws(html, pos);
    if (pos >= html.size()) return nullptr;

    if (html[pos] != '<') {
        // Plain text node
        size_t next_tag = html.find('<', pos);
        std::string text;
        if (next_tag == std::string_view::npos) {
            text = std::string(html.substr(pos));
            pos = html.size();
        } else {
            text = std::string(html.substr(pos, next_tag - pos));
            pos = next_tag;
        }
        text = str_util::trim(text);
        if (text.empty()) return nullptr;

        auto elem = std::make_shared<UIElement>("text");
        elem->text_content = text;
        return elem;
    }

    pos++; // consume '<'
    skip_ws(html, pos);

    // Tag name
    size_t tag_start = pos;
    while (pos < html.size() && !std::isspace(static_cast<unsigned char>(html[pos])) && html[pos] != '>' && html[pos] != '/') {
        pos++;
    }
    std::string tag_name = std::string(html.substr(tag_start, pos - tag_start));
    auto elem = std::make_shared<UIElement>(tag_name);

    // Parse attributes
    while (pos < html.size() && html[pos] != '>' && html[pos] != '/') {
        skip_ws(html, pos);
        if (pos >= html.size() || html[pos] == '>' || html[pos] == '/') break;

        size_t attr_name_start = pos;
        while (pos < html.size() && !std::isspace(static_cast<unsigned char>(html[pos])) && html[pos] != '=' && html[pos] != '>' && html[pos] != '/') {
            pos++;
        }
        std::string attr_name = std::string(html.substr(attr_name_start, pos - attr_name_start));
        skip_ws(html, pos);

        std::string attr_val;
        if (pos < html.size() && html[pos] == '=') {
            pos++; // consume '='
            skip_ws(html, pos);
            if (pos < html.size() && (html[pos] == '"' || html[pos] == '\'')) {
                char quote = html[pos++];
                size_t val_start = pos;
                while (pos < html.size() && html[pos] != quote) {
                    pos++;
                }
                attr_val = std::string(html.substr(val_start, pos - val_start));
                if (pos < html.size()) pos++; // consume closing quote
            } else {
                size_t val_start = pos;
                while (pos < html.size() && !std::isspace(static_cast<unsigned char>(html[pos])) && html[pos] != '>' && html[pos] != '/') {
                    pos++;
                }
                attr_val = std::string(html.substr(val_start, pos - val_start));
            }
        }

        elem->attributes[attr_name] = attr_val;
        if (attr_name == "class") {
            auto cls_list = str_util::split(attr_val, ' ');
            for (const auto& c : cls_list) {
                std::string tc = str_util::trim(c);
                if (!tc.empty()) elem->classes.insert(tc);
            }
        } else if (attr_name == "id") {
            elem->id = attr_val;
        }
    }

    skip_ws(html, pos);
    // Check self-closing <tag />
    if (pos < html.size() && html[pos] == '/') {
        pos++;
        if (pos < html.size() && html[pos] == '>') pos++;
        return elem;
    }

    if (pos < html.size() && html[pos] == '>') {
        pos++; // consume '>'
    }

    // Parse children and inner text
    std::string closing_tag = "</" + tag_name + ">";

    while (pos < html.size()) {
        skip_ws(html, pos);
        if (pos >= html.size()) break;

        // Check if closing tag
        if (html.substr(pos, closing_tag.size()) == closing_tag) {
            pos += closing_tag.size();
            break;
        }

        // Check if closing another tag (mismatched)
        if (html.substr(pos, 2) == "</") {
            size_t close_end = html.find('>', pos);
            if (close_end != std::string_view::npos) {
                pos = close_end + 1;
            }
            break;
        }

        if (html[pos] == '<') {
            auto child = parse_element(html, pos);
            if (child) elem->add_child(child);
        } else {
            // Text node
            size_t next_lt = html.find('<', pos);
            std::string text;
            if (next_lt == std::string_view::npos) {
                text = std::string(html.substr(pos));
                pos = html.size();
            } else {
                text = std::string(html.substr(pos, next_lt - pos));
                pos = next_lt;
            }
            std::string trimmed = str_util::trim(text);
            if (!trimmed.empty()) {
                if (elem->text_content.empty()) {
                    elem->text_content = trimmed;
                } else {
                    elem->text_content += " " + trimmed;
                }
            }
        }
    }

    return elem;
}

std::shared_ptr<UIElement> DOMParser::parse(std::string_view html) {
    size_t pos = 0;
    skip_ws(html, pos);
    return parse_element(html, pos);
}

} // namespace aswell
