/*
 * Popup implementation
 */

#include "components/popup.h"

#include "components/overlay_layer.h"
#include "components/window.h"

#include <algorithm>

namespace Amethyst {

Popup::Popup()
{
    setBaseProperties({.visible = false});
}

void Popup::attachToOverlay()
{
    if (m_overlay != nullptr) {
        return;
    }

    Window *window = getWindow();
    if (window == nullptr) {
        return;
    }
    m_overlay = window->getOverlayLayer();
    if (m_overlay == nullptr || parent == m_overlay) {
        return;
    }

    m_logicalOwner = parent;
    reparent(m_overlay);
    m_ownerDestroyConn = m_logicalOwner->onDestroy.once([this](Instance *) {
        // onClosed may already take this popup down
        std::weak_ptr<bool> alive = aliveToken();
        close();
        if (isAlive(alive)) {
            m_overlay->removeChild(this);
        }
    });
}

void Popup::ensureConnected()
{
    if (!closeOnClickOutside || m_pressConn.connected() || m_overlay == nullptr) {
        return;
    }
    m_pressConn = m_overlay->onPressVote.connect([this](vec2 pos, PressVote &vote) {
        if (!m_open) {
            return;
        }
        if (containsPoint(pos)) {
            vote.add(EventResult::PROPAGATE);
            return;
        }
        vote.add(EventResult::CONSUMED);
        close();
    });
}

static vec2 s_placeAgainst(PopupPlacement placement, vec2 anchorPos, vec2 anchorSize, vec2 contentSize)
{
    switch (placement) {
    case PopupPlacement::ABOVE:
        return {anchorPos.x, anchorPos.y - contentSize.y};
    case PopupPlacement::LEFT:
        return {anchorPos.x - contentSize.x, anchorPos.y};
    case PopupPlacement::RIGHT:
        return {anchorPos.x + anchorSize.x, anchorPos.y};
    default:
        return {anchorPos.x, anchorPos.y + anchorSize.y};
    }
}

static PopupPlacement s_opposite(PopupPlacement placement)
{
    switch (placement) {
    case PopupPlacement::ABOVE:
        return PopupPlacement::BELOW;
    case PopupPlacement::LEFT:
        return PopupPlacement::RIGHT;
    case PopupPlacement::RIGHT:
        return PopupPlacement::LEFT;
    default:
        return PopupPlacement::ABOVE;
    }
}

/**
 * @brief How far a rect sticks out of the viewport along the axis a placement opens on
 */
static float s_overflow(PopupPlacement placement, vec2 pos, vec2 contentSize, vec2 viewport)
{
    bool horizontal = placement == PopupPlacement::LEFT || placement == PopupPlacement::RIGHT;
    float start = horizontal ? pos.x : pos.y;
    float extent = horizontal ? contentSize.x : contentSize.y;
    float limit = horizontal ? viewport.x : viewport.y;
    return std::max(0.0f, -start) + std::max(0.0f, start + extent - limit);
}

vec2 Popup::resolvePlacement(vec2 anchorPos, vec2 anchorSize, vec2 contentSize, vec2 viewport) const
{
    vec2 pos = s_placeAgainst(placement, anchorPos, anchorSize, contentSize) + offset;

    // clamping alone would slide a popup that does not fit back over its anchor (a submenu over its own row), so
    // open on the other side first when that side has more room
    float overflow = s_overflow(placement, pos, contentSize, viewport);
    if (overflow > 0.0f) {
        PopupPlacement flipped = s_opposite(placement);
        vec2 flippedPos = s_placeAgainst(flipped, anchorPos, anchorSize, contentSize) + offset;
        if (s_overflow(flipped, flippedPos, contentSize, viewport) < overflow) {
            pos = flippedPos;
        }
    }

    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, viewport.x - contentSize.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, viewport.y - contentSize.y));
    return pos;
}

void Popup::open(UIObject *anchor)
{
    if (anchor == nullptr) {
        return;
    }

    attachToOverlay();
    if (m_overlay == nullptr) {
        return;
    }

    vec2 viewport = m_overlay->absoluteSize;
    vec2 contentSize = getBaseProperties().size.resolve(viewport);
    if (matchAnchorWidth) {
        contentSize.x = anchor->absoluteSize.x;
    }

    vec2 pos = resolvePlacement(anchor->absolutePosition, anchor->absoluteSize, contentSize, viewport);

    if (matchAnchorWidth) {
        setBaseProperties({
            .position = UDim2::fromOffset(pos.x, pos.y),
            .size = UDim2::fromOffset(contentSize.x, contentSize.y),
            .visible = true,
        });
    } else {
        setBaseProperties({.position = UDim2::fromOffset(pos.x, pos.y), .visible = true});
    }

    ensureConnected();
    m_open = true;
    if (onOpened) {
        onOpened();
    }
}

void Popup::openAt(vec2 absolutePoint)
{
    attachToOverlay();
    if (m_overlay == nullptr) {
        return;
    }

    vec2 viewport = m_overlay->absoluteSize;
    vec2 contentSize = getBaseProperties().size.resolve(viewport);
    vec2 pos = resolvePlacement(absolutePoint, vec2(0.0f), contentSize, viewport);

    setBaseProperties({.position = UDim2::fromOffset(pos.x, pos.y), .visible = true});

    ensureConnected();
    m_open = true;
    if (onOpened) {
        onOpened();
    }
}

void Popup::close()
{
    if (!m_open) {
        return;
    }
    setBaseProperties({.visible = false});
    m_open = false;
    if (onClosed) {
        onClosed();
    }
}

} // namespace Amethyst
