/*
 * Popup implementation
 */

#include "components/popup.h"

#include "components/overlay_layer.h"

#include <algorithm>

namespace Amethyst {

Popup::Popup()
{
    setBaseProperties({.visible = false});
}

void Popup::ensureConnected()
{
    if (!closeOnClickOutside || m_pressConn.connected()) {
        return;
    }
    OverlayLayer *overlay = parent != nullptr ? parent->as<OverlayLayer>() : nullptr;
    if (overlay == nullptr) {
        return;
    }
    m_pressConn = overlay->onPressVote.connect([this](vec2 pos, PressVote &vote) {
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

vec2 Popup::resolvePlacement(vec2 anchorPos, vec2 anchorSize, vec2 contentSize, vec2 viewport) const
{
    vec2 pos;
    switch (placement) {
    case PopupPlacement::ABOVE:
        pos = {anchorPos.x, anchorPos.y - contentSize.y};
        break;
    case PopupPlacement::LEFT:
        pos = {anchorPos.x - contentSize.x, anchorPos.y};
        break;
    case PopupPlacement::RIGHT:
        pos = {anchorPos.x + anchorSize.x, anchorPos.y};
        break;
    default:
        pos = {anchorPos.x, anchorPos.y + anchorSize.y};
        break;
    }
    pos += offset;
    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, viewport.x - contentSize.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, viewport.y - contentSize.y));
    return pos;
}

void Popup::open(UIObject *anchor)
{
    OverlayLayer *overlay = parent != nullptr ? parent->as<OverlayLayer>() : nullptr;
    if (overlay == nullptr || anchor == nullptr) {
        return;
    }

    vec2 viewport = overlay->absoluteSize;
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
    OverlayLayer *overlay = parent != nullptr ? parent->as<OverlayLayer>() : nullptr;
    if (overlay == nullptr) {
        return;
    }

    vec2 viewport = overlay->absoluteSize;
    vec2 contentSize = getBaseProperties().size.resolve(viewport);
    vec2 pos = absolutePoint + offset;
    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, viewport.x - contentSize.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, viewport.y - contentSize.y));

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
