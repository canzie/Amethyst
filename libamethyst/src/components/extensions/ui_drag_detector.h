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
#include <glm/glm.hpp>

namespace Amethyst {

class UIDragDetector : public UIExtension {
  public:
    explicit UIDragDetector(UIObject *owner) : UIExtension(owner), dragZone(owner) {}
    explicit UIDragDetector(UIObject *owner, UIObject *_dragZone) : UIExtension(owner), dragZone(_dragZone) {}
    virtual ~UIDragDetector() = default;

    void handleMouseDown(uint32_t x, uint32_t y);
    void handleMouseMove(uint32_t x, uint32_t y);
    void handleMouseUp(uint32_t x, uint32_t y);

    bool isDragging() const { return m_isDragging; }
    bool isSoftLockBroken() const { return m_softLockBroken; }

  public:
    DragMode mode = DragMode::FREE;
    UIObject *dragZone = nullptr;
    float softLockDistance = 30.0f;

    std::function<void(glm::vec2 startPos)> onDragStart;
    std::function<void(glm::vec2 delta, glm::vec2 position)> onDragUpdate;
    std::function<void(glm::vec2 endPos)> onDragEnd;

  private:
    bool m_isDragging = false;
    bool m_softLockBroken = false;
    glm::vec2 m_dragStartMouse = glm::vec2(0.0f);
    glm::vec2 m_dragStartOffset = glm::vec2(0.0f);
};

} // namespace Amethyst

#endif // AMETHYST__UI_DRAG_DETECTOR_H
