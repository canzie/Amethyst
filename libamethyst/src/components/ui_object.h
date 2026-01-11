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

  public:
    bool active;
    glm::vec2 anchorPoint = glm::vec2(0.0f);
    AutomaticSize automaticSize = AutomaticSize::NONE;
    Color4 backgroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    BorderMode borderMode;
    float borderPixelSize;
    Color4 borderColor = {0.0f, 0.0f, 0.0f, 1.0f};
    bool clipsDescendants;
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
