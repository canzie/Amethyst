/*
 * Extension that enables dragging behavior on a UIObject
 */

#ifndef AMETHYST__UI_DRAG_DETECTOR_H
#define AMETHYST__UI_DRAG_DETECTOR_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
#include "components/ui_object.h"
#include <cstdint>
#include <functional>
#include "math/math.h"

namespace Amethyst {

class UIDragDetector : public UIExtension {
  public:
    explicit UIDragDetector(UIObject *owner) : UIExtension(owner), dragZone(owner) {}
    explicit UIDragDetector(UIObject *owner, UIObject *_dragZone) : UIExtension(owner), dragZone(_dragZone) {}
    virtual ~UIDragDetector() = default;

    /**
     * @brief Starts a drag if the press lands in the drag zone.
     * @param screenPoint The pointer in window coordinates
     */
    void handleMouseDown(vec2 screenPoint);

    /**
     * @brief Moves the owner after the pointer, by the distance it covers in the owner's parent's layout space.
     * @param screenPoint The pointer in window coordinates
     */
    void handleMouseMove(vec2 screenPoint);

    /**
     * @brief Ends the drag.
     * @param screenPoint The pointer in window coordinates
     */
    void handleMouseUp(vec2 screenPoint);

    bool isDragging() const { return m_isDragging; }
    bool isSoftLockBroken() const { return m_softLockBroken; }

  public:
    DragMode mode = DragMode::FREE;
    /// the part of the owner a press has to land on to start a drag, the whole owner when it is the owner itself
    UIObject *dragZone = nullptr;
    float softLockDistance = 30.0f;

    // positions are in window coordinates; the delta is how far the owner moved in its parent's layout space
    std::function<void(vec2 startPos)> onDragStart;
    std::function<void(vec2 delta, vec2 position)> onDragUpdate;
    std::function<void(vec2 endPos)> onDragEnd;

  private:
    bool m_isDragging = false;
    bool m_softLockBroken = false;
    vec2 m_dragStartMouse = vec2(0.0f);
    vec2 m_dragStartOffset = vec2(0.0f);
};

} // namespace Amethyst

#endif // AMETHYST__UI_DRAG_DETECTOR_H
