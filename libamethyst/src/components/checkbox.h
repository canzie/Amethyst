/*
 * Checkbox UI element
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

    bool setCheckboxProperties(const CheckboxStyleProperties &props);
    const CheckboxStyleProperties &getCheckboxProperties() const { return m_cbProps; }

  protected:
    EventResult onMouseButton1Click() override;

  public:
    bool *valueRef = nullptr;
    std::function<void(bool)> onValueChanged;

  protected:
    CheckboxStyleProperties m_cbProps;
};

} // namespace Amethyst

#endif // AMETHYST__CHECKBOX_H
