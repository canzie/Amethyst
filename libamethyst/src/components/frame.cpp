/*
 * Frame implementation
 */

#include "components/frame.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/ui_object.h"
#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

static void applyStyle(Frame &frame)
{
    const auto &style = Style::instance();
    frame.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::FRAME);
    frame.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::FRAME);
    frame.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::FRAME);
    frame.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::FRAME);
    frame.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::FRAME);
    frame.borderMode = style.get<BorderMode>(StyleProperty::BORDER_MODE, ComponentType::FRAME);
    frame.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::FRAME);
}

Frame::Frame()
{
    applyStyle(*this);
}

void Frame::onMouseButton1Click()
{
    AM_LOG_INFO("Frame '{}' clicked (button 1)", name.empty() ? "(unnamed)" : name);
}

void Frame::onMouseButton2Click()
{
    AM_LOG_INFO("Frame '{}' clicked (button 2)", name.empty() ? "(unnamed)" : name);
}

void Frame::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    if (auto *gridLayout = getExtension<UIGridLayout>()) {
        gridLayout->apply(m_children);
    } else if (auto *listLayout = getExtension<UIListLayout>()) {
        listLayout->apply(m_children);
    }

    glm::vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
