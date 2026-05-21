#ifndef AMETHYST__TAB_BAR_H
#define AMETHYST__TAB_BAR_H

#include "components/text_button.h"
#include "components/ui_object.h"
#include "modules/color.h"
#include "parsers/config/config_types.h"
#include "rendering/draw_context.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

enum class TabBarMode {
    INSIDE,
    OUTSIDE
};

enum class TabBarVisibility {
    AUTO,
    NEVER,
    ALWAYS
};

enum class TabCloseButtonVisibility {
    HIDDEN,
    ALWAYS,
    ACTIVE_ONLY,
    HOVERED_OR_ACTIVE,
};

enum class TabBarPosition {
    TOP,
    BOTTOM,
    LEFT,
    RIGHT
};

class TabBar : public UIObject {
  public:
    TabBar();
    ~TabBar() override = default;

    void draw(DrawContext &ctx) override;
    Instance *addChild(std::unique_ptr<Instance> child) override;
    std::unique_ptr<Instance> removeChild(Instance *child) override;
    std::vector<Instance *> getHittableInstances() override;

    void select(int32_t index);
    void select(Instance *content);
    Instance *getSelectedContent() const;
    int32_t getTabCount() const { return static_cast<int32_t>(m_tabs.size()); }

    TabBarConfig saveConfig() const;
    void applyConfig(const TabBarConfig &config);

  public:
    bool closeable = false;
    bool persistLayout = false;
    TabBarMode mode = TabBarMode::INSIDE;
    TabBarPosition tabPosition = TabBarPosition::TOP;
    TabBarVisibility visibility = TabBarVisibility::ALWAYS;
    float barThickness = 30.0f;
    float tabWidth = 100.0f;
    float tabSpacing = 0.0f;
    int32_t selectedIndex = 0;
    Color3 tabColor{};
    Color3 focussedTabColor{};
    Color3 hoveredTabColor{};
    Color3 pressedTabColor{};

    TabCloseButtonVisibility closeButtonVisibility = TabCloseButtonVisibility::HIDDEN;
    std::function<void(Instance *content)> onTabClosed;

    std::function<void(Instance *content)> onTabTornOff;
    std::function<void(Instance *content, glm::vec2 pos)> onTornOffTabMoved;
    std::function<void(Instance *content, glm::vec2 dropPos)> onTornOffTabReleased;
    std::function<void(int32_t index)> onSelectionChanged;

  private:
    struct Tab {
        std::unique_ptr<TextButton> button;
        Instance *content = nullptr;
        UIObject *closeButton = nullptr;
    };

    void setupTabButton(Tab &tab, int32_t index);
    void layoutTabs();
    void layoutContent();
    void markAllTabsDirty();

    bool isVertical() const;
    bool shouldShowTabs() const;
    float getBarSize() const;
    glm::vec2 getContentOffset() const;
    glm::vec2 getContentSizeAdjust() const;

    int32_t findTabIndex(const Tab *tab) const;
    int32_t indexFromPosition(float pos) const;

  private:
    std::vector<std::unique_ptr<Tab>> m_tabs;
    Tab *m_draggedTab = nullptr;
    bool m_tornOff = false;
    int32_t m_lastSelectedIndex = 0;
};

} // namespace Amethyst

#endif // AMETHYST__TAB_BAR_H
