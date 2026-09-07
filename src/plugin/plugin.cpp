#include <iomanip>
#include "aswell/plugin/plugin.hpp"
#include <dirent.h>
#include <fstream>

namespace aswell {

// Builtin Git Enhancer Plugin
class GitPlugin : public Plugin {
public:
    PluginMetadata get_metadata() const override {
        return {"git_info", "1.0.0", "Fast Git branch and repository inspector", "Aswell Team", true};
    }

    void on_load(Environment& /*env*/, HookManager& hooks) override {
        hooks.register_hook(HookType::ON_DIR_CHANGE, [](const std::vector<std::string>& args) {
            // Update git cache if needed
            (void)args;
        });
    }

    void on_unload(Environment& /*env*/) override {}
};

// Builtin Timer Plugin
class TimerPlugin : public Plugin {
public:
    PluginMetadata get_metadata() const override {
        return {"timer", "1.0.0", "Execution duration tracker and notifier", "Aswell Team", true};
    }

    void on_load(Environment& env, HookManager& hooks) override {
        hooks.register_hook(HookType::AFTER_COMMAND, [&env](const std::vector<std::string>& args) {
            if (args.size() >= 2) {
                try {
                    double ms = std::stod(args[1]);
                    if (ms > 5000.0) {
                        env.set_var("LAST_CMD_LONG", "1");
                    }
                } catch (...) {}
            }
        });
    }

    void on_unload(Environment& /*env*/) override {}
};

// Builtin System Info Plugin
class SysInfoPlugin : public Plugin {
public:
    PluginMetadata get_metadata() const override {
        return {"sys_info", "1.0.0", "Lightweight system resources status", "Aswell Team", true};
    }

    void on_load(Environment& env, HookManager& hooks) override {
        hooks.register_hook(HookType::ON_PROMPT, [&env](const std::vector<std::string>&) {
            double load_val = 0.0;
            bool ok = false;
            std::ifstream f("/proc/loadavg");
            if (f && f >> load_val) {
                ok = true;
            }
#if !defined(__ANDROID__)
            if (!ok) {
                double load[3];
                if (getloadavg(load, 1) > 0) {
                    load_val = load[0];
                    ok = true;
                }
            }
#endif
            if (ok) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << load_val;
                env.set_var("SYS_LOAD", oss.str());
            }
        });
    }

    void on_unload(Environment& /*env*/) override {}
};

PluginManager::PluginManager(Environment& env, HookManager& hooks)
    : env_(env), hooks_(hooks) {
    load_builtins();
}

void PluginManager::register_plugin(std::shared_ptr<Plugin> plugin) {
    if (plugin) {
        plugin->on_load(env_, hooks_);
        plugins_.push_back(plugin);
    }
}

void PluginManager::load_builtins() {
    register_plugin(std::make_shared<GitPlugin>());
    register_plugin(std::make_shared<TimerPlugin>());
    register_plugin(std::make_shared<SysInfoPlugin>());
}

void PluginManager::load_plugins_from_directory(const std::string& dir_path, bool safe_mode) {
    if (safe_mode) return; // Do not load external scripts in safe mode

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string fname = ent->d_name;
        if (str_util::ends_with(fname, ".sh") || str_util::ends_with(fname, ".aswell")) {
            std::string full_path = dir_path + "/" + fname;
            std::ifstream file(full_path);
            if (file && script_runner_) {
                std::stringstream ss;
                ss << file.rdbuf();
                script_runner_(ss.str());
            }
        }
    }
    closedir(dir);
}

} // namespace aswell
