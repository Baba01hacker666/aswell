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

    static std::string evaluate_text(const AnimationConfig& anim,
                                     const std::string& base_text,
                                     uint64_t timestamp_ms);

    static Color evaluate_wave_bg(uint64_t timestamp_ms,
                                  size_t char_idx,
                                  size_t total_chars,
                                  float speed = 1.0f);

    static uint64_t now_ms();
};

} // namespace aswell
