#include "parsers/config/layout_config.h"
#include "logging/log.h"

#include <fstream>
#include <toml++/toml.hpp>

namespace Amethyst {

static toml::table s_serializeNode(const DockNodeConfig &node)
{
    toml::table tbl;

    if (!node.isLeaf()) {
        tbl.insert("axis", node.axis);
        tbl.insert("ratio", static_cast<double>(node.ratio));
        tbl.insert("first", static_cast<int64_t>(node.first));
        tbl.insert("second", static_cast<int64_t>(node.second));
    } else {
        toml::array panels;
        for (const auto &p : node.panels) panels.push_back(p);
        tbl.insert("panels", std::move(panels));
        if (!node.selected.empty()) tbl.insert("selected", node.selected);
    }

    return tbl;
}

static toml::table s_serializeDockLayout(const DockLayoutConfig &cfg)
{
    toml::table tbl;

    toml::array nodes;
    for (const auto &node : cfg.nodes) nodes.push_back(s_serializeNode(node));
    tbl.insert("nodes", std::move(nodes));

    return tbl;
}

static toml::table s_serializeTabBar(const TabBarConfig &cfg)
{
    toml::table tbl;
    tbl.insert("selectedTab", cfg.selectedTab);
    return tbl;
}

static DockNodeConfig s_parseDockNodeConfig(const toml::table &tbl)
{
    DockNodeConfig node;

    if (auto a = tbl["axis"].as_string()) {
        node.axis = std::string(**a);
    }
    if (auto r = tbl["ratio"].as_floating_point()) {
        node.ratio = static_cast<float>(**r);
    } else if (auto ri = tbl["ratio"].as_integer()) {
        node.ratio = static_cast<float>(**ri);
    }
    if (auto f = tbl["first"].as_integer()) node.first = static_cast<int32_t>(**f);
    if (auto s = tbl["second"].as_integer()) node.second = static_cast<int32_t>(**s);

    if (auto panels = tbl["panels"].as_array()) {
        for (const auto &v : *panels) {
            if (auto p = v.as_string()) node.panels.push_back(std::string(**p));
        }
    }
    if (auto sel = tbl["selected"].as_string()) {
        node.selected = std::string(**sel);
    }

    return node;
}

static DockLayoutConfig s_parseDockLayout(const toml::table &tbl)
{
    DockLayoutConfig cfg;

    if (auto nodes = tbl["nodes"].as_array()) {
        for (const auto &v : *nodes) {
            if (auto nodeTbl = v.as_table()) {
                cfg.nodes.push_back(s_parseDockNodeConfig(*nodeTbl));
            }
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
