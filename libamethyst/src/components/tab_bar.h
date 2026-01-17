// container for other containers
// it will provide options to select 1 of the n containers that are children

#ifndef AMETHYST__TAB_BAR_H
#define AMETHYST__TAB_BAR_H

#include "components/frame.h"
#include "components/ui_object.h"
#include "rendering/draw_context.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

struct TabItem;

enum class TabBarMode {
    INSIDE,
    OUTSIDE
};

enum class TabBarVisibility {
    AUTO, // hides when only 1 item is present
    NEVER,
    ALWAYS
};

class TabBar : public UIObject {
  public:
    TabBar();
    virtual ~TabBar() = default;
    void draw(DrawContext &ctx) override;

  private:
    void formatChildren();

  public:
    bool closeable = false; /// Wheter a close button per tab item will be rendered
    TabBarMode mode = TabBarMode::OUTSIDE;
    uint32_t tabHeight = 30.0f; /// The width/height (depending on mode) of the tab bar itself
    TabBarVisibility visibility = TabBarVisibility::AUTO;

  private:
    std::vector<std::unique_ptr<TabItem>> m_tabItems;
    std::unique_ptr<Frame> m_tabBar;
};

} // namespace Amethyst

#endif // AMETHYST__TAB_BAR_H
