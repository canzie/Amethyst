/*
 * Numeric text input field: accepts only a valid number grammar
 */

#ifndef AMETHYST__NUMBER_INPUT_H
#define AMETHYST__NUMBER_INPUT_H

#include "components/ui_input.h"

#include <cstdint>

namespace Amethyst {

struct DrawContext;

/**
 * @brief Text input constrained to a numeric grammar.
 *
 * Guarantees the buffer only ever holds a valid (possibly partial, e.g. "-" or "1.") number
 * sequence; it does not own a parsed value or emit a value signal. Use the helpers to read
 * the current text as a real number, and the inherited onTextChanged / onFocusLost to know
 * when it changed.
 */
class NumberInput : public UIInput {
  public:
    NumberInput();
    virtual ~NumberInput() = default;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    /**
     * @brief Parse the current buffer as a double; 0.0 for an empty or partial buffer.
     */
    double asDouble() const;

    /**
     * @brief Parse the current buffer as a 64-bit integer; 0 for an empty or partial buffer.
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
