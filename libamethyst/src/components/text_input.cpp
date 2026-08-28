/*
 * Free-form text input implementation
 */

#include "components/text_input.h"

namespace Amethyst {

TextInput::TextInput()
{
    resolveStyle();
}

void TextInput::resolveStyle()
{
    resolveTextInputStyle(ComponentType::TEXT_INPUT);
}

void TextInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

} // namespace Amethyst
