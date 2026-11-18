#include "components/overlay_layer.h"
#include "components/ui_object.h"
#include "rendering/draw_context.h"

namespace Amethyst {

OverlayLayer::OverlayLayer() = default;

OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::draw(DrawContext &ctx)
{
    // first: nodes whose order moved are marked dirty here, before the check below
    ensureDrawOrder();
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    DrawContext layerCtx = ctx;
    layerCtx.geometry = geometryRegistry();

    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->draw(layerCtx);
        } else if (auto *layer = child->asLayer()) {
            layer->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
