/*
 * Window implementation
 */

#include "components/window.h"

#include <algorithm>
#include <vector>

#include "components/input_interface.h"
#include "components/instance.h"
#include "components/ui_base_2d.h"
#include "components/ui_object.h"
#include "logging/log.h"
#include "rendering/draw_context.h"
#include "utils/profiling.h"

namespace Amethyst {

Window::Window()
{
    InputInterface::registerWindow(this);
}

Window::~Window()
{
    InputInterface::unregisterWindow(this);
}

void Window::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    DrawContext layerCtx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.textProcessor = ctx.textProcessor;
    layerCtx.glyphAtlas = ctx.glyphAtlas;

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(layerCtx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->draw(layerCtx);
        }
    }
}

Instance *Window::findClickedObject(uint32_t x, uint32_t y)
{
    AM_PROFILE_FUNCTION();
    glm::vec2 point(x, y);
    return findClickedObjectRecursive(children, point);
}

Instance *Window::findClickedObjectRecursive(const std::vector<Instance *> &instances, const glm::vec2 &point)
{
    AM_PROFILE_FUNCTION();
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

        auto hittable = inst->getHittableInstances();
        if (shouldCheckChildren && !hittable.empty()) {
            Instance *childResult = findClickedObjectRecursive(hittable, point);
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
    AM_PROFILE_FUNCTION();
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
    if (!uiObject) {
        return;
    }

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
    AM_PROFILE_FUNCTION();
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onMouseMoved(x, y);
        return;
    }

    Instance *hovered = findClickedObject(x, y);
    if (hovered != m_lastHoveredInstance) {
        if (m_lastHoveredInstance) {
            m_lastHoveredInstance->onDestroy = nullptr;
            if (auto obj = m_lastHoveredInstance->as<UIObject>()) {
                obj->onMouseLeave();
            }
        }
        m_lastHoveredInstance = hovered;

        if (m_lastHoveredInstance) {
            m_lastHoveredInstance->onDestroy = [this](Instance *dead) {
                if (this->m_lastHoveredInstance == dead) {
                    this->m_lastHoveredInstance = nullptr;
                }
                if (this->m_mouseCapturedBy == dead) {
                    this->m_mouseCapturedBy = nullptr;
                }
            };
            if (auto obj = m_lastHoveredInstance->as<UIObject>()) {
                obj->onMouseEnter();
            }
        }
    }

    if (m_lastHoveredInstance) {
        if (auto *uiObject = m_lastHoveredInstance->as<UIObject>()) {
            uiObject->onMouseMoved(x, y);
        }
    }

    m_lastHoveredInstance = hovered;
}

void Window::captureMouse(UIObject *object)
{
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onDestroy = nullptr;
    }

    m_mouseCapturedBy = object;

    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onDestroy = [this](Instance *dead) {
            if (this->m_mouseCapturedBy == dead) {
                this->m_mouseCapturedBy = nullptr;
            }
            if (this->m_lastHoveredInstance == dead) {
                this->m_lastHoveredInstance = nullptr;
            }
        };
    }
}

void Window::releaseMouse(UIObject *object)
{
    if (m_mouseCapturedBy == object) {
        if (m_mouseCapturedBy) {
            m_mouseCapturedBy->onDestroy = nullptr;
        }
        m_mouseCapturedBy = nullptr;
    }
}

void Window::onMouseScroll(float xoffset, float yoffset, uint32_t x, uint32_t y)
{
    AM_PROFILE_FUNCTION();
    (void)xoffset;
    Instance *target = findClickedObject(x, y);
    glm::vec2 point(x, y);

    while (target) {
        auto *uiObject = target->as<UIObject>();
        if (uiObject) {
            bool consumed = false;
            if (yoffset > 0) {
                consumed = uiObject->onMouseScrollUp();
            } else if (yoffset < 0) {
                consumed = uiObject->onMouseScrollDown();
            }
            if (consumed) {
                return;
            }
        }

        target = target->parent;
        if (auto *parentObj = target ? target->as<UIBase2D>() : nullptr) {
            if (!parentObj->containsPoint(point)) {
                target = nullptr;
            }
        }
    }
}

} // namespace Amethyst
