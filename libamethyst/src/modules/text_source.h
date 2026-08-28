/**
 * @file text_source.h
 * @brief The text a TextArea reads and edits, which it does not own
 */

#ifndef AMETHYST__TEXT_SOURCE_H
#define AMETHYST__TEXT_SOURCE_H

#include <cstdint>
#include <string_view>

namespace Amethyst {

/**
 * @brief A line, and a byte offset within it.
 *
 * Columns are bytes rather than codepoints, because that is what the text is stored as.
 * Callers step them with the Utf8 helpers so a position never lands inside a sequence.
 */
struct TextPosition {
    uint64_t line = 0;
    uint64_t column = 0;

    bool operator==(const TextPosition &) const = default;
};

/**
 * @brief A half-open span of text, from start up to but not including end.
 */
struct TextRange {
    TextPosition start;
    TextPosition end;

    bool isEmpty() const { return start == end; }
};

/**
 * @brief Text a view reads by line and edits by range.
 *
 * Implementations are passive: a view pulls what it needs and is never called back, so the
 * text can live wherever the application keeps it without knowing a viewport exists.
 */
class TextSourceBase {
  public:
    virtual ~TextSourceBase() = default;

    /**
     * @brief Number of lines, where a trailing newline leaves a final empty line.
     */
    virtual uint64_t lineCount() const = 0;

    /**
     * @brief One line's bytes, without its terminator.
     *
     * The result is valid until the next replace().
     *
     * @param index Line to read; out of range yields an empty view
     * @return The line's bytes
     */
    virtual std::string_view line(uint64_t index) const = 0;

    /**
     * @brief Replace a range with new text, the single primitive every edit is built from.
     *
     * Inserting is a replace of an empty range, erasing is a replace with empty text. Undo
     * inverts one of these, and an incremental parser consumes the same record.
     *
     * @param range Span to remove
     * @param with Text to put in its place, which may contain newlines
     */
    virtual void replace(TextRange range, std::string_view with) = 0;

    /**
     * @brief Counter bumped by every edit, for a view to notice changes it did not make.
     */
    virtual uint64_t revision() const = 0;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_SOURCE_H
