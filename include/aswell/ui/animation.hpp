#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/style.hpp"
#include "aswell/ui/color.hpp"

namespace aswell {

class AnimationEngine {
public:
    static Color evaluate_color(const AnimationConfig& anim,
                                const Color& base_color,
                                uint64_t timestamp_ms);

    static std::string evaluate_glyph(const AnimationConfig& anim,
                                      const std::string& default_glyph,
                                      uint64_t timestamp_ms);

    static uint64_t now_ms();
};

} // namespace aswell
