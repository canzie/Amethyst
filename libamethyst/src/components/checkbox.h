/*
 * Checkbox UI element with optional label
 */

#ifndef AMETHYST__CHECKBOX_H
#define AMETHYST__CHECKBOX_H

#include "components/common.h"
#include "components/properties.h"
#include "components/ui_button.h"
#include <functional>

namespace Amethyst {

struct Font;

class Checkbox : public UIButton {
  public:
    Checkbox();
    virtual ~Checkbox() = default;

    void draw(DrawContext &ctx) override;

    bool setCheckboxProperties(const CheckboxProperties &props);
    const CheckboxProperties &getCheckboxProperties() const { return m_cbProps; }

  protected:
    EventResult onMouseButton1Click() override;

  public:
    bool *valueRef = nullptr;
    std::function<void(bool)> onValueChanged;

  protected:
    CheckboxProperties m_cbProps;
};

} // namespace Amethyst

#endif // AMETHYST__CHECKBOX_H
