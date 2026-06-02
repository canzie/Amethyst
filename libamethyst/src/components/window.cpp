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
    m_overlayLayer = std::make_unique<OverlayLayer>();
    m_overlayLayer->setDisplayOrder(1000);
    m_overlayLayer->parent = this;
}

Window::~Window()
{
    InputInterface::unregisterWindow(this);
    m_children.clear();
}

void Window::purgeFromHoverStacks(Instance *dead)
{
    auto purge = [dead](HoverStack &stack) {
        uint8_t w = 0;
        for (uint8_t r = 0; r < stack.count; ++r) {
            if (stack.items[r] != dead) {
                stack.items[w++] = stack.items[r];
            }
        }
        stack.count = w;
    };
    purge(m_hoverCurrent);
    purge(m_hoverPrevious);
}

std::vector<Instance *> Window::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_children.size() + 1);
    for (auto &child : m_children) {
        result.push_back(child.get());
    }
    result.push_back(m_overlayLayer.get());
    return result;
}

void Window::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    DrawContext layerCtx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.overlay = m_overlayLayer->geometryRegistry();
    layerCtx.textProcessor = ctx.textProcessor;
    layerCtx.glyphAtlas = ctx.glyphAtlas;
    layerCtx.svgAtlas = ctx.svgAtlas;

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(layerCtx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->draw(layerCtx);
        }
    }

    m_overlayLayer->absoluteSize = absoluteSize;
    m_overlayLayer->absolutePosition = absolutePosition;
    m_overlayLayer->clipRect = clipRect;
    m_overlayLayer->draw(layerCtx);
}

static bool s_fillHoverStackRecursive(const std::vector<Instance *> &instances, const glm::vec2 &point,
                                    UIObject **stack, uint8_t &count, uint8_t capacity)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        Instance *inst = *it;
        if (!inst->isHitTestVisible()) {
            continue;
        }

        bool pointInside = inst->containsPoint(point);
        bool childHit = false;

        if (pointInside || !inst->getClipsDescendants()) {
            auto hittable = inst->getHittableInstances();
            if (!hittable.empty()) {
                childHit = s_fillHoverStackRecursive(hittable, point, stack, count, capacity);
            }
        }

        if (!childHit && !pointInside) {
            continue;
        }

        if (pointInside) {
            if (auto *obj = inst->as<UIObject>()) {
                if (count < capacity) {
                    stack[count++] = obj;
                }
            }
        }
        return true;
    }
    return false;
}

template<typename Fn>
static bool s_dispatchRecursive(const std::vector<Instance *> &instances, const glm::vec2 &point, Fn &&fn)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        Instance *inst = *it;

        if (!inst->isHitTestVisible()) {
            continue;
        }

        bool pointInside = inst->containsPoint(point);
        bool shouldCheckChildren = pointInside || !inst->getClipsDescendants();

        if (shouldCheckChildren) {
            auto hittable = inst->getHittableInstances();
            if (!hittable.empty()) {
                if (s_dispatchRecursive(std::move(hittable), point, fn)) {
                    return true;
                }
            }
        }

        if (pointInside) {
            if (auto *obj = inst->as<UIObject>()) {
                if (fn(obj) == EventResult::CONSUMED) {
                    return true;
                }
            }
        }
    }

    return false;
}

Instance *Window::findClickedObject(uint32_t x, uint32_t y)
{
    AM_PROFILE_FUNCTION();
    glm::vec2 point(x, y);
    Instance *result = nullptr;
    s_dispatchRecursive(getHittableInstances(), point, [&result](UIObject *obj) {
        result = obj;
        return EventResult::CONSUMED;
    });
    return result;
}

