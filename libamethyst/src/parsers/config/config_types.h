#ifndef AMETHYST__CONFIG_TYPES_H
#define AMETHYST__CONFIG_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace Amethyst {

enum class ConfigType {
    DOCK_LAYOUT,
    TAB_BAR
};

struct DockNodeConfig {
    std::string axis;
    float ratio = 0.5f;
    int32_t first = -1;
    int32_t second = -1;
    std::vector<std::string> panels;
    std::string selected;

    bool isLeaf() const { return first < 0 && second < 0; }
};

struct DockLayoutConfig {
    std::vector<DockNodeConfig> nodes;
};

struct TabBarConfig {
    std::string selectedTab;
};

} // namespace Amethyst

#endif // AMETHYST__CONFIG_TYPES_H
