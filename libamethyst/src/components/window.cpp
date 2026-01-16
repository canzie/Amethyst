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
    return findClickedObjectRecursive(children, point);
}

Instance *Window::findClickedObjectRecursive(const std::vector<Instance *> &instances, const glm::vec2 &point)
{
    std::vector<Instance *> sortedInstances(instances.begin(), instances.end());
    std::sort(sortedInstances.begin(), sortedInstances.end(), [](Instance *a, Instance *b) {
        auto *aObj = a->as<UIObject>();
        auto *bObj = b->as<UIObject>();
        if (aObj && bObj) {
            return aObj->zIndex < bObj->zIndex;
        }
        return false;
    });

    for (auto it = sortedInstances.rbegin(); it != sortedInstances.rend(); ++it) {
        Instance *inst = *it;
        auto *base2d = inst->as<UIBase2D>();
        if (!base2d) {
            continue;
        }

        auto *obj = inst->as<UIObject>();
        if (obj && (!obj->visible || !obj->interactable)) {
            continue;
        }

        bool pointInside = base2d->containsPoint(point);
        bool shouldCheckChildren = pointInside || (obj && !obj->clipsDescendants);

        if (shouldCheckChildren && !inst->children.empty()) {
            Instance *childResult = findClickedObjectRecursive(inst->children, point);
            if (childResult) {
                return childResult;
            }
        }

        if (pointInside) {
            return inst;
        }
    }

    return nullptr;
}

void Window::onMouseButton(int button, int action, int mods, uint32_t x, uint32_t y)
{
    (void)mods;

    if (m_mouseCapturedBy && action == MOUSE_ACTION_RELEASE) {
        UIObject *capturedObject = m_mouseCapturedBy;
        switch (button) {
        case MOUSE_BUTTON_1:
            capturedObject->onMouseButton1Up(x, y);
            if (findClickedObject(x, y) == capturedObject) {
                capturedObject->onMouseButton1Click();
            }
            break;
        case MOUSE_BUTTON_2:
            capturedObject->onMouseButton2Up(x, y);
            if (findClickedObject(x, y) == capturedObject) {
                capturedObject->onMouseButton2Click();
            }
            break;
        default:
            break;
        }
        return;
    }

    Instance *clicked = findClickedObject(x, y);
    if (!clicked) {
        return;
    }

    auto *uiObject = clicked->as<UIObject>();

    switch (button) {
    case MOUSE_BUTTON_1:
        switch (action) {
        case MOUSE_ACTION_PRESS:
            uiObject->onMouseButton1Down(x, y);
            break;
        case MOUSE_ACTION_RELEASE:
            uiObject->onMouseButton1Up(x, y);
            uiObject->onMouseButton1Click();
            break;
        default:
            break;
        }
        break;
    case MOUSE_BUTTON_2:
        switch (action) {
        case MOUSE_ACTION_PRESS:
            uiObject->onMouseButton2Down(x, y);
            break;
        case MOUSE_ACTION_RELEASE:
            uiObject->onMouseButton2Up(x, y);
            uiObject->onMouseButton2Click();
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void Window::onMouseMove(uint32_t x, uint32_t y)
{
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onMouseMoved(x, y);
        return;
    }

    Instance *hovered = findClickedObject(x, y);
    if (hovered != m_lastHoveredInstance) {
        if (m_lastHoveredInstance) {
            auto obj = m_lastHoveredInstance->as<UIObject>();
            obj->onMouseLeave(x, y);
        }
        if (hovered) {
            auto obj = hovered->as<UIObject>();
            obj->onMouseEnter(x, y);
        }
    }

    if (hovered) {
        auto *uiObject = hovered->as<UIObject>();
        uiObject->onMouseMoved(x, y);
    }

    m_lastHoveredInstance = hovered;
}

void Window::captureMouse(UIObject *object)
{
    m_mouseCapturedBy = object;
}

void Window::releaseMouse(UIObject *object)
{
    if (m_mouseCapturedBy == object) {
        m_mouseCapturedBy = nullptr;
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
