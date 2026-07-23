/*
 * Checkbox UI element
 */

#ifndef AMETHYST__CHECKBOX_H
#define AMETHYST__CHECKBOX_H

#include "components/common.h"
#include "components/image_label.h"
#include "components/properties.h"
#include "components/ui_button.h"
#include <functional>
#include <string>

namespace Amethyst {

struct Font;

class Checkbox : public UIButton {
  public:
    Checkbox();
    virtual ~Checkbox() = default;

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    bool setCheckboxProperties(const CheckboxStylePropertiesArgs &props);
    const CheckboxStyleProperties &getCheckboxProperties() const { return m_cbProps; }

    void setCheckIcon(std::string svg);

  protected:
    EventResult onMouseButton1Click() override;

  public:
    bool *value = nullptr;
    std::function<void(bool)> onValueChanged;

  protected:
    CheckboxStyleProperties m_cbProps;

  private:
    ImageLabel *m_checkIcon = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__CHECKBOX_H
