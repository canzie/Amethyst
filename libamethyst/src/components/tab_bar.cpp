#include "tab_bar.h"

#include "components/common.h"
#include "components/image_button.h"
#include "components/text_label.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

struct TabItem {
    std::unique_ptr<Frame> frame;
    std::unique_ptr<TextLabel> title;
    std::unique_ptr<ImageButton> closeBtn;
    Instance *content;
};

TabBar::TabBar() {}

void TabBar::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    // NOTE: Maybe call this before the flag check, depends on if we will manually tell the tab we are gone, maybe if we are able to
    // use a custom remove child or something
    formatChildren();

    if (flags & FLAG_DIRTY) {
        size.scale.x = 1.0f;
        size.offset.x = 0.0f;

        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    // Update the tab items also???

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void TabBar::formatChildren()
{
    if (m_tabItems.size() == children.size()) return;

    // TODO: If childrens size is less, then remove the old ones
    //       if more add the new one and mark dirty

    // the contents sizes should be equal to the space available - tabHeight, and x should be scale 1.0f - padding
}

} // namespace Amethyst
