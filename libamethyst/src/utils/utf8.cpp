#include "utils/utf8.h"

namespace Amethyst::Utf8 {

static constexpr uint32_t MAX_CODEPOINT = 0x10FFFFu;
static constexpr uint32_t SURROGATE_LO = 0xD800u;
static constexpr uint32_t SURROGATE_HI = 0xDFFFu;

static bool s_isContinuation(unsigned char b)
{
    return (b & 0xC0u) == 0x80u;
}

Decoded decode(std::string_view text, size_t pos)
{
    if (pos >= text.size()) {
        return {};
    }

    auto byteAt = [&](size_t offset) { return static_cast<unsigned char>(text[pos + offset]); };
    const size_t available = text.size() - pos;
    const unsigned char lead = byteAt(0);

    if (lead < 0x80u) {
        return {lead, 1};
    }

    // Sequence length from the lead byte. 0x80..0xBF is a stray continuation byte and
    // 0xC0/0xC1/0xF5..0xFF can only ever start an overlong or out-of-range sequence.
    size_t length;
    uint32_t codepoint;
    if (lead >= 0xC2u && lead <= 0xDFu) {
        length = 2;
        codepoint = lead & 0x1Fu;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
        length = 3;
        codepoint = lead & 0x0Fu;
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
        length = 4;
        codepoint = lead & 0x07u;
    } else {
        return {};
    }

    if (available < length) {
        return {};
    }

    for (size_t i = 1; i < length; ++i) {
        unsigned char cont = byteAt(i);
        if (!s_isContinuation(cont)) {
            return {};
        }
        codepoint = (codepoint << 6) | (cont & 0x3Fu);
    }

    // Reject the encodings that are structurally valid but not canonical: overlongs, the
    // UTF-16 surrogate range, and anything past the Unicode maximum.
    static constexpr uint32_t MIN_FOR_LENGTH[MAX_SEQUENCE + 1] = {0, 0, 0x80u, 0x800u, 0x10000u};
    if (codepoint < MIN_FOR_LENGTH[length] || codepoint > MAX_CODEPOINT) {
        return {};
    }
    if (codepoint >= SURROGATE_LO && codepoint <= SURROGATE_HI) {
        return {};
    }

    return {codepoint, length};
}

size_t nextBoundary(std::string_view text, size_t pos)
{
    if (pos >= text.size()) {
        return text.size();
    }
    return pos + decode(text, pos).bytes;
}

size_t prevBoundary(std::string_view text, size_t pos)
{
    if (pos == 0) {
        return 0;
    }
    if (pos > text.size()) {
        pos = text.size();
    }

    // Walk back over continuation bytes to the lead byte, then confirm the sequence
    // starting there actually ends at pos. If it does not, the bytes are malformed and a
    // single-byte step is the honest answer.
    size_t start = pos - 1;
    size_t steps = 0;
    while (start > 0 && steps < MAX_SEQUENCE - 1 && s_isContinuation(static_cast<unsigned char>(text[start]))) {
        --start;
        ++steps;
    }

    if (start + decode(text, start).bytes == pos) {
        return start;
    }
    return pos - 1;
}

size_t alignToBoundary(std::string_view text, size_t pos)
{
    if (pos >= text.size()) {
        return text.size();
    }
    if (!s_isContinuation(static_cast<unsigned char>(text[pos]))) {
        return pos;
    }
    return prevBoundary(text, pos);
}

size_t encode(uint32_t codepoint, char out[MAX_SEQUENCE])
{
    if (codepoint > MAX_CODEPOINT || (codepoint >= SURROGATE_LO && codepoint <= SURROGATE_HI)) {
        codepoint = REPLACEMENT;
    }

    if (codepoint < 0x80u) {
        out[0] = static_cast<char>(codepoint);
        return 1;
    }
    if (codepoint < 0x800u) {
        out[0] = static_cast<char>(0xC0u | (codepoint >> 6));
        out[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        return 2;
    }
    if (codepoint < 0x10000u) {
        out[0] = static_cast<char>(0xE0u | (codepoint >> 12));
        out[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        return 3;
    }
    out[0] = static_cast<char>(0xF0u | (codepoint >> 18));
    out[1] = static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu));
    out[2] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
    out[3] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    return 4;
}

size_t count(std::string_view text)
{
    size_t total = 0;
    for (size_t i = 0; i < text.size(); i = nextBoundary(text, i)) {
        ++total;
    }
    return total;
}

} // namespace Amethyst::Utf8
