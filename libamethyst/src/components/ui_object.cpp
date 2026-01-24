#include "components/ui_object.h"
#include "components/common.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/window.h"

#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>

namespace Amethyst {

UIObject::~UIObject()
{
    if (Window *window = getWindow()) {
        window->releaseMouse(this);
    }
}

void UIObject::computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation)
{
    absoluteSize = size.resolve(parentSize);
    absolutePosition = parentPos + position.resolve(parentSize) - anchorPoint * absoluteSize;
    absoluteRotation = rotation + parentRotation;
}

glm::mat4 UIObject::buildTransform() const
{

    glm::vec2 centerPos = absolutePosition + absoluteSize * glm::vec2(0.5f);

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(centerPos, 0.0f)) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(absoluteRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(absoluteSize, 1.0f));

    return transform;
}

InstanceData UIObject::createInstanceData() const
{
    return InstanceData{.transform = buildTransform(),
                        .fillColor = Color4(backgroundColor, 1.0f - backgroundTransparency),
                        .borderColor = Color4(borderColor, 1.0f - borderTransparency),
                        .borderThickness = borderPixelSize,
                        .cornerRadius = cornerRadius,
                        .primitiveType = PRIMITIVE_TRIANGLE,
                        .borderMode = static_cast<uint32_t>(borderMode),
                        .textureId = UINT32_MAX,
                        .zIndex = zIndex};
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
