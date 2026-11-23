#include "components/window.h"

#include <algorithm>
#include <cmath>
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
    adoptPart(m_overlayLayer.get());
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

void Window::collectDrawChildren(std::vector<Instance *> &out)
{
    Instance::collectDrawChildren(out);
    out.push_back(m_overlayLayer.get());
}

void Window::markChildrenDirty()
{
    // the overlay layer is owned outside the child list
    Instance::markChildrenDirty();
    m_overlayLayer->markSubtreeDirty();
}

static bool s_isValidScale(float scale, const char *what)
{
    if (!std::isfinite(scale) || scale <= 0.0f) {
        AM_LOG_WARN("ignoring {} {}, it must be finite and above 0", what, scale);
        return false;
    }
    return true;
}

void Window::setDisplayScale(float scale)
{
    if (s_isValidScale(scale, "display scale")) {
        m_displayScale = scale;
        applyScale();
    }
}

void Window::setUiScale(float scale)
{
    if (s_isValidScale(scale, "UI scale")) {
        m_uiScale = scale;
        applyScale();
    }
}

void Window::applyScale()
{
    const float scale = m_displayScale * m_uiScale;
    if (scale == m_logicalToPhysicalScale) {
        return;
    }
    m_logicalToPhysicalScale = scale;
    applyFramebufferSize();
    // positions are logical and stay put, but text and SVGs rasterise again at the new size
    markSubtreeDirty();
}

void Window::setFramebufferSize(vec2 pixels)
{
    m_framebufferSize = pixels;
    applyFramebufferSize();
}

void Window::applyFramebufferSize()
{
    if (m_framebufferSize.x <= 0.0f || m_framebufferSize.y <= 0.0f) {
        return;
    }
    vec2 logical = m_framebufferSize / m_logicalToPhysicalScale;
    if (logical != absoluteSize) {
        absoluteSize = logical;
        markSubtreeDirty();
    }
}

void Window::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    ensureDrawOrder();
    DrawContext layerCtx = ctx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.overlay = m_overlayLayer->geometryRegistry();
    layerCtx.logicalToPhysicalScale = m_logicalToPhysicalScale;

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

// Hit-testing visits nodes in exactly the reverse of their draw order. Inside a layer that is: nested
// layers drawn above it, ABSOLUTE subtrees with zIndex >= 0 (highest first), the tree (later and higher
// siblings first, a node's children before the node itself), ABSOLUTE subtrees with zIndex < 0, then
// nested layers drawn below it. Layers draw by display order; a nested layer tied with its parent counts
// as below it, as its registry is created later and sorts before the parent's.

// a handler compares the pointer against its own absolutes, so it gets the pointer in its own layout space
static InputObject s_mapInputToLayout(const UIObject &obj, const InputObject &input)
{
    InputObject local = input;
    const vec2 point = obj.toLayout(vec2(input.position.x, input.position.y));
    local.position.x = point.x;
    local.position.y = point.y;
    return local;
}

// TRANSPARENT mirrors a propagated click: lower siblings still see the cursor
enum class HoverHit : uint8_t {
    MISS,        // nothing in the subtree is under the cursor
    TRANSPARENT, // something is hovered, but nothing there consumes hover
    OPAQUE,      // a hover consumer was hit, siblings below are blocked
};

// one entry of a layer's hit order: an absolute node, a nested layer, or the layer's own tree (nullptr)
struct HitSegment {
    Instance *node = nullptr;
    std::weak_ptr<bool> alive;
};

static std::vector<HitSegment> s_hitSegments(UILayer *layer)
{
    layer->ensureDrawOrder();

    std::vector<UILayer *> above;
    std::vector<UILayer *> below;
    for (UILayer *nested : layer->getNestedLayers()) {
        (nested->getDisplayOrder() > layer->getDisplayOrder() ? above : below).push_back(nested);
    }
    auto byDisplayOrder = [](UILayer *a, UILayer *b) { return a->getDisplayOrder() < b->getDisplayOrder(); };
    std::stable_sort(above.begin(), above.end(), byDisplayOrder);
    std::stable_sort(below.begin(), below.end(), byDisplayOrder);

    // the absolute nodes come sorted by zIndex, lowest first
    const std::vector<UIObject *> &absolutes = layer->getAbsoluteNodes();
    auto firstAbove = std::find_if(absolutes.begin(), absolutes.end(), [](UIObject *obj) { return obj->getZIndex() >= 0; });

    std::vector<HitSegment> segments;
    segments.reserve(above.size() + below.size() + absolutes.size() + 1);
    for (auto it = above.rbegin(); it != above.rend(); ++it) {
        segments.push_back({*it, (*it)->aliveToken()});
    }
    for (auto it = absolutes.rbegin(); it != std::make_reverse_iterator(firstAbove); ++it) {
        segments.push_back({*it, (*it)->aliveToken()});
    }
    segments.push_back({});
    for (auto it = std::make_reverse_iterator(firstAbove); it != absolutes.rend(); ++it) {
        segments.push_back({*it, (*it)->aliveToken()});
    }
    for (auto it = below.rbegin(); it != below.rend(); ++it) {
        segments.push_back({*it, (*it)->aliveToken()});
    }
    return segments;
}

