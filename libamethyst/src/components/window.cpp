#include "components/window.h"

#include <algorithm>
#include <vector>

#include "components/input_interface.h"
#include "components/instance.h"
#include "components/tooltip.h"
#include "components/ui_base_2d.h"
#include "components/ui_object.h"
#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/profiling.h"

namespace Amethyst {

Window::Window()
{
    InputInterface::registerWindow(this);
    m_overlayLayer = std::make_unique<OverlayLayer>();
    m_overlayLayer->setDisplayOrder(1000);
    m_overlayLayer->parent = this;
    m_tooltips = std::make_unique<TooltipStack>(this);
}

Window::~Window()
{
    InputInterface::unregisterWindow(this);
    m_children.clear();
    // Overlay children (popups, menus, focused inputs) still reach back into the tick list and
    // the tooltip stack while they die, so they go before those members do.
    m_overlayLayer.reset();
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
    // a reloaded theme leaves nothing dirty by itself, so wake every node; each one sees the new
    // generation in arrange and re-resolves
    if (Style::generation() != m_styleGeneration) {
        m_styleGeneration = Style::generation();
        markSubtreeDirty();
    }
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

    // Content moved under a still cursor (wheel scroll): hover follows the new layout instead of
    // waiting for the next mouse move.
    if (m_hoverStale && m_hasMousePosition && m_mouseCapturedBy == nullptr) {
        m_hoverStale = false;
        updateHover(static_cast<int32_t>(m_lastMousePosition.x), static_cast<int32_t>(m_lastMousePosition.y));
    }
}

// TRANSPARENT mirrors a propagated click: lower siblings still see the cursor
enum class HoverHit : uint8_t {
    MISS,        // nothing in the subtree is under the cursor
    TRANSPARENT, // something is hovered, but nothing there consumes hover
    OPAQUE,      // a hover consumer was hit, siblings below are blocked
};

static HoverHit s_fillHoverStackRecursive(const std::vector<Instance *> &instances, const vec2 &point, UIObject **stack,
                                          uint8_t &count, uint8_t capacity)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

    HoverHit result = HoverHit::MISS;
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        Instance *inst = *it;
        if (!inst->isHitTestVisible()) {
            continue;
        }

        bool pointInside = inst->containsPoint(point);
        HoverHit childHit = HoverHit::MISS;

        if (pointInside || !inst->getClipsDescendants()) {
            auto hittable = inst->getHittableInstances();
            if (!hittable.empty()) {
                childHit = s_fillHoverStackRecursive(hittable, point, stack, count, capacity);
            }
        }

        if (childHit == HoverHit::MISS && !pointInside) {
            continue;
        }

        UIObject *obj = inst->asUiObject();
        // an ignored node is transparent: only what its children hit counts
        if (obj != nullptr && obj->ignores(INTERACTION_CATEGORY_HOVER)) {
            if (childHit == HoverHit::OPAQUE) {
                return HoverHit::OPAQUE;
            }
            if (childHit == HoverHit::TRANSPARENT) {
                result = HoverHit::TRANSPARENT;
            }
            continue;
        }

        HoverHit hit = childHit;
        if (pointInside) {
            if (obj != nullptr) {
                if (count < capacity) {
                    stack[count++] = obj;
                }
                if (obj->consumes(INTERACTION_CATEGORY_HOVER)) {
                    hit = HoverHit::OPAQUE;
                }
            }
            if (hit == HoverHit::MISS) {
                hit = HoverHit::TRANSPARENT;
            }
        }

        if (hit == HoverHit::OPAQUE) {
            return HoverHit::OPAQUE;
        }
        result = HoverHit::TRANSPARENT;
    }
    return result;
}

