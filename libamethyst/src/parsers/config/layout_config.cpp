#include "parsers/config/layout_config.h"
#include "logging/log.h"

#include <fstream>
#include <toml++/toml.hpp>

namespace Amethyst {

static toml::table s_serializeDockLayout(const DockLayoutConfig &cfg)
{
    toml::table tbl;

    toml::array axes;
    for (const auto &a : cfg.axes) axes.push_back(a);
    tbl.insert("axes", std::move(axes));

    toml::array ratios;
    for (float r : cfg.ratios) ratios.push_back(static_cast<double>(r));
    tbl.insert("ratios", std::move(ratios));

    toml::array tabs;
    for (const auto &t : cfg.selectedTabs) tabs.push_back(t);
    tbl.insert("selectedTabs", std::move(tabs));

    return tbl;
}

static toml::table s_serializeTabBar(const TabBarConfig &cfg)
{
    toml::table tbl;
    tbl.insert("selectedTab", cfg.selectedTab);
    return tbl;
}

static DockLayoutConfig s_parseDockLayout(const toml::table &tbl)
{
    DockLayoutConfig cfg;

    if (auto axes = tbl["axes"].as_array()) {
        for (const auto &v : *axes) {
            if (auto s = v.as_string()) cfg.axes.push_back(std::string(**s));
        }
    }

    if (auto ratios = tbl["ratios"].as_array()) {
        for (const auto &v : *ratios) {
            if (auto f = v.as_floating_point()) cfg.ratios.push_back(static_cast<float>(**f));
            else if (auto i = v.as_integer()) cfg.ratios.push_back(static_cast<float>(**i));
        }
    }

    if (auto tabs = tbl["selectedTabs"].as_array()) {
        for (const auto &v : *tabs) {
            if (auto s = v.as_string()) cfg.selectedTabs.push_back(std::string(**s));
        }
    }

    return cfg;
}

static TabBarConfig s_parseTabBar(const toml::table &tbl)
{
    TabBarConfig cfg;
    if (auto s = tbl["selectedTab"].as_string()) {
        cfg.selectedTab = std::string(**s);
    }
    return cfg;
}

LayoutConfig &LayoutConfig::instance()
{
    static LayoutConfig s_instance;
    return s_instance;
}

bool LayoutConfig::loadFromFile(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path)) return false;

    toml::parse_result parseResult = toml::parse_file(path.string());
    if (!parseResult) {
        AM_LOG_WARN("Failed to parse layout config from {}", path.string());
        return false;
    }

    toml::table &root = parseResult.table();
    m_entries.clear();

    if (auto dockSection = root["dock"].as_table()) {
        for (const auto &[key, value] : *dockSection) {
            if (auto tbl = value.as_table()) {
                m_entries.emplace(std::string(key.str()), ConfigEntry(s_parseDockLayout(*tbl)));
            }
        }
    }

    if (auto tabBarSection = root["tabbar"].as_table()) {
        for (const auto &[key, value] : *tabBarSection) {
            if (auto tbl = value.as_table()) {
                m_entries.emplace(std::string(key.str()), ConfigEntry(s_parseTabBar(*tbl)));
            }
        }
    }

    m_loadedPath = path;
    return true;
}

bool LayoutConfig::save() const
{
    if (m_loadedPath.empty()) return false;
    return saveToFile(m_loadedPath);
}

bool LayoutConfig::saveToFile(const std::filesystem::path &path) const
{
    toml::table root;
    toml::table dockSection;
    toml::table tabBarSection;

    for (const auto &[name, entry] : m_entries) {
        switch (entry.type) {
        case ConfigType::DOCK_LAYOUT:
            dockSection.insert(name, s_serializeDockLayout(entry.dockLayout));
            break;
        case ConfigType::TAB_BAR:
            tabBarSection.insert(name, s_serializeTabBar(entry.tabBar));
            break;
        }
    }

    if (!dockSection.empty()) root.insert("dock", std::move(dockSection));
    if (!tabBarSection.empty()) root.insert("tabbar", std::move(tabBarSection));

    std::ofstream file(path);
    if (!file.is_open()) {
        AM_LOG_ERROR("Failed to write layout config to {}", path.string());
        return false;
    }

    file << root;
    return file.good();
}

ConfigEntry *LayoutConfig::get(const std::string &name)
{
    auto it = m_entries.find(name);
    if (it == m_entries.end()) return nullptr;
    return &it->second;
}

const ConfigEntry *LayoutConfig::get(const std::string &name) const
{
    auto it = m_entries.find(name);
    if (it == m_entries.end()) return nullptr;
    return &it->second;
}

void LayoutConfig::set(const std::string &name, ConfigEntry entry)
{
    auto it = m_entries.find(name);
    if (it != m_entries.end()) {
        it->second = std::move(entry);
    } else {
        m_entries.emplace(name, std::move(entry));
    }
}

} // namespace Amethyst
