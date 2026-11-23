#include "ui_drag_detector.h"

#include "components/ui_base_2d.h"
#include "components/ui_object.h"
#include "components/window.h"

namespace Amethyst {

// the owner's position offset lives in its parent's layout space, which is scaled by every ancestor but not by the owner
static vec2 s_toParentLayout(const UIObject &owner, vec2 screenPoint)
{
    const UIBase2D *parentBase = owner.parent != nullptr ? owner.parent->asBase2D() : nullptr;
    if (parentBase != nullptr) {
        return parentBase->toLayout(screenPoint) - parentBase->absolutePosition;
    }
    return screenPoint;
}

void UIDragDetector::handleMouseDown(vec2 screenPoint)
{
    if (!enabled) {
        return;
    }

    // a press that reaches the owner outside the zone it is dragged by is not a drag
    if (dragZone != nullptr && dragZone != m_owner) {
        const vec2 point = dragZone->toLayout(screenPoint);
        const vec2 zoneMin = dragZone->absolutePosition;
        const vec2 zoneMax = zoneMin + dragZone->absoluteSize;
        if (point.x < zoneMin.x || point.y < zoneMin.y || point.x >= zoneMax.x || point.y >= zoneMax.y) {
            return;
        }
    }

    m_isDragging = true;
    m_softLockBroken = false;

    m_dragStartMouse = s_toParentLayout(*m_owner, screenPoint);
    m_dragStartOffset = m_owner->getBaseProperties().position.offset;

    if (onDragStart) {
        onDragStart(screenPoint);
    }

    if (auto *window = m_owner->getWindow()) {
        window->captureMouse(m_owner);
    }
}

void UIDragDetector::handleMouseMove(vec2 screenPoint)
{
    if (!m_isDragging) {
        return;
    }

    vec2 delta = s_toParentLayout(*m_owner, screenPoint) - m_dragStartMouse;

    switch (mode) {
    case DragMode::NONE:
        return;
    case DragMode::FREE:
        break;
    case DragMode::HORIZONTAL:
        delta.y = 0.0f;
        break;
    case DragMode::VERTICAL:
        delta.x = 0.0f;
        break;
    case DragMode::SOFT_HORIZONTAL:
        if (!m_softLockBroken) {
            if (abs(delta.y) > softLockDistance) {
                m_softLockBroken = true;
            } else {
                delta.y = 0.0f;
            }
        }
        break;
    case DragMode::SOFT_VERTICAL:
        if (!m_softLockBroken) {
            if (abs(delta.x) > softLockDistance) {
                m_softLockBroken = true;
            } else {
                delta.x = 0.0f;
            }
        }
        break;
    }

    UDim2 pos = m_owner->getBaseProperties().position;
    pos.offset = m_dragStartOffset + delta;
    m_owner->setBaseProperties({.position = pos});

    if (onDragUpdate) {
        onDragUpdate(delta, screenPoint);
    }
}

void UIDragDetector::handleMouseUp(vec2 screenPoint)
{
    if (!m_isDragging) {
        return;
    }

    m_isDragging = false;

    // Release capture before firing onDragEnd: onDragEnd may destroy m_owner
    // (e.g. torn-off tab drop rebuilds the tab button via setupTabButton).
    // After releaseMouse, m_mouseCapturedBy is null and onDestroy is cleared,
    // so the destruction that follows is safe.
    if (auto *window = m_owner->getWindow()) {
        window->releaseMouse(m_owner);
    }

    if (onDragEnd) {
        onDragEnd(screenPoint);
        // onDragEnd may have destroyed *this; no member access after this point.
    }
}

} // namespace Amethyst
