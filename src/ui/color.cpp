#include "aswell/ui/color.hpp"

namespace aswell {

Color Color::from_hex(std::string_view hex) {
    if (hex.empty()) return Color();
    if (hex[0] == '#') hex = hex.substr(1);

    if (hex.size() == 3) {
        // #rgb -> #rrggbb
        int r = std::stoi(std::string(2, hex[0]), nullptr, 16);
        int g = std::stoi(std::string(2, hex[1]), nullptr, 16);
        int b = std::stoi(std::string(2, hex[2]), nullptr, 16);
        return Color(r, g, b);
    } else if (hex.size() >= 6) {
        int r = std::stoi(std::string(hex.substr(0, 2)), nullptr, 16);
        int g = std::stoi(std::string(hex.substr(2, 2)), nullptr, 16);
        int b = std::stoi(std::string(hex.substr(4, 2)), nullptr, 16);
        return Color(r, g, b);
    }
    return Color();
}

Color Color::from_name(std::string_view name) {
    std::string lower = str_util::to_lower(name);

    // Standard CSS / Terminal colors
    static const std::unordered_map<std::string, Color> color_table = {
        {"black", Color(0, 0, 0)},
        {"red", Color(255, 85, 85)},
        {"green", Color(80, 250, 123)},
        {"yellow", Color(241, 250, 140)},
        {"blue", Color(98, 114, 164)},
        {"magenta", Color(255, 121, 198)},
        {"purple", Color(189, 147, 249)},
        {"cyan", Color(139, 233, 253)},
        {"white", Color(248, 248, 242)},
        {"gray", Color(98, 114, 164)},
        {"grey", Color(98, 114, 164)},
        {"transparent", Color()},
        {"none", Color()},
        {"orange", Color(255, 184, 108)},
        {"pink", Color(255, 121, 198)},

        // Nord palette
        {"nord0", Color(46, 52, 64)},
        {"nord1", Color(59, 66, 82)},
        {"nord2", Color(67, 76, 94)},
        {"nord3", Color(76, 86, 106)},
        {"nord4", Color(216, 222, 233)},
        {"nord5", Color(229, 233, 240)},
        {"nord6", Color(236, 239, 244)},
        {"nord7", Color(143, 188, 187)},
        {"nord8", Color(136, 192, 208)},
        {"nord9", Color(129, 161, 193)},
        {"nord10", Color(94, 129, 172)},
        {"nord11", Color(191, 97, 106)},
        {"nord12", Color(208, 135, 112)},
        {"nord13", Color(235, 203, 139)},
        {"nord14", Color(163, 190, 140)},
        {"nord15", Color(180, 142, 173)},

        // Cyberpunk palette
        {"neon-cyan", Color(0, 240, 255)},
        {"neon-yellow", Color(254, 231, 21)},
        {"neon-pink", Color(255, 0, 128)},
        {"cyber-blue", Color(0, 102, 255)},
        {"cyber-bg", Color(13, 2, 33)},

        // Dracula palette
        {"dracula-bg", Color(40, 42, 54)},
        {"dracula-fg", Color(248, 248, 242)},
        {"dracula-selection", Color(68, 71, 90)},
        {"dracula-comment", Color(98, 114, 164)},
        {"dracula-cyan", Color(139, 233, 253)},
        {"dracula-green", Color(80, 250, 123)},
        {"dracula-orange", Color(255, 184, 108)},
        {"dracula-pink", Color(255, 121, 198)},
        {"dracula-purple", Color(189, 147, 249)},
        {"dracula-red", Color(255, 85, 85)},
        {"dracula-yellow", Color(241, 250, 140)}
    };

    auto it = color_table.find(lower);
    if (it != color_table.end()) {
        return it->second;
    }
    return Color();
}

Color Color::parse(std::string_view str) {
    str = str_util::trim_sv(str);
    if (str.empty()) return Color();
    if (str[0] == '#') {
        return from_hex(str);
    }
    if (str_util::starts_with(str, "rgb(") && str.back() == ')') {
        std::string inner = std::string(str.substr(4, str.size() - 5));
        auto parts = str_util::split(inner, ',');
        if (parts.size() >= 3) {
            try {
                int r = std::stoi(str_util::trim(parts[0]));
                int g = std::stoi(str_util::trim(parts[1]));
                int b = std::stoi(str_util::trim(parts[2]));
                return Color(r, g, b);
            } catch (...) {}
        }
    }
    return from_name(str);
}

std::string Color::to_fg_ansi(bool truecolor) const {
    if (is_none) return "";
    if (truecolor) {
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    } else {
        // Fallback to 16/256 ANSI
        int code = 30;
        if (r > 128 && g < 128 && b < 128) code = 31;
        else if (r < 128 && g > 128 && b < 128) code = 32;
        else if (r > 128 && g > 128 && b < 128) code = 33;
        else if (r < 128 && g < 128 && b > 128) code = 34;
        else if (r > 128 && g < 128 && b > 128) code = 35;
        else if (r < 128 && g > 128 && b > 128) code = 36;
        else if (r > 128 && g > 128 && b > 128) code = 37;
        return "\033[" + std::to_string(code) + "m";
    }
}

std::string Color::to_bg_ansi(bool truecolor) const {
    if (is_none) return "";
    if (truecolor) {
        return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    } else {
        int code = 40;
        if (r > 128 && g < 128 && b < 128) code = 41;
        else if (r < 128 && g > 128 && b < 128) code = 42;
        else if (r > 128 && g > 128 && b < 128) code = 43;
        else if (r < 128 && g < 128 && b > 128) code = 44;
        else if (r > 128 && g < 128 && b > 128) code = 45;
        else if (r < 128 && g > 128 && b > 128) code = 46;
        else if (r > 128 && g > 128 && b > 128) code = 47;
        return "\033[" + std::to_string(code) + "m";
    }
}

Color Color::blend(const Color& other, float t) const {
    if (is_none) return other;
    if (other.is_none) return *this;
    t = std::clamp(t, 0.0f, 1.0f);
    uint8_t nr = static_cast<uint8_t>(r + (other.r - r) * t);
    uint8_t ng = static_cast<uint8_t>(g + (other.g - g) * t);
    uint8_t nb = static_cast<uint8_t>(b + (other.b - b) * t);
    return Color(nr, ng, nb);
}

Color Color::adjust_brightness(float factor) const {
    if (is_none) return *this;
    uint8_t nr = static_cast<uint8_t>(std::clamp(r * factor, 0.0f, 255.0f));
    uint8_t ng = static_cast<uint8_t>(std::clamp(g * factor, 0.0f, 255.0f));
    uint8_t nb = static_cast<uint8_t>(std::clamp(b * factor, 0.0f, 255.0f));
    return Color(nr, ng, nb);
}

} // namespace aswell
