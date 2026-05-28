#include "ui_drag_detector.h"

#include "components/ui_base_2d.h"
#include "components/ui_object.h"
#include "components/window.h"

namespace Amethyst {

void UIDragDetector::handleMouseDown(uint32_t x, uint32_t y)
{
    if (!enabled) {
        return;
    }

    m_isDragging = true;
    m_softLockBroken = false;

    glm::vec2 mousePosParentRelative(x, y);
    if (m_owner->parent) {
        auto *parentBase = m_owner->parent->as<UIBase2D>();
        if (parentBase) {
            mousePosParentRelative -= parentBase->absolutePosition;
        }
    }

    m_dragStartMouse = mousePosParentRelative;
    m_dragStartOffset = m_owner->getBaseProperties().position.offset;

    if (onDragStart) {
        onDragStart(glm::vec2(x, y));
    }

    if (auto *window = m_owner->getWindow()) {
        window->captureMouse(m_owner);
    }
}

void UIDragDetector::handleMouseMove(uint32_t x, uint32_t y)
{
    if (!m_isDragging) {
        return;
    }

    glm::vec2 mousePosParentRelative(x, y);
    if (m_owner->parent) {
        auto *parentBase = m_owner->parent->as<UIBase2D>();
        if (parentBase) {
            mousePosParentRelative -= parentBase->absolutePosition;
        }
    }

    glm::vec2 delta = mousePosParentRelative - m_dragStartMouse;

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
            if (glm::abs(delta.y) > softLockDistance) {
                m_softLockBroken = true;
            } else {
                delta.y = 0.0f;
            }
        }
        break;
    case DragMode::SOFT_VERTICAL:
        if (!m_softLockBroken) {
            if (glm::abs(delta.x) > softLockDistance) {
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
        onDragUpdate(delta, glm::vec2(x, y));
    }
}

void UIDragDetector::handleMouseUp(uint32_t x, uint32_t y)
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
        onDragEnd(glm::vec2(x, y));
        // onDragEnd may have destroyed *this; no member access after this point.
    }
}

} // namespace Amethyst
