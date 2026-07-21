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
        if (auto *obj = child->asUiObject()) {
            obj->draw(layerCtx);
        } else if (auto *layer = child->asLayer()) {
            layer->draw(layerCtx);
        }
    }

    m_overlayLayer->draw(layerCtx);
}

void Window::arrange()
{
    AM_PROFILE_FUNCTION();
    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            obj->arrange();
        } else if (auto *layer = child->asLayer()) {
            layer->arrange();
        }
    }

    m_overlayLayer->absoluteSize = absoluteSize;
    m_overlayLayer->absolutePosition = absolutePosition;
    m_overlayLayer->clipRect = clipRect;
    m_overlayLayer->arrange();
}

static bool s_fillHoverStackRecursive(const std::vector<Instance *> &instances, const vec2 &point, UIObject **stack, uint8_t &count,
                                      uint8_t capacity)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

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
            if (auto *obj = inst->asUiObject()) {
                if (count < capacity) {
                    stack[count++] = obj;
                }
            }
        }
        return true;
    }
    return false;
}

template <typename Fn> static bool s_dispatchRecursive(const std::vector<Instance *> &instances, const vec2 &point, Fn &&fn)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

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
            if (auto *obj = inst->asUiObject()) {
                if (fn(obj) == EventResult::CONSUMED) {
                    return true;
                }
            }
        }
    }

    return false;
}

Instance *Window::findClickedObject(int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    vec2 point(x, y);
    Instance *result = nullptr;
    s_dispatchRecursive(getHittableInstances(), point, [&result](UIObject *obj) {
        result = obj;
        return EventResult::CONSUMED;
    });
    return result;
}

void Window::onMouseButton(int button, int action, int mods, int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();

    InputType type;
    switch (button) {
    case MOUSE_BUTTON_1:
        type = InputType::MOUSE_BUTTON_1;
        break;
    case MOUSE_BUTTON_2:
        type = InputType::MOUSE_BUTTON_2;
        break;
    case MOUSE_BUTTON_3:
        type = InputType::MOUSE_BUTTON_3;
        break;
    default:
        return;
    }

    InputObject input{};
    input.type = type;
    input.position = {static_cast<float>(x), static_cast<float>(y), 0.0f};
    input.modifiers = mods;
    if (action == MOUSE_ACTION_PRESS) {
        input.state = InputState::BEGIN;
    } else if (action == MOUSE_ACTION_RELEASE) {
        input.state = InputState::END;
    } else {
        return;
    }

    if (m_mouseCapturedBy && input.state == InputState::END) {
        m_mouseCapturedBy->onInputEnded(input);
        return;
    }

    vec2 point(x, y);

    if (input.state == InputState::BEGIN && m_overlayLayer != nullptr) {
        PressVote vote{};
        m_overlayLayer->onPressVote.fire(point, vote);
        if (vote.result() == EventResult::CONSUMED) {
            return;
        }
    }

    s_dispatchRecursive(getHittableInstances(), point, [&](UIObject *obj) -> EventResult {
        if (input.state == InputState::BEGIN) {
            return obj->onInputBegan(input);
        }
        return obj->onInputEnded(input);
    });
}

void Window::onMouseMove(int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onMouseMoved(x, y);
        return;
    }

    vec2 point(x, y);

    std::swap(m_hoverCurrent, m_hoverPrevious);
    m_hoverCurrent.count = 0;

    s_fillHoverStackRecursive(getHittableInstances(), point, m_hoverCurrent.items.data(), m_hoverCurrent.count, MAX_HOVER_DEPTH);
    std::reverse(m_hoverCurrent.items.data(), m_hoverCurrent.items.data() + m_hoverCurrent.count);

    uint8_t common = 0;
    uint8_t minCount = std::min(m_hoverCurrent.count, m_hoverPrevious.count);
    while (common < minCount && m_hoverPrevious.items[common] == m_hoverCurrent.items[common]) {
        ++common;
    }

    for (int i = m_hoverPrevious.count - 1; i >= common; --i) {
        UIObject *obj = m_hoverPrevious.items[i];
        obj->onMouseLeave();
    }

    for (uint8_t i = common; i < m_hoverCurrent.count; ++i) {
        UIObject *obj = m_hoverCurrent.items[i];
        if (obj != m_mouseCapturedBy) {
            obj->onDestroy.detachedOnce([this](Instance *dead) { this->purgeFromHoverStacks(dead); });
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
    if (m_mouseCapturedBy == object) {
        return;
    }

    m_mouseCapturedByConn.disconnect();
    m_mouseCapturedBy = object;

    if (object != nullptr) {
        m_mouseCapturedByConn = object->onDestroy.once([this](Instance *dead) {
            if (this->m_mouseCapturedBy == dead) {
                this->m_mouseCapturedBy = nullptr;
            }
            this->purgeFromHoverStacks(dead);
        });
    }
}

void Window::releaseMouse(UIObject *object)
{
    if (m_mouseCapturedBy == object) {
        m_mouseCapturedByConn.disconnect();
        m_mouseCapturedBy = nullptr;
    }
}

void TickHandle::unregister()
{
    if (active()) {
        window->unregisterTick(id);
        window = nullptr;
    }
}

TickHandle Window::registerTick(std::function<void(float)> callback)
{
    return TickHandle{this, m_tickCallbacks.insert(std::move(callback))};
}

void Window::unregisterTick(uint32_t id)
{
    m_tickCallbacks.erase(id);
}

void Window::tick(float deltaTime)
{
    // Copy each callback before invoking: a tick may unregister itself (e.g. an input losing
    // focus), which frees its slot mid-iteration.
    uint32_t slots = m_tickCallbacks.slotCount();
    for (uint32_t i = 0; i < slots; ++i) {
        if (auto *callback = m_tickCallbacks.tryGet(i)) {
            std::function<void(float)> invoke = *callback;
            invoke(deltaTime);
        }
    }
}

void Window::onMouseScroll(float xoffset, float yoffset, int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    (void)xoffset;
    Instance *target = findClickedObject(x, y);
    vec2 point(x, y);

    while (target) {
        auto *uiObject = target->asUiObject();
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
