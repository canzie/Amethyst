/*
 * Window implementation
 */

#include "components/window.h"

#include "components/ui_base_2d.h"

namespace Amethyst {

void Window::draw(GeometryRegistry& registry)
{
    for (Instance* child : children) {
        if (auto* drawable = child->as<UIBase2D>()) {
            drawable->draw(registry);
        }
    }
}

} // namespace Amethyst
