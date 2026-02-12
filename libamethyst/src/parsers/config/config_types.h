#ifndef AMETHYST__CONFIG_TYPES_H
#define AMETHYST__CONFIG_TYPES_H

#include <string>
#include <vector>

namespace Amethyst {

enum class ConfigType {
    DOCK_LAYOUT,
    TAB_BAR
};

struct DockLayoutConfig {
    std::vector<std::string> axes;
    std::vector<float> ratios;
    std::vector<std::string> selectedTabs;
};

struct TabBarConfig {
    std::string selectedTab;
};

} // namespace Amethyst

#endif // AMETHYST__CONFIG_TYPES_H
