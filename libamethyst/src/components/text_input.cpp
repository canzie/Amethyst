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
    resolveBaseStyle(ComponentType::TEXT_INPUT);

    TextInputStyleProperties resolved = Style::instance().getTextInputStyle(ComponentType::TEXT_INPUT, getClasses(), effectiveGuiState());
    if (m_tiProps.apply(resolved)) {
        markDirty();
    }
}

void TextInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

} // namespace Amethyst
