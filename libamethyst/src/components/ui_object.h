/*
 * Base class for drawable UI objects (frames, buttons, labels)
 */

#ifndef AMETHYST__UI_OBJECT_H
#define AMETHYST__UI_OBJECT_H

#include "components/common.h"
#include "components/input_events.h"
#include "components/ui_base_2d.h"
#include <cstdint>
#include <functional>

namespace Amethyst {

class UIObject : public UIBase2D {
  public:
    UIObject() = default;
    virtual ~UIObject() = default;

    void computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation);
    glm::mat4 buildTransform() const;
    InstanceData createInstanceData() const;

    virtual void onMouseEnter() {}
    virtual void onMouseLeave() {}
    virtual void onMouseMoved(uint32_t, uint32_t) {}
    virtual void onMouseButton1Down(uint32_t, uint32_t) {}
    virtual void onMouseButton1Up(uint32_t, uint32_t) {}
    virtual void onMouseButton1Click() {}
    virtual void onMouseButton2Down(uint32_t, uint32_t) {}
    virtual void onMouseButton2Up(uint32_t, uint32_t) {}
    virtual void onMouseButton2Click() {}
    virtual void onMouseScrollUp() {}
    virtual void onMouseScrollDown() {}

  public:
    bool active = false;
    glm::vec2 anchorPoint = glm::vec2(0.0f);
    AutomaticSize automaticSize = AutomaticSize::NONE;
    Color3 backgroundColor = {1.0f, 1.0f, 1.0f};
    float backgroundTransparency = 0.0f;
    BorderMode borderMode = BorderMode::OUTLINE;
    float borderPixelSize = 0.0f;
    Color3 borderColor = {0.0f, 0.0f, 0.0f};
    float borderTransparency = 0.0f;
    bool clipsDescendants = false;
    float cornerRadius = 0.0f;
    GuiState guiState = GuiState::IDLE;
    bool interactable = true;
    LayoutOrder layoutOrder = 0;

    UDim2 position;
    UDim2 size;
    Degrees rotation = 0.0f;
    bool visible = true;
    uint32_t zIndex = 0;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
