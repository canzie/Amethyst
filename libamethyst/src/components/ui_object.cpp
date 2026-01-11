#include "components/ui_object.h"
#include "components/common.h"

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
                        .primitiveType = PRIMITIVE_TRIANGLE};
}

} // namespace Amethyst
