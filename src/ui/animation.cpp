#include "aswell/ui/animation.hpp"
#include <chrono>
#include <cmath>

namespace aswell {

uint64_t AnimationEngine::now_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

Color AnimationEngine::evaluate_color(const AnimationConfig& anim,
                                      const Color& base_color,
                                      uint64_t timestamp_ms) {
    if (anim.type == AnimationType::NONE) {
        return base_color;
    }

    int duration = anim.duration_ms > 0 ? anim.duration_ms : 1000;
    double progress = std::fmod(static_cast<double>(timestamp_ms), static_cast<double>(duration)) / duration;

    switch (anim.type) {
        case AnimationType::PULSE: {
            // Sine oscillation between 0.4 and 1.2 brightness
            double factor = 0.8 + 0.4 * std::sin(progress * 2.0 * M_PI);
            return base_color.adjust_brightness(static_cast<float>(factor));
        }
        case AnimationType::RAINBOW: {
            // HSV rainbow cycling
            double h = progress * 360.0;
            double s = 0.85;
            double v = 0.95;

            double c = v * s;
            double x = c * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
            double m = v - c;

            double r_ = 0, g_ = 0, b_ = 0;
            if (h < 60) { r_ = c; g_ = x; b_ = 0; }
            else if (h < 120) { r_ = x; g_ = c; b_ = 0; }
            else if (h < 180) { r_ = 0; g_ = c; b_ = x; }
            else if (h < 240) { r_ = 0; g_ = x; b_ = c; }
            else if (h < 300) { r_ = x; g_ = 0; b_ = c; }
            else { r_ = c; g_ = 0; b_ = x; }

            uint8_t r = static_cast<uint8_t>((r_ + m) * 255.0);
            uint8_t g = static_cast<uint8_t>((g_ + m) * 255.0);
            uint8_t b = static_cast<uint8_t>((b_ + m) * 255.0);
            return Color(r, g, b);
        }
        case AnimationType::FIRE: {
            // Flickering gradient between red (255, 60, 0), orange (255, 150, 0), and yellow (255, 230, 50)
            double flicker = (std::sin(progress * 4.0 * M_PI) + std::sin(progress * 9.0 * M_PI)) * 0.25 + 0.5;
            Color c1(255, 60, 20);
            Color c2(255, 220, 60);
            return c1.blend(c2, static_cast<float>(flicker));
        }
        case AnimationType::WAVE: {
            double wave = std::pow(std::sin(progress * M_PI), 2.0);
            return base_color.adjust_brightness(static_cast<float>(0.6 + 0.6 * wave));
        }
        default:
            return base_color;
    }
}

std::string AnimationEngine::evaluate_glyph(const AnimationConfig& anim,
                                            const std::string& default_glyph,
                                            uint64_t timestamp_ms) {
    if (anim.type == AnimationType::SPIN) {
        static const std::vector<std::string> frames = {
            "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
        };
        uint64_t duration = anim.duration_ms > 0 ? static_cast<uint64_t>(anim.duration_ms) : static_cast<uint64_t>(800);
        uint64_t step = duration / frames.size();
        if (step == 0) step = 1;
        size_t frame_idx = static_cast<size_t>((timestamp_ms / step) % frames.size());
        return frames[frame_idx];
    }
    return default_glyph;
}

} // namespace aswell
