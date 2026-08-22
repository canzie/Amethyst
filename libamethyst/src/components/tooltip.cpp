#include "components/tooltip.h"

#include "components/overlay_layer.h"
#include "components/ui_object.h"
#include "components/window.h"

namespace Amethyst {

static constexpr int32_t TOOLTIP_Z_INDEX = 10000;

Tooltip::Tooltip()
{
    closeOnClickOutside = false;
    setBaseProperties({.interactable = false, .zIndex = TOOLTIP_Z_INDEX});
}

uint32_t TooltipStack::depthOf(UIObject *object)
{
    uint32_t depth = 0;
    for (Instance *current = object; current != nullptr; current = current->parent) {
        if (current->as<Tooltip>() != nullptr) {
            depth++;
        }
    }
    return depth;
}

Tooltip &TooltipStack::surfaceAt(uint32_t depth)
{
    OverlayLayer *overlay = m_window->getOverlayLayer();
    while (m_surfaces.size() <= depth) {
        m_surfaces.push_back(overlay->add<Tooltip>());
        m_owners.push_back(nullptr);
    }
    return *m_surfaces[depth];
}

void TooltipStack::closeFrom(uint32_t depth)
{
    for (size_t i = m_surfaces.size(); i > depth; i--) {
        m_surfaces[i - 1]->close();
        m_owners[i - 1] = nullptr;
    }
}

void TooltipStack::schedule(UIObject *owner, vec2 cursorPosition, float delaySeconds, std::function<void(Tooltip &)> build)
{
    m_pendingOwner = owner;
    m_pendingBuild = std::move(build);
    m_pendingPosition = cursorPosition;
    m_remaining = delaySeconds;
}

void TooltipStack::moveTo(vec2 cursorPosition)
{
    if (m_pendingOwner != nullptr) {
        m_pendingPosition = cursorPosition;
    }
}

void TooltipStack::cancel(UIObject *owner)
{
    if (m_pendingOwner == owner) {
        m_pendingOwner = nullptr;
        m_pendingBuild = {};
    }

    // a surface that is up belongs to one object, and whatever opened inside it goes with it
    for (size_t i = 0; i < m_owners.size(); i++) {
        if (m_owners[i] == owner) {
            closeFrom(static_cast<uint32_t>(i));
            return;
        }
    }
}

void TooltipStack::onTick(float deltaTime)
{
    if (m_pendingOwner == nullptr) {
        return;
    }

    m_remaining -= deltaTime;
    if (m_remaining > 0.0f) {
        return;
    }

    UIObject *owner = m_pendingOwner;
    std::function<void(Tooltip &)> build = std::move(m_pendingBuild);
    m_pendingOwner = nullptr;
    m_pendingBuild = {};

    uint32_t depth = depthOf(owner);
    closeFrom(depth);

    Tooltip &surface = surfaceAt(depth);
    if (build) {
        build(surface);
    }
    surface.openAt(m_pendingPosition + cursorOffset);
    m_owners[depth] = owner;
}

} // namespace Amethyst