// An absolute node or nested layer is hit-tested out of tree order, so the visibility and clipping of
// the ancestors the walk skipped are checked here instead.
static bool s_reachable(Instance *node, UILayer *layer, const vec2 &point)
{
    for (Instance *ancestor = node->parent; ancestor != nullptr && ancestor != layer; ancestor = ancestor->parent) {
        if (!ancestor->isHitTestVisible()) {
            return false;
        }
        if (ancestor->getClipsDescendants() && !ancestor->containsScreenPoint(point)) {
            return false;
        }
    }
    return true;
}

// node's children in its tree walk, topmost first; absolute nodes and nested layers are left to their layer
static void s_treeChildrenTopFirst(Instance *node, std::vector<Instance *> &out)
{
    std::vector<Instance *> sorted;
    node->collectSortedDrawChildren(sorted);
    out.clear();
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        if (UILayer::isTreeNode(*it)) {
            out.push_back(*it);
        }
    }
}

static HoverHit s_fillHoverChildren(Instance *node, const vec2 &point, UIObject **stack, uint8_t &count, uint8_t capacity);

static HoverHit s_fillHoverNode(Instance *inst, const vec2 &point, UIObject **stack, uint8_t &count, uint8_t capacity)
{
    if (!inst->isHitTestVisible()) {
        return HoverHit::MISS;
    }

    bool pointInside = inst->containsScreenPoint(point);
    HoverHit childHit = HoverHit::MISS;
    if (pointInside || !inst->getClipsDescendants()) {
        childHit = s_fillHoverChildren(inst, point, stack, count, capacity);
    }

    if (childHit == HoverHit::MISS && !pointInside) {
        return HoverHit::MISS;
    }

    UIObject *obj = inst->asUiObject();
    // an ignored node is transparent: only what its children hit counts
    if (obj != nullptr && obj->ignores(INTERACTION_CATEGORY_HOVER)) {
        return childHit;
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
    }
    return hit == HoverHit::OPAQUE ? HoverHit::OPAQUE : HoverHit::TRANSPARENT;
}

static HoverHit s_fillHoverChildren(Instance *node, const vec2 &point, UIObject **stack, uint8_t &count, uint8_t capacity)
{
    std::vector<Instance *> children;
    s_treeChildrenTopFirst(node, children);

    HoverHit result = HoverHit::MISS;
    for (Instance *child : children) {
        HoverHit hit = s_fillHoverNode(child, point, stack, count, capacity);
        if (hit == HoverHit::OPAQUE) {
            return HoverHit::OPAQUE;
        }
        if (hit == HoverHit::TRANSPARENT) {
            result = HoverHit::TRANSPARENT;
        }
    }
    return result;
}

static HoverHit s_fillHoverLayer(UILayer *layer, const vec2 &point, UIObject **stack, uint8_t &count, uint8_t capacity)
{
    HoverHit result = HoverHit::MISS;
    for (const HitSegment &segment : s_hitSegments(layer)) {
        HoverHit hit = HoverHit::MISS;
        if (segment.node == nullptr) {
            hit = s_fillHoverChildren(layer, point, stack, count, capacity);
        } else if (s_reachable(segment.node, layer, point)) {
            if (UILayer *nested = segment.node->asLayer(); nested != nullptr) {
                if (nested->isHitTestVisible()) {
                    hit = s_fillHoverLayer(nested, point, stack, count, capacity);
                }
            } else {
                hit = s_fillHoverNode(segment.node, point, stack, count, capacity);
            }
        }

        if (hit == HoverHit::OPAQUE) {
            return HoverHit::OPAQUE;
        }
        if (hit == HoverHit::TRANSPARENT) {
            result = HoverHit::TRANSPARENT;
        }
    }
    return result;
}

template <typename Fn> static bool s_dispatchChildren(Instance *node, const vec2 &point, InteractionCategory category, Fn &fn);

template <typename Fn> static bool s_dispatchNode(Instance *inst, const vec2 &point, InteractionCategory category, Fn &fn)
{
    if (!inst->isHitTestVisible()) {
        return false;
    }

    // A handler further down may destroy this node, or the siblings we still have queued
    // (a row callback clearing its table). Once anything on the path dies the event counts
    // as handled: carrying on would walk freed nodes.
    std::weak_ptr<bool> alive = inst->aliveToken();

    bool pointInside = inst->containsScreenPoint(point);
    if (pointInside || !inst->getClipsDescendants()) {
        if (s_dispatchChildren(inst, point, category, fn)) {
            return true;
        }
        if (!Instance::isAlive(alive)) {
            return true;
        }
    }

    if (pointInside) {
        if (auto *obj = inst->asUiObject(); obj != nullptr && !obj->ignores(category)) {
            if (fn(obj) == EventResult::CONSUMED || !Instance::isAlive(alive)) {
                return true;
            }
        }
    }
    return false;
}

