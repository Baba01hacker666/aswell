#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/color.hpp"

namespace drills {
}

namespace aswell {

enum class BorderType {
    NONE,
    SOLID,      // ┌─┐ │ └─┘
    ROUNDED,    // ╭─╮ │ ╰─╯
    DOUBLE,     // ╔═╗ ║ ╚═╝
    DASHED      // ┌╌┐ ┆ └╌┘
};

enum class DisplayType {
    INLINE,
    BLOCK,
    FLEX,
    NONE
};

enum class AnimationType {
    NONE,
    PULSE,
    RAINBOW,
    FIRE,
    SPIN,
    WAVE,
    SCRAMBLE,
    GLITCH
};

struct AnimationConfig {
    AnimationType type = AnimationType::NONE;
    int duration_ms = 1000;
    bool infinite = true;
};

struct BoxSpacing {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

struct Style {
    Color color;
    Color bg_color;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool dim = false;

    DisplayType display = DisplayType::INLINE;
    BorderType border_type = BorderType::NONE;
    Color border_color;
    BoxSpacing border_width;
    BoxSpacing padding;
    BoxSpacing margin;

    std::optional<std::string> content_override;
    AnimationConfig animation;

    void merge(const Style& other) {
        if (!other.color.is_none) color = other.color;
        if (!other.bg_color.is_none) bg_color = other.bg_color;
        if (other.bold) bold = true;
        if (other.italic) italic = true;
        if (other.underline) underline = true;
        if (other.strikethrough) strikethrough = true;
        if (other.dim) dim = true;

        if (other.display != DisplayType::INLINE) display = other.display;
        if (other.border_type != BorderType::NONE) border_type = other.border_type;
        if (!other.border_color.is_none) border_color = other.border_color;
        if (other.border_width.top || other.border_width.right || other.border_width.bottom || other.border_width.left) {
            border_width = other.border_width;
        }
        if (other.padding.top || other.padding.right || other.padding.bottom || other.padding.left) {
            padding = other.padding;
        }
        if (other.margin.top || other.margin.right || other.margin.bottom || other.margin.left) {
            margin = other.margin;
        }
        if (other.content_override.has_value()) {
            content_override = other.content_override;
        }
        if (other.animation.type != AnimationType::NONE) {
            animation = other.animation;
        }
    }
};

} // namespace aswell
