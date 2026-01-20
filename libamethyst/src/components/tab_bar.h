// container for other containers
// it will provide options to select 1 of the n containers that are children

#ifndef AMETHYST__TAB_BAR_H
#define AMETHYST__TAB_BAR_H

#include "components/image_button.h"
#include "components/text_button.h"
#include "components/ui_object.h"
#include "rendering/draw_context.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

struct TabItem {
    std::unique_ptr<TextButton> button;
    std::unique_ptr<ImageButton> closeBtn;
    Instance *content;
};

enum class TabBarMode {
    INSIDE,
    OUTSIDE
};

enum class TabBarVisibility {
    AUTO, // hides when only 1 item is present
    NEVER,
    ALWAYS
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
    TabBar(Instance *parent);
    virtual ~TabBar();

    void draw(DrawContext &ctx) override;
    void addChild(Instance *child) override;
    void removeChild(Instance *child) override;
    std::vector<Instance *> getHittableInstances() override;

  private:
    void formatChildren();
    int32_t findTabIndex(TabItem *item) const;
    int32_t computeTargetIndex(float dragPosition) const;

  public:
    bool closeable = false; /// Whether a close button per tab item will be rendered
    TabBarMode mode = TabBarMode::INSIDE;
    TabBarPosition tabPosition = TabBarPosition::TOP;
    float tabThickness = 30.0f; /// The width/height (depending on position) of the tab bar itself
    float tabWidth = 100.0f;
    TabBarVisibility visibility = TabBarVisibility::ALWAYS;
    int32_t selectedIndex = 0;

    std::function<void(Instance *content)> onTabTornOff;
    std::function<void(Instance *content, glm::vec2 pos)> onTornOffTabMoved;
    std::function<void(Instance *content, glm::vec2 dropPos)> onTornOffTabReleased;

  private:
    std::vector<std::unique_ptr<TabItem>> m_tabItems;
    TabItem *m_draggedTab = nullptr;
    bool m_tabTornOff = false;
};

} // namespace Amethyst

#endif // AMETHYST__TAB_BAR_H
