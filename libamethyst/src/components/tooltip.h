/*
 * Tooltip: a hover surface, and the stack that keeps nested surfaces in order.
 */

#ifndef AMETHYST__TOOLTIP_H
#define AMETHYST__TOOLTIP_H

#include "components/popup.h"
#include "math/math.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Amethyst {

class UIObject;
class Window;

class Tooltip : public Popup {
  public:
    Tooltip();
};

/**
 * @brief The tooltip surfaces of one window, held one per nesting depth rather than one per element
 */
class TooltipStack {
  public:
    explicit TooltipStack(Window *window) : m_window(window) {}

    /**
     * @brief Starts the delay after which a surface appears beside a point
     * @param owner The object the surface describes, whose depth decides which surface it takes
     * @param cursorPosition Absolute position the surface is placed beside
     * @param delaySeconds How long the cursor must rest before the surface appears
     * @param build Fills the surface the moment before it appears
     */
    void schedule(UIObject *owner, vec2 cursorPosition, float delaySeconds, std::function<void(Tooltip &)> build);

    /**
     * @brief Moves a pending appearance to a new point, doing nothing once the surface is up
     * @param cursorPosition Absolute position the surface is placed beside
     */
    void moveTo(vec2 cursorPosition);

    /**
     * @brief Drops a pending appearance and closes the surface an object owns, along with any nested in it
     * @param owner The object whose surface is going away
     */
    void cancel(UIObject *owner);

    void onTick(float deltaTime);

  public:
    vec2 cursorOffset = {14.0f, 18.0f};

  private:
    /**
     * @brief How many tooltip surfaces an object sits inside
     * @param object The object to measure
     * @return The object's nesting depth, 0 when it is in ordinary UI
     */
    static uint32_t depthOf(UIObject *object);

    Tooltip &surfaceAt(uint32_t depth);
    void closeFrom(uint32_t depth);

    Window *m_window = nullptr;
    // owned by the overlay they are added to, held here only to be reused at their depth
    std::vector<Tooltip *> m_surfaces;
    // the object each open surface belongs to, so a leave closes only what that object owns
    std::vector<UIObject *> m_owners;

    UIObject *m_pendingOwner = nullptr;
    std::function<void(Tooltip &)> m_pendingBuild;
    vec2 m_pendingPosition = {0.0f, 0.0f};
    float m_remaining = 0.0f;
};

} // namespace Amethyst

#endif // AMETHYST__TOOLTIP_H
