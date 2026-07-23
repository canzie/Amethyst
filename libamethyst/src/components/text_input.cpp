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
    setTextInputProperties(style.getTextInputStyle(ComponentType::TEXT_INPUT, getClasses()));
}

void TextInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

} // namespace Amethyst
