/*
 * Free-form single-line text input field
 */

#ifndef AMETHYST__TEXT_INPUT_H
#define AMETHYST__TEXT_INPUT_H

#include "components/ui_text_field.h"

namespace Amethyst {

struct DrawContext;

class TextInput : public UITextField {
  public:
    TextInput();
    virtual ~TextInput() = default;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_INPUT_H
