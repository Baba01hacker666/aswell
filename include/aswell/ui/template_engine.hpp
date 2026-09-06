#pragma once

#include "aswell/common.hpp"
#include "aswell/ui/color.hpp"
#include <map>
#include <vector>
#include <string>
#include <memory>

namespace aswell {

struct TemplateContext {
    std::map<std::string, std::string> vars;
    std::map<std::string, double> num_vars;

    void set(const std::string& key, const std::string& val) {
        vars[key] = val;
    }

    void set_num(const std::string& key, double val) {
        num_vars[key] = val;
        vars[key] = std::to_string(val);
    }

    std::string get(const std::string& key, const std::string& fallback = "") const {
        auto it = vars.find(key);
        if (it != vars.end()) return it->second;
        return fallback;
    }

    double get_num(const std::string& key, double fallback = 0.0) const {
        auto it = num_vars.find(key);
        if (it != num_vars.end()) return it->second;
        auto sit = vars.find(key);
        if (sit != vars.end()) {
            try {
                return std::stod(sit->second);
            } catch (...) {}
        }
        return fallback;
    }

    bool has(const std::string& key) const {
        return vars.find(key) != vars.end() || num_vars.find(key) != num_vars.end();
    }
};

class TemplateEngine {
public:
    // Render a template markup string using the given context
    static std::string render(const std::string& template_str, TemplateContext& ctx);

    // Render a template file from disk
    static std::string render_file(const std::string& file_path, TemplateContext& ctx);

    // Play an animation defined in a template (<animation frames="..." delay="...">)
    static void play_animation(const std::string& template_str, TemplateContext& ctx, bool force_headless = false);

    // Execute event hooks defined in ~/.config/aswell/templates/events.html
    static bool handle_event(const std::string& cmd_line, bool is_interactive = true);

    // Expression & math evaluation engine
    static double eval_math(const std::string& expr, const TemplateContext& ctx);
    static std::string eval_expr(const std::string& expr, const TemplateContext& ctx);

    // Template directories & defaults
    static std::string get_templates_dir();
    static void ensure_default_templates();
};

} // namespace aswell
