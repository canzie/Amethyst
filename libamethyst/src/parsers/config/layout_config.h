#ifndef AMETHYST__LAYOUT_CONFIG_H
#define AMETHYST__LAYOUT_CONFIG_H

#include "parsers/config/config_types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace Amethyst {

struct ConfigEntry {
    ConfigType type;

    union {
        DockLayoutConfig dockLayout;
        TabBarConfig tabBar;
    };

    explicit ConfigEntry(DockLayoutConfig cfg) : type(ConfigType::DOCK_LAYOUT), dockLayout(std::move(cfg)) {}

    explicit ConfigEntry(TabBarConfig cfg) : type(ConfigType::TAB_BAR), tabBar(std::move(cfg)) {}

    ~ConfigEntry() { destroy(); }

    ConfigEntry(const ConfigEntry &other) : type(other.type)
    {
        switch (type) {
        case ConfigType::DOCK_LAYOUT:
            std::construct_at(&dockLayout, other.dockLayout);
            break;
        case ConfigType::TAB_BAR:
            std::construct_at(&tabBar, other.tabBar);
            break;
        }
    }

    ConfigEntry(ConfigEntry &&other) noexcept : type(other.type)
    {
        switch (type) {
        case ConfigType::DOCK_LAYOUT:
            std::construct_at(&dockLayout, std::move(other.dockLayout));
            break;
        case ConfigType::TAB_BAR:
            std::construct_at(&tabBar, std::move(other.tabBar));
            break;
        }
    }

    ConfigEntry &operator=(const ConfigEntry &other)
    {
        if (this == &other) return *this;
        destroy();
        type = other.type;
        switch (type) {
        case ConfigType::DOCK_LAYOUT:
            std::construct_at(&dockLayout, other.dockLayout);
            break;
        case ConfigType::TAB_BAR:
            std::construct_at(&tabBar, other.tabBar);
            break;
        }
        return *this;
    }

    ConfigEntry &operator=(ConfigEntry &&other) noexcept
    {
        if (this == &other) return *this;
        destroy();
        type = other.type;
        switch (type) {
        case ConfigType::DOCK_LAYOUT:
            std::construct_at(&dockLayout, std::move(other.dockLayout));
            break;
        case ConfigType::TAB_BAR:
            std::construct_at(&tabBar, std::move(other.tabBar));
            break;
        }
        return *this;
    }

  private:
    void destroy()
    {
        switch (type) {
        case ConfigType::DOCK_LAYOUT:
            std::destroy_at(&dockLayout);
            break;
        case ConfigType::TAB_BAR:
            std::destroy_at(&tabBar);
            break;
        }
    }
};

class LayoutConfig {
  public:
    static LayoutConfig &instance();

    bool loadFromFile(const std::filesystem::path &path);
    bool save() const;
    bool saveToFile(const std::filesystem::path &path) const;

    ConfigEntry *get(const std::string &name);
    const ConfigEntry *get(const std::string &name) const;
    void set(const std::string &name, ConfigEntry entry);

    LayoutConfig() = default;
    ~LayoutConfig() { save(); }

  private:
    std::unordered_map<std::string, ConfigEntry> m_entries;
    std::filesystem::path m_loadedPath;
};

} // namespace Amethyst

#endif // AMETHYST__LAYOUT_CONFIG_H
