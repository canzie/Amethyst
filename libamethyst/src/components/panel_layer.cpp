#include "components/panel_layer.h"
#include "components/ui_object.h"
#include "rendering/draw_context.h"

namespace Amethyst {

PanelLayer::PanelLayer() = default;

PanelLayer::~PanelLayer() = default;

void PanelLayer::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    DrawContext layerCtx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.overlay = ctx.overlay;
    layerCtx.textProcessor = ctx.textProcessor;
    layerCtx.glyphAtlas = ctx.glyphAtlas;

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = clipRect;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(layerCtx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
