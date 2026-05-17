/*
 * Checkbox UI element with optional label
 */

#ifndef AMETHYST__CHECKBOX_H
#define AMETHYST__CHECKBOX_H

#include "components/common.h"
#include "components/ui_object.h"
#include <functional>
#include <string>

namespace Amethyst {

struct Font;

class Checkbox : public UIObject {
  public:
    Checkbox() = default;
    virtual ~Checkbox() = default;

    void draw(DrawContext &ctx) override;

  protected:
    EventResult onMouseButton1Click() override;

  public:
    bool *valueRef = nullptr;
    std::function<void(bool)> onValueChanged;

    std::string label;
    LabelSide labelSide = LabelSide::RIGHT;
    Font *font = nullptr;
    Color4 labelColor = {0.0f, 0.0f, 0.0f, 1.0f};
    TextXAlignment labelXAlignment = TextXAlignment::LEFT;
    TextYAlignment labelYAlignment = TextYAlignment::CENTER;

    Color3 checkColor = {0.0f, 0.0f, 0.0f};
    float checkTransparency = 0.0f;
    float checkboxSize = 20.0f;
    UDim labelPadding = UDim::fromOffset(5.0f);
};

} // namespace Amethyst

#endif // AMETHYST__CHECKBOX_H
