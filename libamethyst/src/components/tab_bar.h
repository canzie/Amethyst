#ifndef AMETHYST__TAB_BAR_H
#define AMETHYST__TAB_BAR_H

#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/properties.h"
#include "components/text_button.h"
#include "components/ui_object.h"
#include "parsers/config/config_types.h"
#include "rendering/draw_context.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace Amethyst {

class TextLabel;

class TabBar : public UIObject {
  public:
    TabBar();
    ~TabBar() override = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

    /** @brief Not allowed. Use addTab() to add tabs. */
    Instance *addChild(std::unique_ptr<Instance> child) override;
    /** @brief Not allowed. Use removeTab() to remove tabs. */
    std::unique_ptr<Instance> removeChild(Instance *child) override;

    /**
     * @brief Add a tab with a string label.
     * @param content The content instance owned by this tab.
     * @param label The text shown on the tab button.
     * @return Raw pointer to the content instance.
     */
    Instance *addTab(std::unique_ptr<Instance> content, std::string_view label);

    /**
     * @brief Add a tab with a custom label frame.
     * @param content The content instance owned by this tab.
     * @param labelSetup Callback to populate the label Frame.
     * @return Raw pointer to the content instance.
     */
    Instance *addTab(std::unique_ptr<Instance> content, std::function<void(Frame &)> labelSetup);

    /**
     * @brief Remove the tab whose content matches the given pointer.
     * @param content Pointer to the content instance to remove.
     * @return The removed content instance.
     */
    std::unique_ptr<Instance> removeTab(Instance *content);

    void select(int32_t index);
    void select(Instance *content);
    Instance *getSelectedContent() const;
    int32_t getTabCount() const { return static_cast<int32_t>(m_tabs.size()); }

    TabBarConfig saveConfig() const;
    void applyConfig(const TabBarConfig &config);

    bool setTabBarProperties(const TabBarProperties &props);
    const TabBarProperties &getTabBarProperties() const { return m_tbProps; }

    std::function<void(Instance *content)> onTabClosed;
    std::function<void(Instance *content)> onTabTornOff;
    std::function<void(Instance *content, glm::vec2 pos)> onTornOffTabMoved;
    std::function<void(Instance *content, glm::vec2 dropPos)> onTornOffTabReleased;
    std::function<void(int32_t index)> onSelectionChanged;

  protected:
    TabBarProperties m_tbProps;

  private:
    struct Tab {
        std::unique_ptr<Frame> labelFrame;
        std::unique_ptr<InvisibleButton> button;
        TextLabel *label = nullptr;
        TextButton *closeButton = nullptr;
        std::unique_ptr<Instance> content;
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
