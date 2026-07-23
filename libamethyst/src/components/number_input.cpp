#include "components/number_input.h"

#include "modules/style.h"

#include <cstdlib>

namespace Amethyst {

NumberInput::NumberInput()
{
    resolveStyle();
}

void NumberInput::resolveStyle()
{
    resolveBaseStyle(ComponentType::TEXT_INPUT);

    auto &style = Style::instance();
    std::span<const StyleKey> classes = getClasses();
    TextInputStyleProperties oldBaseline = style.getTextInputStyle(ComponentType::TEXT_INPUT, classes, m_lastResolvedGuiState);
    TextInputStyleProperties resolved = style.getTextInputStyle(ComponentType::TEXT_INPUT, classes, effectiveGuiState());
    reconcileStyleOverrides(oldBaseline, resolved, getTextInputProperties(), [this](const TextInputStyleProperties &next) { setTextInputProperties(next); });
}

void NumberInput::draw(DrawContext &ctx)
{
    drawInput(ctx);
}

bool NumberInput::acceptText(std::string_view candidate) const
{
    bool seenDot = false;
    for (size_t i = 0; i < candidate.size(); ++i) {
        char ch = candidate[i];
        if (ch == '-') {
            if (!allowNegative || i != 0) {
                return false;
            }
        } else if (ch == '.') {
            if (!allowDecimal || seenDot) {
                return false;
            }
            seenDot = true;
        } else if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

double NumberInput::asDouble() const
{
    std::string text = getText();
    char *end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        return 0.0;
    }
    return value;
}

int64_t NumberInput::asInt64() const
{
    std::string text = getText();
    char *end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str()) {
        return 0;
    }
    return static_cast<int64_t>(value);
}

} // namespace Amethyst
