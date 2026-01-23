/*
 * InvisibleButton implementation
 */

#include "components/invisible_button.h"

namespace Amethyst {

InvisibleButton::~InvisibleButton() = default;

void InvisibleButton::draw(DrawContext &ctx)
{
    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
