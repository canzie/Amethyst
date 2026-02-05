#include "components/ui_object.h"
#include "components/common.h"
#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/extensions/ui_size_constraint.h"
#include "components/window.h"
#include "rendering/instance_data.h"
#include "utils/profiling.h"

#include "ui_object.h"
#include <cstdint>

namespace Amethyst {

UIObject::~UIObject() {}

void UIObject::computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation)
{
    AM_PROFILE_FUNCTION();
    absoluteSize = size.resolve(parentSize);
    absolutePosition = parentPos + position.resolve(parentSize) - anchorPoint * absoluteSize;
    absoluteRotation = rotation + parentRotation;

    glm::vec4 p = padding.resolve(parentSize);
    absolutePosition += glm::vec2(p.w, p.x);
    absoluteSize -= glm::vec2(p.w + p.y, p.x + p.z);

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
    data.setFillColor(Color4(backgroundColor, 1.0f - backgroundTransparency));
    data.setBorderColor(Color4(borderColor, 1.0f - borderTransparency));
    data.setRotation(glm::radians(absoluteRotation));
    data.setBorderThickness(borderPixelSize);
    data.setCornerRadius(cornerRadius);
    data.setPrimitiveType(PRIMITIVE_TRIANGLE);
    data.setBorderMode(borderMode);
    data.zIndex = getZIndex();
    data.setVisible(isVisible());
    return data;
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

void UIObject::onMouseEnter() {}

bool UIObject::isVisible() const
{
    if (!visible) return false;
    if (!parent) return true;
    if (auto *obj = parent->as<UIObject>()) {
        return obj->isVisible();
    }
    return true;
}

int32_t UIObject::getAbsoluteZIndex() const
{
    if (!parent) {
        return zIndex;
    }

    if (auto *obj = parent->as<UIObject>()) {
        return obj->getAbsoluteZIndex() + zIndex;
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->getDisplayOrder() + zIndex;
    }

    return zIndex;
}

int32_t UIObject::getZIndex() const
{
    if (zindexBehavior == ZIndexBehavior::GLOBAL) {
        return getAbsoluteZIndex();
    }
    return zIndex;
}

void UIObject::onMouseLeave() {}

void UIObject::onMouseMoved(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseMove(x, y);
    }
}

void UIObject::onMouseButton1Down(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseDown(x, y);
    }
}

void UIObject::onMouseButton1Up(uint32_t x, uint32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseUp(x, y);
    }
}

} // namespace Amethyst