template <typename Fn> static bool s_dispatchChildren(Instance *node, const vec2 &point, InteractionCategory category, Fn &fn)
{
    std::vector<Instance *> children;
    s_treeChildrenTopFirst(node, children);
    for (Instance *child : children) {
        if (s_dispatchNode(child, point, category, fn)) {
            return true;
        }
    }
    return false;
}

template <typename Fn> static bool s_dispatchLayer(UILayer *layer, const vec2 &point, InteractionCategory category, Fn &fn)
{
    std::weak_ptr<bool> layerAlive = layer->aliveToken();
    for (const HitSegment &segment : s_hitSegments(layer)) {
        if (segment.node == nullptr) {
            if (s_dispatchChildren(layer, point, category, fn)) {
                return true;
            }
        } else if (Instance::isAlive(segment.alive) && s_reachable(segment.node, layer, point)) {
            if (UILayer *nested = segment.node->asLayer(); nested != nullptr) {
                if (nested->isHitTestVisible() && s_dispatchLayer(nested, point, category, fn)) {
                    return true;
                }
            } else if (s_dispatchNode(segment.node, point, category, fn)) {
                return true;
            }
        }
        if (!Instance::isAlive(layerAlive)) {
            return true;
        }
    }
    return false;
}

Instance *Window::findClickedObject(int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    vec2 point(x, y);
    Instance *result = nullptr;
    auto take = [&result](UIObject *obj) {
        result = obj;
        return EventResult::CONSUMED;
    };
    s_dispatchLayer(this, point, INTERACTION_CATEGORY_SCROLL, take);
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
            captured->onInputEnded(s_mapInputToLayout(*captured, input));
        } else {
            auto release = [&](UIObject *obj) -> EventResult {
                if (obj == press.object) {
                    press.released = true;
                }
                return obj->onInputEnded(s_mapInputToLayout(*obj, input));
            };
            s_dispatchLayer(this, vec2(x, y), INTERACTION_CATEGORY_CLICK, release);
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

    auto begin = [&](UIObject *obj) -> EventResult {
        std::weak_ptr<bool> alive = obj->aliveToken();
        EventResult result = obj->onInputBegan(s_mapInputToLayout(*obj, input));
        if (result == EventResult::CONSUMED && Instance::isAlive(alive)) {
            press = PressRecord{obj, alive, false};
        }
        return result;
    };
    s_dispatchLayer(this, point, INTERACTION_CATEGORY_CLICK, begin);
}

void Window::onMouseMove(int32_t x, int32_t y)
{
    AM_PROFILE_FUNCTION();
    m_lastMousePosition = vec2(x, y);
    m_hasMousePosition = true;

    const vec2 point(static_cast<float>(x), static_cast<float>(y));
    if (m_mouseCapturedBy != nullptr) {
        m_mouseCapturedBy->onMouseMoved(m_mouseCapturedBy->toLayout(point));
        return;
    }

    updateHover(x, y);

    for (int i = m_hoverCurrent.count - 1; i >= 0; --i) {
        const HoverEntry &entry = m_hoverCurrent.items[i];
        if (!Instance::isAlive(entry.alive)) {
            continue;
        }
        if (entry.object->onMouseMoved(entry.object->toLayout(point)) == EventResult::CONSUMED) {
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
    s_fillHoverLayer(this, point, hit.data(), hitCount, MAX_HOVER_DEPTH);
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
    if (yoffset == 0.0f) {
        return;
    }
    m_hoverStale = true;
    Instance *target = findClickedObject(x, y);
    vec2 point(x, y);

    while (target) {
        auto *uiObject = target->asUiObject();
        if (uiObject != nullptr && !uiObject->ignores(INTERACTION_CATEGORY_SCROLL)) {
            std::weak_ptr<bool> alive = target->aliveToken();
            const EventResult result = uiObject->handleMouseWheel(yoffset, point);
            if (result == EventResult::CONSUMED || !Instance::isAlive(alive)) {
                return;
            }
        }

        target = target->parent;
        // an ancestor that ignores the wheel is not there for it, so its rect cannot end the walk
        const auto *parentUiObject = target != nullptr ? target->asUiObject() : nullptr;
        const bool parentIgnoresScroll = parentUiObject != nullptr && parentUiObject->ignores(INTERACTION_CATEGORY_SCROLL);
        if (auto *parentObj = target ? target->asBase2D() : nullptr; parentObj != nullptr && !parentIgnoresScroll) {
            if (!parentObj->containsScreenPoint(point)) {
                target = nullptr;
            }
        }
    }
}

} // namespace Amethyst
