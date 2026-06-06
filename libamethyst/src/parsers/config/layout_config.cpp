#include "parsers/config/layout_config.h"
#include "parsers/config/layout_config_parser.h"

namespace Amethyst {

LayoutConfig &LayoutConfig::instance()
{
    static LayoutConfig s_instance;
    return s_instance;
}

bool LayoutConfig::loadFromFile(const std::filesystem::path &path)
{
    m_loadedPath = path;
    return LayoutConfigParser::read(path, m_entries);
}

bool LayoutConfig::save() const
{
    if (m_loadedPath.empty()) {
        return false;
    }
    return saveToFile(m_loadedPath);
}

bool LayoutConfig::saveToFile(const std::filesystem::path &path) const
{
    return LayoutConfigParser::write(path, m_entries);
}

ConfigEntry *LayoutConfig::get(const std::string &name)
{
    auto it = m_entries.find(name);
    if (it == m_entries.end()) {
        return nullptr;
    }
    return &it->second;
}

const ConfigEntry *LayoutConfig::get(const std::string &name) const
{
    auto it = m_entries.find(name);
    if (it == m_entries.end()) {
        return nullptr;
    }
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
