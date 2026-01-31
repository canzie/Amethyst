/*
 * InvisibleButton implementation
 */

#include "components/invisible_button.h"

namespace Amethyst {

InvisibleButton::~InvisibleButton() = default;

void InvisibleButton::draw(DrawContext &ctx)
{
    glm::vec4 childClip = computeChildClipRect();

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