void Window::onMouseButton(int button, int action, int mods, uint32_t x, uint32_t y)
{
    AM_PROFILE_FUNCTION();
    (void)mods;

    if (m_mouseCapturedBy && action == MOUSE_ACTION_RELEASE) {
        UIObject *capturedObject = m_mouseCapturedBy;
        bool stillOver = capturedObject->containsPoint(glm::vec2(x, y));
        switch (button) {
        case MOUSE_BUTTON_1:
            capturedObject->onMouseButton1Up(x, y);
            if (m_mouseCapturedBy == capturedObject && stillOver) capturedObject->onMouseButton1Click();
            break;
        case MOUSE_BUTTON_2:
            capturedObject->onMouseButton2Up(x, y);
            if (m_mouseCapturedBy == capturedObject && stillOver) capturedObject->onMouseButton2Click();
            break;
        default:
            break;
        }
        return;
    }

    glm::vec2 point(x, y);
    s_dispatchRecursive(getHittableInstances(), point, [&](UIObject *obj) -> EventResult {
        switch (button) {
        case MOUSE_BUTTON_1:
            switch (action) {
            case MOUSE_ACTION_PRESS:
                return obj->onMouseButton1Down(x, y);
            case MOUSE_ACTION_RELEASE: {
                EventResult up = obj->onMouseButton1Up(x, y);
                EventResult click = obj->onMouseButton1Click();
                return (up == EventResult::CONSUMED && click == EventResult::CONSUMED)
                    ? EventResult::CONSUMED : EventResult::PROPAGATE;
            }
            default: break;
            }
            break;
        case MOUSE_BUTTON_2:
            switch (action) {
            case MOUSE_ACTION_PRESS:
                return obj->onMouseButton2Down(x, y);
            case MOUSE_ACTION_RELEASE: {
                EventResult up = obj->onMouseButton2Up(x, y);
                EventResult click = obj->onMouseButton2Click();
                return (up == EventResult::CONSUMED && click == EventResult::CONSUMED)
                    ? EventResult::CONSUMED : EventResult::PROPAGATE;
            }
            default: break;
            }
            break;
        default: break;
        }
        return EventResult::CONSUMED;
    });
}

void Window::onMouseMove(uint32_t x, uint32_t y)
{
    AM_PROFILE_FUNCTION();
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onMouseMoved(x, y);
        return;
    }

    glm::vec2 point(x, y);

    std::swap(m_hoverCurrent, m_hoverPrevious);
    m_hoverCurrent.count = 0;

    s_fillHoverStackRecursive(getHittableInstances(), point,
                            m_hoverCurrent.items.data(), m_hoverCurrent.count, MAX_HOVER_DEPTH);
    std::reverse(m_hoverCurrent.items.data(), m_hoverCurrent.items.data() + m_hoverCurrent.count);

    uint8_t common = 0;
    uint8_t minCount = std::min(m_hoverCurrent.count, m_hoverPrevious.count);
    while (common < minCount && m_hoverPrevious.items[common] == m_hoverCurrent.items[common]) {
        ++common;
    }

    for (int i = m_hoverPrevious.count - 1; i >= common; --i) {
        UIObject *obj = m_hoverPrevious.items[i];
        obj->onMouseLeave();
        if (obj != m_mouseCapturedBy) {
            obj->onDestroy = nullptr;
        }
    }

    for (uint8_t i = common; i < m_hoverCurrent.count; ++i) {
        UIObject *obj = m_hoverCurrent.items[i];
        if (obj != m_mouseCapturedBy) {
            // TODO: replace with signal connection when signal/event system is implemented
            obj->onDestroy = [this](Instance *dead) {
                this->purgeFromHoverStacks(dead);
            };
        }
        obj->onMouseEnter();
    }

    for (int i = m_hoverCurrent.count - 1; i >= 0; --i) {
        if (m_hoverCurrent.items[i]->onMouseMoved(x, y) == EventResult::CONSUMED) {
            break;
        }
    }
}

void Window::captureMouse(UIObject *object)
{
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onDestroy = nullptr;
    }

    m_mouseCapturedBy = object;

    if (m_mouseCapturedBy) {
        // TODO: replace with signal connection when signal/event system is implemented
        m_mouseCapturedBy->onDestroy = [this](Instance *dead) {
            if (this->m_mouseCapturedBy == dead) {
                this->m_mouseCapturedBy = nullptr;
            }
            this->purgeFromHoverStacks(dead);
        };
    }
}

void Window::releaseMouse(UIObject *object)
{
    if (m_mouseCapturedBy == object) {
        if (m_mouseCapturedBy) {
            m_mouseCapturedBy->onDestroy = nullptr;
            for (uint8_t i = 0; i < m_hoverCurrent.count; ++i) {
                if (m_hoverCurrent.items[i] == object) {
                    // TODO: replace with signal connection when signal/event system is implemented
                    object->onDestroy = [this](Instance *dead) {
                        this->purgeFromHoverStacks(dead);
                    };
                    break;
                }
            }
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
            EventResult result = EventResult::PROPAGATE;
            if (yoffset > 0) {
                result = uiObject->onMouseScrollUp();
            } else if (yoffset < 0) {
                result = uiObject->onMouseScrollDown();
            }
            if (result == EventResult::CONSUMED) {
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
