#include <gtest/gtest.h>

#include "parsers/config/layout_config.h"

#include <cmath>
#include <filesystem>

using namespace Amethyst;

static std::filesystem::path s_tempPath()
{
    return std::filesystem::temp_directory_path() / "amethyst_layout_config_test.conf";
}

TEST(LayoutConfig, RoundTripDockLayout)
{
    DockLayoutConfig dock;
    DockNodeConfig root;
    root.axis = "vertical";
    root.ratio = 0.3f;
    root.first = 1;
    root.second = 2;

    DockNodeConfig leaf1;
    leaf1.panels = {"inspector", "scene"};
    leaf1.selected = "inspector";

    DockNodeConfig leaf2;
    leaf2.panels = {"console"};
    leaf2.selected = "console";

    dock.nodes = {root, leaf1, leaf2};

    LayoutConfig cfg;
    cfg.set("editor", ConfigEntry(dock));

    auto path = s_tempPath();
    ASSERT_TRUE(cfg.saveToFile(path));

    LayoutConfig fresh;
    ASSERT_TRUE(fresh.loadFromFile(path));

    auto *entry = fresh.get("editor");
    ASSERT_TRUE(entry != nullptr);
    ASSERT_TRUE(entry->type == ConfigType::DOCK_LAYOUT);
    ASSERT_EQ(entry->dockLayout.nodes.size(), 3u);

    auto &n0 = entry->dockLayout.nodes[0];
    ASSERT_EQ(n0.axis, "vertical");
    ASSERT_TRUE(std::abs(n0.ratio - 0.3f) < 0.001f);
    ASSERT_EQ(n0.first, 1);
    ASSERT_EQ(n0.second, 2);

    auto &n1 = entry->dockLayout.nodes[1];
    ASSERT_TRUE(n1.isLeaf());
    ASSERT_EQ(n1.panels.size(), 2u);
    ASSERT_EQ(n1.panels[0], "inspector");
    ASSERT_EQ(n1.panels[1], "scene");
    ASSERT_EQ(n1.selected, "inspector");

    auto &n2 = entry->dockLayout.nodes[2];
    ASSERT_TRUE(n2.isLeaf());
    ASSERT_EQ(n2.panels.size(), 1u);
    ASSERT_EQ(n2.panels[0], "console");

    std::filesystem::remove(path);
}

TEST(LayoutConfig, RoundTripTabBar)
{
    TabBarConfig tab;
    tab.selectedTab = "settings";

    LayoutConfig cfg;
    cfg.set("mainTabs", ConfigEntry(tab));

    auto path = s_tempPath();
    ASSERT_TRUE(cfg.saveToFile(path));

    LayoutConfig fresh;
    ASSERT_TRUE(fresh.loadFromFile(path));

    auto *entry = fresh.get("mainTabs");
    ASSERT_TRUE(entry != nullptr);
    ASSERT_TRUE(entry->type == ConfigType::TAB_BAR);
    ASSERT_EQ(entry->tabBar.selectedTab, "settings");

    std::filesystem::remove(path);
}

TEST(LayoutConfig, MissingFileReturnsFalse)
{
    LayoutConfig cfg;
    ASSERT_FALSE(cfg.loadFromFile("/nonexistent/path/layout.conf"));
}

TEST(LayoutConfig, SaveWithoutLoadReturnsFalse)
{
    LayoutConfig cfg;
    ASSERT_FALSE(cfg.save());
}

TEST(LayoutConfig, UnknownEntryReturnsNull)
{
    LayoutConfig cfg;
    ASSERT_EQ(cfg.get("nonexistent"), nullptr);
}

TEST(LayoutConfig, OverwriteEntry)
{
    TabBarConfig tab1;
    tab1.selectedTab = "first";

    TabBarConfig tab2;
    tab2.selectedTab = "second";

    LayoutConfig cfg;
    cfg.set("tabs", ConfigEntry(tab1));
    cfg.set("tabs", ConfigEntry(tab2));

    auto *entry = cfg.get("tabs");
    ASSERT_TRUE(entry != nullptr);
    ASSERT_EQ(entry->tabBar.selectedTab, "second");
}
