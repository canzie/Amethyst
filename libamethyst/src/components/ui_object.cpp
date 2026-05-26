#include "components/ui_object.h"
#include "components/common.h"
#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/extensions/ui_size_constraint.h"
#include "components/ui_layer.h"
#include "components/window.h"
#include "logging/log.h"
#include "rendering/instance_data.h"
#include "utils/profiling.h"

#include <cstdint>

namespace Amethyst {

UIObject::UIObject()
{
    // sane defaults so we can compare to the default struct
    // otherwise we need to force the user to copy the current struct all the time, or use optionals everywhere?
    m_uiObjProps.active = false;
    m_uiObjProps.anchorPoint = glm::vec2(0.0f);
    m_uiObjProps.automaticSize = AutomaticSize::OFF;
    m_uiObjProps.backgroundColor = Color3{1.0f, 1.0f, 1.0f};
    m_uiObjProps.backgroundTransparency = 0.0f;
    m_uiObjProps.borderMode = BorderMode::OUTLINE;
    m_uiObjProps.borderPixelSize = 0.0f;
    m_uiObjProps.borderColor = Color3{0.0f, 0.0f, 0.0f};
    m_uiObjProps.borderTransparency = 0.0f;
    m_uiObjProps.clipsDescendants = true;
    m_uiObjProps.cornerRadius = 0.0f;
    m_uiObjProps.guiState = GuiState::IDLE;
    m_uiObjProps.interactable = true;
    m_uiObjProps.layoutOrder = 0;
    m_uiObjProps.padding = UDim4{};
    m_uiObjProps.margin = UDim4{};
    m_uiObjProps.position = UDim2{};
    m_uiObjProps.size = UDim2{};
    m_uiObjProps.rotation = 0.0f;
    m_uiObjProps.visible = true;
    m_uiObjProps.zIndex = 1;
    m_uiObjProps.zindexBehavior = ZIndexBehavior::SIBLING;
}

UIObject::~UIObject() {}

bool UIObject::setBaseProperties(BaseProperties props)
{
    bool changed = false;

#define AM_APPLY(field)                                                \
    if (propIsSet(props.field) && m_uiObjProps.field != props.field) { \
        m_uiObjProps.field = props.field;                              \
        changed = true;                                                \
    }

    AM_APPLY(active)
    AM_APPLY(anchorPoint)
    AM_APPLY(automaticSize)
    AM_APPLY(backgroundColor)
    AM_APPLY(backgroundTransparency)
    AM_APPLY(borderMode)
    AM_APPLY(borderPixelSize)
    AM_APPLY(borderColor)
    AM_APPLY(borderTransparency)
    AM_APPLY(clipsDescendants)
    AM_APPLY(cornerRadius)
    AM_APPLY(guiState)
    AM_APPLY(interactable)
    AM_APPLY(layoutOrder)
    AM_APPLY(padding)
    AM_APPLY(margin)
    AM_APPLY(position)
    AM_APPLY(size)
    AM_APPLY(rotation)
    AM_APPLY(visible)
    AM_APPLY(zIndex)
    AM_APPLY(zindexBehavior)

#undef AM_APPLY

    if (changed) {
        markDirty();
    }
    return changed;
}

void UIObject::computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation)
{
    AM_PROFILE_FUNCTION();
    absoluteSize = m_uiObjProps.size.resolve(parentSize);
    absolutePosition = parentPos + m_uiObjProps.position.resolve(parentSize) - m_uiObjProps.anchorPoint * absoluteSize;
    absoluteRotation = m_uiObjProps.rotation + parentRotation;

    glm::vec4 m = m_uiObjProps.margin.resolve(parentSize);
    absolutePosition += glm::vec2(m.w, m.x);
    absoluteSize -= glm::vec2(m.w + m.y, m.x + m.z);

    glm::vec4 p = m_uiObjProps.padding.resolve(absoluteSize);
    absoluteContentPosition = absolutePosition + glm::vec2(p.w, p.x);
    absoluteContentSize = absoluteSize - glm::vec2(p.w + p.y, p.x + p.z);

    if (auto *sizeConstraint = getExtension<UISizeConstraint>()) {
        sizeConstraint->apply();
    }
    if (auto *arConstraint = getExtension<UIAspectRatioConstraint>()) {
        arConstraint->apply();
    }
}

InstanceData UIObject::createInstanceData() const
{
    AM_PROFILE_FUNCTION();
    glm::vec2 centerPos = absolutePosition + absoluteSize * 0.5f;

    InstanceData data{};
    data.translation = centerPos;
    data.scale = absoluteSize;
    data.clipRect = clipRect;
    data.setFillColor(Color4(m_uiObjProps.backgroundColor, 1.0f - m_uiObjProps.backgroundTransparency));
    data.setBorderColor(Color4(m_uiObjProps.borderColor, 1.0f - m_uiObjProps.borderTransparency));
    data.setRotation(glm::radians(absoluteRotation));
    data.setBorderThickness(m_uiObjProps.borderPixelSize);
    data.setCornerRadius(m_uiObjProps.cornerRadius);
    data.setPrimitiveType(PRIMITIVE_TRIANGLE);
    data.setBorderMode(m_uiObjProps.borderMode);
    data.zIndex = getZIndex();
    data.setVisible(isVisible());
    return data;
}

glm::vec4 UIObject::computeChildClipRect() const
{
    if (!m_uiObjProps.clipsDescendants) {
        return clipRect;
    }
    glm::vec4 myBounds = {absolutePosition.x, absolutePosition.y, absolutePosition.x + absoluteSize.x,
                          absolutePosition.y + absoluteSize.y};
    if (clipRect == glm::vec4(0.0f)) {
        return myBounds;
    }
    return {glm::max(clipRect.x, myBounds.x), glm::max(clipRect.y, myBounds.y), glm::min(clipRect.z, myBounds.z),
            glm::min(clipRect.w, myBounds.w)};
}

Window *UIObject::getWindow()
{
    for (Instance *current = parent; current != nullptr; current = current->parent) {
        if (auto *window = current->as<Window>()) {
            return window;
        }
    }
    return nullptr;
}

EventResult UIObject::onMouseEnter()
{
    if (onHoverChanged) {
        onHoverChanged(true);
        markDirty();
    }
    return EventResult::CONSUMED;
}

bool UIObject::isVisible() const
{
    if (!m_uiObjProps.visible) return false;
    if (!parent) return true;
    if (auto *obj = parent->as<UIObject>()) {
        return obj->isVisible();
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->isVisible();
    }
    return true;
}

int32_t UIObject::getAbsoluteZIndex() const
{
    if (parent == nullptr) {
        return m_uiObjProps.zIndex;
    }

    if (auto *obj = parent->as<UIObject>()) {
        return obj->getAbsoluteZIndex() + m_uiObjProps.zIndex;
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->getDisplayOrder() + m_uiObjProps.zIndex;
    }

    return m_uiObjProps.zIndex;
}

int32_t UIObject::getZIndex() const
{
    if (m_uiObjProps.zindexBehavior == ZIndexBehavior::GLOBAL) {
        return getAbsoluteZIndex();
    }
    return m_uiObjProps.zIndex;
}

EventResult UIObject::onMouseLeave()
{
    if (onHoverChanged) {
        onHoverChanged(false);
        markDirty();
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onMouseMoved(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseMove(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onMouseButton1Down(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseDown(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onMouseButton1Up(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseUp(x, y);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
