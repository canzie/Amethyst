/*
 * Base class for drawable UI objects (frames, buttons, labels)
 */

#ifndef AMETHYST__UI_OBJECT_H
#define AMETHYST__UI_OBJECT_H

#include "components/common.h"
#include "components/ui_base_2d.h"
#include <cstdint>

namespace Amethyst {

class UIObject : public UIBase2D {
  public:
    UIObject() = default;
    virtual ~UIObject() = default;

    void computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation);
    glm::mat4 buildTransform() const;
    InstanceData createInstanceData() const;

  public:
    bool active;
    glm::vec2 anchorPoint = glm::vec2(0.0f);
    AutomaticSize automaticSize = AutomaticSize::NONE;
    Color3 backgroundColor = {1.0f, 1.0f, 1.0f};
    float backgroundTransparency = 0.0;
    BorderMode borderMode;
    float borderPixelSize = 0.0f;
    Color3 borderColor = {0.0f, 0.0f, 0.0f};
    float borderTransparency = 0.0f;
    bool clipsDescendants;
    float cornerRadius = 0.0f;
    GuiState guiState;
    bool interactable;
    LayoutOrder layoutOrder;

    UDim2 position;
    UDim2 size;
    Degrees rotation;
    bool visible;
    uint32_t zIndex;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
