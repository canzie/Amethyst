#include "parsers/config/layout_config_parser.h"
#include "logging/log.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

static std::string s_quote(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::vector<std::string> s_tokenize(std::string_view line)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        if (line[i] == '"') {
            ++i;
            std::string val;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    ++i;
                }
                val.push_back(line[i]);
                ++i;
            }
            if (i < line.size()) {
                ++i;
            }
            out.push_back(std::move(val));
        } else {
            size_t start = i;
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            out.emplace_back(line.substr(start, i - start));
        }
    }
    return out;
}

static void s_writeNode(std::ofstream &file, const DockNodeConfig &node)
{
    if (node.isLeaf()) {
        file << "leaf " << s_quote(node.selected);
        for (const auto &panel : node.panels) {
            file << ' ' << s_quote(panel);
        }
        file << '\n';
    } else {
        file << "branch " << s_quote(node.axis) << ' ' << node.ratio << ' ' << node.first << ' ' << node.second << '\n';
    }
}

bool LayoutConfigParser::read(const std::filesystem::path &path, std::unordered_map<std::string, ConfigEntry> &outEntries)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    outEntries.clear();

    std::string name;
    DockLayoutConfig dock;
    TabBarConfig tabBar;
    ConfigType current = ConfigType::DOCK_LAYOUT;
    bool haveEntry = false;

    auto commit = [&]() {
        if (!haveEntry) {
            return;
        }
        if (current == ConfigType::DOCK_LAYOUT) {
            outEntries.emplace(name, ConfigEntry(dock));
        } else {
            outEntries.emplace(name, ConfigEntry(tabBar));
        }
    };

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> tokens = s_tokenize(line);
        if (tokens.empty() || tokens[0].empty() || tokens[0][0] == '#') {
            continue;
        }
        const std::string &kind = tokens[0];

        if (kind == "dock") {
            commit();
            name = tokens.size() > 1 ? tokens[1] : std::string();
            dock = DockLayoutConfig{};
            current = ConfigType::DOCK_LAYOUT;
            haveEntry = true;
        } else if (kind == "tabbar") {
            commit();
            name = tokens.size() > 1 ? tokens[1] : std::string();
            tabBar = TabBarConfig{};
            current = ConfigType::TAB_BAR;
            haveEntry = true;
        } else if (kind == "branch" && current == ConfigType::DOCK_LAYOUT) {
            DockNodeConfig node;
            node.axis = tokens.size() > 1 ? tokens[1] : std::string();
            node.ratio = tokens.size() > 2 ? std::strtof(tokens[2].c_str(), nullptr) : 0.5f;
            node.first = tokens.size() > 3 ? static_cast<int32_t>(std::strtol(tokens[3].c_str(), nullptr, 10)) : -1;
            node.second = tokens.size() > 4 ? static_cast<int32_t>(std::strtol(tokens[4].c_str(), nullptr, 10)) : -1;
            dock.nodes.push_back(std::move(node));
        } else if (kind == "leaf" && current == ConfigType::DOCK_LAYOUT) {
            DockNodeConfig node;
            node.selected = tokens.size() > 1 ? tokens[1] : std::string();
            for (size_t i = 2; i < tokens.size(); ++i) {
                node.panels.push_back(tokens[i]);
            }
            dock.nodes.push_back(std::move(node));
        } else if (kind == "selected" && current == ConfigType::TAB_BAR) {
            tabBar.selectedTab = tokens.size() > 1 ? tokens[1] : std::string();
        } else {
            AM_LOG_WARN("Unrecognized layout config line: {}", line);
        }
    }
    commit();

    return true;
}

bool LayoutConfigParser::write(const std::filesystem::path &path, const std::unordered_map<std::string, ConfigEntry> &entries)
{
    std::ofstream file(path);
    if (!file.is_open()) {
        AM_LOG_ERROR("Failed to write layout config to {}", path.string());
        return false;
    }

    file << "# Amethyst layout config\n";
    for (const auto &[name, entry] : entries) {
        switch (entry.type) {
        case ConfigType::DOCK_LAYOUT:
            file << "dock " << s_quote(name) << '\n';
            for (const auto &node : entry.dockLayout.nodes) {
                s_writeNode(file, node);
            }
            break;
        case ConfigType::TAB_BAR:
            file << "tabbar " << s_quote(name) << '\n';
            file << "selected " << s_quote(entry.tabBar.selectedTab) << '\n';
            break;
        }
    }

    return file.good();
}

} // namespace Amethyst
