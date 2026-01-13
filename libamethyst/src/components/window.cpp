/*
 * Window implementation
 */

#include "components/window.h"

#include <algorithm>
#include <vector>

#include "components/input_interface.h"
#include "components/ui_base_2d.h"
#include "components/ui_object.h"
#include "logging/log.h"

namespace Amethyst {

Window::Window()
{
    InputInterface::registerWindow(this);
}

Window::~Window()
{
    InputInterface::unregisterWindow(this);
}

void Window::draw(GeometryRegistry &registry)
{
    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {

            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(registry);
        }
    }
}

Instance *Window::findClickedObject(uint32_t x, uint32_t y)
{
    glm::vec2 point(x, y);

    std::vector<Instance *> sortedChildren(children.begin(), children.end());
    std::sort(sortedChildren.begin(), sortedChildren.end(), [](Instance *a, Instance *b) {
        auto *aObj = a->as<UIObject>();
        auto *bObj = b->as<UIObject>();
        if (aObj && bObj) {
            return aObj->zIndex < bObj->zIndex;
        }
        return false;
    });

    for (auto it = sortedChildren.rbegin(); it != sortedChildren.rend(); ++it) {
        Instance *child = *it;
        if (auto *base2d = child->as<UIBase2D>()) {
            if (base2d->containsPoint(point)) {
                if (auto *obj = child->as<UIObject>()) {
                    if (!obj->visible || !obj->interactable) {
                        continue;
                    }
                }
                if (auto *layer = child->as<UILayer>()) {
                    auto *result = static_cast<Window *>(layer)->findClickedObject(x, y);
                    if (result) {
                        return result;
                    }
                }
                return child;
            }
        }
    }

    return nullptr;
}

void Window::onMouseButton(int button, int action, int mods, uint32_t x, uint32_t y)
{
    (void)mods;
    Instance *clicked = findClickedObject(x, y);

    if (clicked) {
        auto *uiObject = clicked->as<UIObject>();

        if (button == MOUSE_BUTTON_1) {
            if (action == MOUSE_ACTION_PRESS) {
                uiObject->onMouseButton1Down(x, y);
            } else if (action == MOUSE_ACTION_RELEASE) {
                uiObject->onMouseButton1Up(x, y);
                uiObject->onMouseButton1Click();
            }
        } else if (button == MOUSE_BUTTON_2) {
            if (action == MOUSE_ACTION_PRESS) {
                uiObject->onMouseButton2Down(x, y);
            } else if (action == MOUSE_ACTION_RELEASE) {
                uiObject->onMouseButton2Up(x, y);
                uiObject->onMouseButton2Click();
            }
        }
    }
}

void Window::onMouseScroll(float xoffset, float yoffset, uint32_t x, uint32_t y)
{
    (void)xoffset;
    Instance *clicked = findClickedObject(x, y);

    if (clicked) {
        auto *uiObject = clicked->as<UIObject>();
        if (yoffset > 0) {
            uiObject->onMouseScrollUp();
        } else if (yoffset < 0) {
            uiObject->onMouseScrollDown();
        }
    }
}

} // namespace Amethyst
