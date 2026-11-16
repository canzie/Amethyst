/*
 * printf-style number formats checked before they ever reach snprintf
 */

#ifndef AMETHYST__NUMBER_FORMAT_H
#define AMETHYST__NUMBER_FORMAT_H

#include <optional>
#include <string>
#include <string_view>

namespace Amethyst {

enum class NumberKind {
    INTEGER,
    FLOATING,
};

/**
 * @brief A user-supplied printf format reduced to something safe to hand to snprintf with one
 * value of a known kind: exactly one conversion, of a matching type, with the length modifier
 * rewritten to the argument actually passed (long long or double).
 */
class NumberFormat {
  public:
    /**
     * @brief Check a format against a value kind and build its safe form.
     * @param format printf-style format, e.g. "%.2f ms" or "%d px"; "%%" is a literal percent
     * @param kind Kind of value the format will receive
     * @return The checked format, or nullopt when it has no conversion, more than one, a '*'
     *         width/precision, an over-long width/precision or a conversion of the wrong type
     */
    static std::optional<NumberFormat> parse(std::string_view format, NumberKind kind);

    std::string format(long long value) const;
    std::string format(double value) const;

    NumberKind kind() const { return m_kind; }

    // an empty format that prints nothing; only useful as a placeholder before parse()
    NumberFormat() = default;

  private:
    std::string m_format; // with the length modifier fixed up
    NumberKind m_kind = NumberKind::INTEGER;
    bool m_unsigned = false; // o, u, x, X take an unsigned argument
};

} // namespace Amethyst

#endif // AMETHYST__NUMBER_FORMAT_H
