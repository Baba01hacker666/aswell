#pragma once

#include "aswell/common.hpp"

namespace aswell {

struct ShellConfig {
    std::string theme_name = "modern";
    bool enable_animation = true;
    bool enable_syntax_highlighting = true;
    bool enable_autosuggestions = true;
    bool show_username = true;
    bool show_hostname = true;
    bool show_directory = true;
    bool show_git = true;
    bool show_runtime = true;
    bool show_jobs = true;
    bool show_status = true;
    bool vi_mode = false;
    std::string custom_template_html;
};

class ConfigManager {
public:
    static std::string get_config_dir();
    static ShellConfig load();
    static void save(const ShellConfig& cfg);
    static std::string build_template(const ShellConfig& cfg);
};

} // namespace aswell
