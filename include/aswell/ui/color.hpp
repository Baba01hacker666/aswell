#pragma once

#include "aswell/common.hpp"

namespace aswell {

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    bool is_none = true;

    Color() = default;
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_), is_none(false) {}

    static Color from_hex(std::string_view hex);
    static Color from_name(std::string_view name);
    static Color parse(std::string_view str);

    std::string to_fg_ansi(bool truecolor = true) const;
    std::string to_bg_ansi(bool truecolor = true) const;

    Color blend(const Color& other, float t) const;
    Color adjust_brightness(float factor) const;

    bool operator==(const Color& o) const {
        if (is_none && o.is_none) return true;
        if (is_none != o.is_none) return false;
        return r == o.r && g == o.g && b == o.b;
    }
};

} // namespace aswell
