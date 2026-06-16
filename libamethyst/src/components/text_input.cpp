/*
 * Free-form text input implementation
 */

#include "components/text_input.h"

#include "modules/style.h"

namespace Amethyst {

TextInput::TextInput()
{
    resolveStyle();
}

void TextInput::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::TEXT_INPUT, getClasses()));
    TextInputStyleProperties tiStyle;
    tiStyle.text = style.getTextStyle(ComponentType::TEXT_INPUT, getClasses());
    setTextInputProperties(tiStyle);
}

void TextInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

} // namespace Amethyst
