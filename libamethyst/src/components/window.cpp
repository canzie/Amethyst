/*
 * Window implementation
 */

#include "components/window.h"

#include "components/ui_base_2d.h"
#include "components/ui_object.h"

namespace Amethyst {

void Window::draw(GeometryRegistry &registry)
{
    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {

            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(registry);
        }
    }
}

} // namespace Amethyst
