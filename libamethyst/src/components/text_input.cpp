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

    auto &style = Style::instance();
    std::span<const StyleKey> classes = getClasses();
    TextInputStyleProperties oldBaseline = style.getTextInputStyle(ComponentType::TEXT_INPUT, classes, m_lastResolvedGuiState);
    TextInputStyleProperties resolved = style.getTextInputStyle(ComponentType::TEXT_INPUT, classes, effectiveGuiState());
    reconcileStyleOverrides(oldBaseline, resolved, getTextInputProperties(), [this](const TextInputStyleProperties &next) { setTextInputProperties(next); });
}

void TextInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

} // namespace Amethyst
