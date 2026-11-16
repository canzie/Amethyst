#include "components/number_input.h"

#include <charconv>
#include <limits>
#include <string>
#include <system_error>

namespace Amethyst {

NumberInput::NumberInput()
{
    resolveStyle();
}

void NumberInput::resolveStyle()
{
    resolveTextInputStyle(ComponentType::TEXT_INPUT);
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

// std::from_chars rather than strtod/strtoll: those follow the C locale, so a German locale
// would read "1.5" as 1.
double NumberInput::asDouble() const
{
    std::string text = getText();
    const char *begin = text.data();
    const char *end = text.data() + text.size();

    double value = 0.0;
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec == std::errc::result_out_of_range) {
        // from_chars leaves value untouched here, so read the direction from the text: a
        // zero integer part or a negative exponent underflowed to 0, anything else
        // overflowed and clamps to the largest finite double
        bool negative = !text.empty() && text.front() == '-';
        size_t pos = negative ? 1 : 0;
        while (pos < text.size() && text[pos] == '0') {
            ++pos;
        }
        bool zeroIntegerPart = pos >= text.size() || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E';
        bool negativeExponent = text.find("e-") != std::string::npos || text.find("E-") != std::string::npos;
        if (zeroIntegerPart || negativeExponent) {
            return negative ? -0.0 : 0.0;
        }
        double max = std::numeric_limits<double>::max();
        return negative ? -max : max;
    }
    if (ec != std::errc{} || ptr == begin) {
        return 0.0;
    }
    return value;
}

int64_t NumberInput::asInt64() const
{
    // Only the integer part is read, so a decimal buffer truncates toward zero ("1.9" is 1,
    // "-1.9" is -1), and a magnitude past int64 clamps to its range instead of wrapping.
    std::string text = getText();
    const char *begin = text.data();
    const char *end = text.data() + text.size();

    int64_t value = 0;
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec == std::errc::result_out_of_range) {
        bool negative = !text.empty() && text.front() == '-';
        return negative ? std::numeric_limits<int64_t>::min() : std::numeric_limits<int64_t>::max();
    }
    if (ec != std::errc{} || ptr == begin) {
        return 0;
    }
    return value;
}

} // namespace Amethyst
