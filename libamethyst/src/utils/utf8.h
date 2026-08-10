/**
 * @file utf8.h
 * @brief UTF-8 decoding, encoding and boundary stepping
 *
 * One shared implementation for every place that walks text: the shaper, the text inputs
 * and (later) the editor view. Decoding is validating, so malformed input yields U+FFFD
 * rather than a plausible-looking wrong codepoint.
 */

#ifndef AMETHYST__UTF8_H
#define AMETHYST__UTF8_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Amethyst::Utf8 {

/** @brief U+FFFD REPLACEMENT CHARACTER, produced for any malformed sequence. */
constexpr uint32_t REPLACEMENT = 0xFFFDu;

/** @brief Longest valid UTF-8 sequence, in bytes. */
constexpr size_t MAX_SEQUENCE = 4;

/**
 * @brief One decoded codepoint and the number of bytes it occupied.
 *
 * On a malformed sequence, `codepoint` is REPLACEMENT and `bytes` is 1, so a decode loop
 * always advances and resynchronises on the next lead byte.
 */
struct Decoded {
    uint32_t codepoint = REPLACEMENT;
    size_t bytes = 1;
};

/**
 * @brief Decode the codepoint starting at a byte offset.
 *
 * Rejects overlong encodings, surrogate halves (U+D800..U+DFFF), codepoints above
 * U+10FFFF, bad continuation bytes and sequences truncated by the end of the string.
 *
 * @param text Text to read from.
 * @param pos Byte offset of the lead byte; must be < text.size().
 * @return The codepoint and its length in bytes.
 */
Decoded decode(std::string_view text, size_t pos);

/**
 * @brief Byte offset of the next codepoint boundary at or after a position.
 * @param text Text to walk.
 * @param pos Byte offset to step from.
 * @return The next boundary, clamped to text.size().
 */
size_t nextBoundary(std::string_view text, size_t pos);

/**
 * @brief Byte offset of the previous codepoint boundary before a position.
 * @param text Text to walk.
 * @param pos Byte offset to step back from.
 * @return The previous boundary, or 0 if pos is already at the start.
 */
size_t prevBoundary(std::string_view text, size_t pos);

/**
 * @brief Snap a byte offset onto the codepoint boundary at or before it.
 *
 * Used to keep externally supplied caret offsets from landing inside a sequence.
 *
 * @param text Text to align against.
 * @param pos Byte offset to align, clamped to text.size().
 * @return The boundary at or before pos.
 */
size_t alignToBoundary(std::string_view text, size_t pos);

/**
 * @brief Encode a codepoint as UTF-8.
 * @param codepoint Codepoint to encode; invalid values are encoded as REPLACEMENT.
 * @param out Receives the encoded bytes; not null-terminated.
 * @return Number of bytes written, 1 to MAX_SEQUENCE.
 */
size_t encode(uint32_t codepoint, char out[MAX_SEQUENCE]);

/**
 * @brief Number of codepoints in a string, counting each malformed byte as one.
 * @param text Text to count.
 * @return The codepoint count.
 */
size_t count(std::string_view text);

} // namespace Amethyst::Utf8

#endif // AMETHYST__UTF8_H
