/*
 * Numeric text input field: accepts only a valid number grammar
 */

#ifndef AMETHYST__NUMBER_INPUT_H
#define AMETHYST__NUMBER_INPUT_H

#include "components/ui_text_field.h"

#include <cstdint>

namespace Amethyst {

struct DrawContext;

/**
 * @brief Text input constrained to a numeric grammar.
 *
 * Guarantees that user edits only ever leave a valid (possibly partial, e.g. "-" or "1.")
 * number sequence. setText() is deliberately not filtered: callers such as Drag display
 * formatted values ("1.50 m", "1e-05") through it, and the parsers below read the numeric
 * prefix of whatever is there. It does not own a parsed value or emit a value signal. Use the
 * helpers to read the current text as a real number, and the inherited onTextChanged / onFocusLost to know
 * when it changed.
 */
class NumberInput : public UITextField {
  public:
    NumberInput();
    virtual ~NumberInput() = default;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    /**
     * @brief Parse the current buffer as a double, independent of the C locale.
     *
     * Reads the longest numeric prefix, so formatted text set through setText ("1.50 m")
     * still yields its number. An empty or unparsable buffer ("", "-", ".") is 0.0; a
     * magnitude beyond double clamps to the largest finite value.
     */
    double asDouble() const;

    /**
     * @brief Parse the current buffer as a 64-bit integer, independent of the C locale.
     *
     * Reads the integer part only, so "1.9" is 1 and "-1.9" is -1 (truncation toward zero).
     * An empty or unparsable buffer is 0; a magnitude beyond int64 clamps to INT64_MIN/MAX.
     */
    int64_t asInt64() const;

  public:
    bool allowDecimal = true;
    bool allowNegative = true;

  protected:
    bool acceptText(std::string_view candidate) const override;
};

} // namespace Amethyst

#endif // AMETHYST__NUMBER_INPUT_H
