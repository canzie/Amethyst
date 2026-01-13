#include "components/ui_object.h"
#include "components/common.h"
#include "logging/log.h"

#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>

namespace Amethyst {

void UIObject::computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation)
{
    absoluteSize = size.resolve(parentSize);
    absolutePosition = parentPos + position.resolve(parentPos);
    absoluteRotation = rotation + parentRotation;
}

glm::mat4 UIObject::buildTransform() const
{

    glm::vec2 offset = anchorPoint * absoluteSize - glm::vec2(0.5f) * absoluteSize;

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(absolutePosition, 0.0f)) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(absoluteRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
                          glm::translate(glm::mat4(1.0f), glm::vec3(-offset, 0.0f)) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(absoluteSize, 1.0f));

    return transform;
}

InstanceData UIObject::createInstanceData() const
{
    return InstanceData{.transform = buildTransform(),
                        .fillColor = backgroundColor,
                        .borderColor = borderColor,
                        .borderThickness = borderPixelSize,
                        .cornerRadius = cornerRadius,
                        .primitiveType = PRIMITIVE_TRIANGLE,
                        .borderMode = static_cast<uint32_t>(borderMode)};
}

void UIObject::onMouseEnter(uint32_t mouseX, uint32_t mouseY)
{
    startDrag(mouseX, mouseY);
}

void UIObject::onMouseLeave(uint32_t, uint32_t)
{
    endDrag();
}

void UIObject::onMouseMoved(uint32_t mouseX, uint32_t mouseY)
{
    updateDrag(mouseX, mouseY);
}

void UIObject::startDrag(uint32_t mouseX, uint32_t mouseY)
{
    m_isDragging = true;
    m_dragStartMouse = glm::vec2(mouseX, mouseY);
    m_dragStartOffset = position.offset;
}

void UIObject::updateDrag(uint32_t mouseX, uint32_t mouseY)
{
    if (!m_isDragging) {
        return;
    }

    glm::vec2 currentMouse(mouseX, mouseY);
    glm::vec2 delta = currentMouse - m_dragStartMouse;
    position.offset = m_dragStartOffset + delta;
    AM_LOG_TRACE("Drag delta {},{}", delta.x, delta.y);
    markDirty();
}

void UIObject::endDrag()
{
    m_isDragging = false;

    AM_LOG_TRACE("Drag Ended");
}

} // namespace Amethyst
