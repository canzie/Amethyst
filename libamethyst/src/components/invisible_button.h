#ifndef AMETHYST__INVISIBLE_BUTTON_H
#define AMETHYST__INVISIBLE_BUTTON_H

#include "components/ui_button.h"

namespace Amethyst {

struct Font;

class InvisibleButton : public UIButton {
  public:
    InvisibleButton(Instance *parent)
    {
        backgroundTransparency = 1.0f;
        setParent(parent);
    };
    virtual ~InvisibleButton();

    void draw(DrawContext &ctx) override;

  public:
};

} // namespace Amethyst

#endif // AMETHYST__INVISIBLE_BUTTON_H
