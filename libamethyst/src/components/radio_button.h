/*
 * RadioButton UI element. A RadioGroup holds the shared selected value and
 * an EventSignal; every RadioButton bound to the same group (including the
 * one that changed it) receives the signal and just marks itself dirty, so
 * the next arrange()/draw() re-checks whether it's the selected one. Drawn
 * as a single circle whose fill comes from the normal background-color /
 * background-transparency style fields, which already resolve differently
 * for the :active pseudo-state, so no dedicated style property is needed.
 */

#ifndef AMETHYST__RADIO_BUTTON_H
#define AMETHYST__RADIO_BUTTON_H

#include "components/common.h"
#include "components/ui_button.h"
#include "modules/event_signal.h"
#include <cstdint>

namespace Amethyst {

class RadioGroup {
  public:
    void select(int32_t v)
    {
        if (value == v) {
            return;
        }
        value = v;
        onChanged.fire();
    }

    int32_t value = 0;
    EventSignal<void()> onChanged;
};

class RadioButton : public UIButton {
  public:
    RadioButton();
    virtual ~RadioButton() = default;

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    void setGroup(RadioGroup *group);

  protected:
    EventResult onMouseButton1Click() override;

  public:
    int32_t value = 0;
    EventSignal<void(int32_t)> onSelected;

  private:
    RadioGroup *m_group = nullptr;
    EventConnection m_groupConn;
};

} // namespace Amethyst

#endif // AMETHYST__RADIO_BUTTON_H
