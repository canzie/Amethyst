/*
 * InvisibleButton implementation
 */

#include "components/invisible_button.h"

namespace Amethyst {

InvisibleButton::~InvisibleButton() = default;

void InvisibleButton::draw(DrawContext &ctx)
{
    glm::vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