template <typename Fn>
static bool s_dispatchRecursive(const std::vector<Instance *> &instances, const vec2 &point, InteractionCategory category, Fn &&fn)
{
    std::vector<Instance *> sorted(instances.begin(), instances.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });

    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        Instance *inst = *it;

        if (!inst->isHitTestVisible()) {
            continue;
        }

        // A handler further down may destroy this node, or the siblings we still have queued
        // (a row callback clearing its table). Once anything on the path dies the event counts
        // as handled: carrying on would walk freed nodes.
        std::weak_ptr<bool> alive = inst->aliveToken();

        bool pointInside = inst->containsPoint(point);
        bool shouldCheckChildren = pointInside || !inst->getClipsDescendants();

        if (shouldCheckChildren) {
            auto hittable = inst->getHittableInstances();
            if (!hittable.empty()) {
                if (s_dispatchRecursive(std::move(hittable), point, category, fn)) {
                    return true;
                }
                if (!Instance::isAlive(alive)) {
                    return true;
                }
            }
        }

        if (pointInside) {
            if (auto *obj = inst->asUiObject(); obj != nullptr && !obj->ignores(category)) {
                if (fn(obj) == EventResult::CONSUMED || !Instance::isAlive(alive)) {
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
    s_dispatchRecursive(getHittableInstances(), point, INTERACTION_CATEGORY_SCROLL, [&result](UIObject *obj) {
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

    const size_t pressSlot = static_cast<size_t>(button - MOUSE_BUTTON_1);
    PressRecord &press = m_pressTargets[pressSlot];

    if (input.state == InputState::END) {
        press.released = false;
        if (m_mouseCapturedBy != nullptr) {
            UIObject *captured = m_mouseCapturedBy;
            press.released = press.object == captured;
            captured->onInputEnded(input);
        } else {
            s_dispatchRecursive(getHittableInstances(), vec2(x, y), INTERACTION_CATEGORY_CLICK, [&](UIObject *obj) -> EventResult {
                if (obj == press.object) {
                    press.released = true;
                }
                return obj->onInputEnded(input);
            });
        }

        PressRecord finished = std::move(press);
        press = PressRecord{};
        if (!finished.released && Instance::isAlive(finished.alive)) {
            finished.object->onPressCancelled(type);
        }
        return;
    }

    vec2 point(x, y);

    press = PressRecord{};
    if (m_overlayLayer != nullptr) {
        PressVote vote{};
        m_overlayLayer->onPressVote.fire(point, vote);
        if (vote.result() == EventResult::CONSUMED) {
            return;
        }
    }

    s_dispatchRecursive(getHittableInstances(), point, INTERACTION_CATEGORY_CLICK, [&](UIObject *obj) -> EventResult {
        std::weak_ptr<bool> alive = obj->aliveToken();
        EventResult result = obj->onInputBegan(input);
        if (result == EventResult::CONSUMED && Instance::isAlive(alive)) {
            press = PressRecord{obj, alive, false};
        }
        return result;
    });
}

void Window::onMouseMove(int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    m_lastMousePosition = vec2(x, y);
    m_hasMousePosition = true;

    if (m_mouseCapturedBy) {
        m_mouseCapturedBy->onMouseMoved(x, y);
        return;
    }

    updateHover(x, y);

    for (int i = m_hoverCurrent.count - 1; i >= 0; --i) {
        const HoverEntry &entry = m_hoverCurrent.items[i];
        if (!Instance::isAlive(entry.alive)) {
            continue;
        }
        if (entry.object->onMouseMoved(x, y) == EventResult::CONSUMED) {
            break;
        }
    }
}

void Window::updateHover(int32_t x, int32_t y)
{
    vec2 point(x, y);

    std::swap(m_hoverCurrent, m_hoverPrevious);
    m_hoverCurrent.count = 0;

    std::array<UIObject *, MAX_HOVER_DEPTH> hit{};
    uint8_t hitCount = 0;
    s_fillHoverStackRecursive(getHittableInstances(), point, hit.data(), hitCount, MAX_HOVER_DEPTH);
    std::reverse(hit.data(), hit.data() + hitCount);
    for (uint8_t i = 0; i < hitCount; ++i) {
        m_hoverCurrent.items[i] = HoverEntry{hit[i], hit[i]->aliveToken()};
    }
    m_hoverCurrent.count = hitCount;

    uint8_t common = 0;
    uint8_t minCount = std::min(m_hoverCurrent.count, m_hoverPrevious.count);
    while (common < minCount && m_hoverPrevious.items[common].object == m_hoverCurrent.items[common].object &&
           Instance::isAlive(m_hoverPrevious.items[common].alive)) {
        ++common;
    }

    // Each handler may destroy any hovered node; the tokens are rechecked right before every
    // call so a node killed by an earlier handler is skipped rather than called.
    for (int i = m_hoverPrevious.count - 1; i >= common; --i) {
        const HoverEntry &entry = m_hoverPrevious.items[i];
        if (Instance::isAlive(entry.alive)) {
            entry.object->onMouseLeave();
        }
    }

    for (uint8_t i = common; i < m_hoverCurrent.count; ++i) {
        const HoverEntry &entry = m_hoverCurrent.items[i];
        if (Instance::isAlive(entry.alive)) {
            entry.object->onMouseEnter();
        }
    }

    // drop the previous frame's references so nothing keeps dead nodes' tokens around
    for (uint8_t i = 0; i < m_hoverPrevious.count; ++i) {
        m_hoverPrevious.items[i] = HoverEntry{};
    }
    m_hoverPrevious.count = 0;
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
    m_tooltips->onTick(deltaTime);

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
    m_hoverStale = true;
    Instance *target = findClickedObject(x, y);
    vec2 point(x, y);

    while (target) {
        auto *uiObject = target->asUiObject();
        if (uiObject != nullptr && !uiObject->ignores(INTERACTION_CATEGORY_SCROLL)) {
            std::weak_ptr<bool> alive = target->aliveToken();
            EventResult result = EventResult::PROPAGATE;
            if (yoffset > 0) {
                result = uiObject->onMouseScrollUp();
            } else if (yoffset < 0) {
                result = uiObject->onMouseScrollDown();
            }
            if (result == EventResult::CONSUMED || !Instance::isAlive(alive)) {
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
