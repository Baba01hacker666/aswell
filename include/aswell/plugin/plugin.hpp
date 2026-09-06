#pragma once

#include "aswell/common.hpp"
#include "aswell/plugin/hooks.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

struct PluginMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    bool enabled = true;
};

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual PluginMetadata get_metadata() const = 0;
    virtual void on_load(Environment& env, HookManager& hooks) = 0;
    virtual void on_unload(Environment& env) = 0;
};

class PluginManager {
public:
    explicit PluginManager(Environment& env, HookManager& hooks);

    void load_builtins();
    void load_plugins_from_directory(const std::string& dir_path, bool safe_mode);
    void register_plugin(std::shared_ptr<Plugin> plugin);

    const std::vector<std::shared_ptr<Plugin>>& get_plugins() const { return plugins_; }

private:
    Environment& env_;
    HookManager& hooks_;
    std::vector<std::shared_ptr<Plugin>> plugins_;
};

} // namespace aswell
