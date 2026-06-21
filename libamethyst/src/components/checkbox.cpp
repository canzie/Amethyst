#include "components/checkbox.h"

#include "amethyst/icons.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

Checkbox::Checkbox()
{
    m_checkIcon = add<ImageLabel>();
    m_checkIcon->setSvg(Icons::CHECK);
    m_checkIcon->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
    m_checkIcon->setBaseProperties({.interactable = false, .size = UDim2::fromScale(1.0f, 1.0f)});

    resolveStyle();
}

void Checkbox::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::CHECKBOX, getClasses()));
    setCheckboxProperties(style.getCheckboxStyle(ComponentType::CHECKBOX, getClasses()));
}

bool Checkbox::setCheckboxProperties(const CheckboxStyleProperties &props)
{
    bool changed = m_cbProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void Checkbox::setCheckIcon(std::string svg)
{
    m_checkIcon->setSvg(std::move(svg));
}

void Checkbox::draw(DrawContext &ctx)
{
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

    bool checked = value != nullptr && *value;
    m_checkIcon->setImageStyleProperties({.imageColor = m_cbProps.checkColor});
    m_checkIcon->setBaseProperties({.visible = checked});
    m_checkIcon->clipRect = computeChildClipRect();
    m_checkIcon->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
    m_checkIcon->draw(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

EventResult Checkbox::onMouseButton1Click()
{
    if (value != nullptr) {
        *value = !(*value);
        markDirty();
    }
    if (onValueChanged) {
        onValueChanged(value != nullptr ? *value : false);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
