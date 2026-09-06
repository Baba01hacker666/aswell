#include "aswell/config/config.hpp"
#include <fstream>
#include <sys/stat.h>

namespace aswell {

std::string ConfigManager::get_config_dir() {
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/root";
    return base + "/.config/aswell";
}

std::string ConfigManager::build_template(const ShellConfig& cfg) {
    std::string html = "<prompt class=\"main\">\n  <segment class=\"top-line\">\n";
    html += "    <text class=\"bracket\">╭─</text>\n";

    if (cfg.show_username) html += "    <user />\n";
    if (cfg.show_username && cfg.show_hostname) html += "    <text class=\"at\">@</text>\n";
    if (cfg.show_hostname) html += "    <hostname />\n";
    if (cfg.show_directory) {
        if (cfg.show_username || cfg.show_hostname) html += "    <text class=\"sep\"> in </text>\n";
        html += "    <directory />\n";
    }
    if (cfg.show_git) html += "    <git />\n";
    if (cfg.show_runtime) html += "    <runtime />\n";
    if (cfg.show_status) html += "    <status />\n";
    if (cfg.show_jobs) html += "    <jobs />\n";

    html += "  </segment>\n  <newline />\n  <segment class=\"bottom-line\">\n";
    html += "    <text class=\"bracket\">╰─</text>\n    <mode />\n    <symbol />\n    <text> </text>\n";
    html += "  </segment>\n</prompt>\n";

    return html;
}

ShellConfig ConfigManager::load() {
    ShellConfig cfg;
    std::string dir = get_config_dir();
    std::string cfg_file = dir + "/config.txt";

    std::ifstream f(cfg_file);
    if (!f) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string k = str_util::trim(line.substr(0, eq));
        std::string v = str_util::trim(line.substr(eq + 1));

        if (k == "theme") cfg.theme_name = v;
        else if (k == "animation") cfg.enable_animation = (v == "true" || v == "1");
        else if (k == "syntax_highlighting") cfg.enable_syntax_highlighting = (v == "true" || v == "1");
        else if (k == "autosuggestions") cfg.enable_autosuggestions = (v == "true" || v == "1");
        else if (k == "show_username") cfg.show_username = (v == "true" || v == "1");
        else if (k == "show_hostname") cfg.show_hostname = (v == "true" || v == "1");
        else if (k == "show_directory") cfg.show_directory = (v == "true" || v == "1");
        else if (k == "show_git") cfg.show_git = (v == "true" || v == "1");
        else if (k == "show_runtime") cfg.show_runtime = (v == "true" || v == "1");
        else if (k == "show_jobs") cfg.show_jobs = (v == "true" || v == "1");
        else if (k == "show_status") cfg.show_status = (v == "true" || v == "1");
        else if (k == "vi_mode") cfg.vi_mode = (v == "true" || v == "1");
    }

    return cfg;
}

void ConfigManager::save(const ShellConfig& cfg) {
    std::string dir = get_config_dir();
    mkdir(dir.c_str(), 0755);
    mkdir((dir + "/themes").c_str(), 0755);
    mkdir((dir + "/plugins").c_str(), 0755);

    std::string cfg_file = dir + "/config.txt";
    std::ofstream f(cfg_file);
    if (!f) return;

    f << "theme=" << cfg.theme_name << "\n";
    f << "animation=" << (cfg.enable_animation ? "true" : "false") << "\n";
    f << "syntax_highlighting=" << (cfg.enable_syntax_highlighting ? "true" : "false") << "\n";
    f << "autosuggestions=" << (cfg.enable_autosuggestions ? "true" : "false") << "\n";
    f << "show_username=" << (cfg.show_username ? "true" : "false") << "\n";
    f << "show_hostname=" << (cfg.show_hostname ? "true" : "false") << "\n";
    f << "show_directory=" << (cfg.show_directory ? "true" : "false") << "\n";
    f << "show_git=" << (cfg.show_git ? "true" : "false") << "\n";
    f << "show_runtime=" << (cfg.show_runtime ? "true" : "false") << "\n";
    f << "show_jobs=" << (cfg.show_jobs ? "true" : "false") << "\n";
    f << "show_status=" << (cfg.show_status ? "true" : "false") << "\n";
    f << "vi_mode=" << (cfg.vi_mode ? "true" : "false") << "\n";
}

} // namespace aswell
