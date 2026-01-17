/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

namespace Amethyst {

std::vector<CharacterInstance> TextProcessor::layoutText(const std::string &text, const TextLayoutParams &params) const
{
    std::vector<CharacterInstance> result;

    if (!m_fontData || text.empty()) {
        return result;
    }

    result.reserve(text.size());

    float scale = params.fontSize;
    float cursorX = params.position.x;
    float cursorY = params.position.y;
    uint32_t packedColor = packColor(params.color);

    for (char c : text) {
        uint32_t codepoint = static_cast<uint32_t>(c);
        uint32_t glyphIndex = m_fontData->getGlyphIndex(codepoint);

        const TTF::Glyph *glyph = m_fontData->getGlyph(glyphIndex);
        if (!glyph) {
            glyph = m_fontData->getGlyph(0);
        }
        if (!glyph) {
            continue;
        }

        if (TTF_IS_EMPTY(glyph->flags)) {
            cursorX += glyph->advanceWidth * scale + params.letterSpacing;
            continue;
        }

        glm::vec2 glyphSize = (glyph->bboxMax - glyph->bboxMin) * scale;
        float xOffset = glyph->leftSideBearing * scale;
        float yOffset = glyph->bboxMin.y * scale;

        CharacterInstance ch;
        ch.position = {cursorX + xOffset, cursorY - yOffset - glyphSize.y};
        ch.size = glyphSize;
        ch.glyphIndex = glyphIndex;
        ch.color = packedColor;

        result.push_back(ch);

        cursorX += glyph->advanceWidth * scale + params.letterSpacing;
    }

    return result;
}

} // namespace Amethyst
