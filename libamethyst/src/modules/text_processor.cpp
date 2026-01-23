/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

namespace Amethyst {

constexpr float TEXT_AA_PADDING = 1.0f;

std::vector<CharacterInstance> TextProcessor::layoutText(const std::string &text, const TextLayoutParams &params) const
{
    std::vector<CharacterInstance> result;

    if (!m_fontData || text.empty()) {
        return result;
    }

    result.reserve(text.size());

    float scale = params.fontSize;
    float padding = params.strokeThickness + TEXT_AA_PADDING;
    uint32_t packedColor = packColor(params.color);
    uint32_t packedStrokeColor = packColor(params.strokeColor);

    float maxAscent = 0.0f;
    for (char c : text) {
        uint32_t idx = m_fontData->getGlyphIndex(static_cast<uint32_t>(c));
        const TTF::Glyph *g = m_fontData->getGlyph(idx);
        if (g) maxAscent = std::max(maxAscent, g->bboxMax.y);
    }

    glm::vec2 textSize = measureText(text, params.letterSpacing) * scale;
    float lineHeightPx = textSize.y * params.lineHeight;

    std::vector<std::vector<CharacterInstance>> lines;
    std::vector<float> lineWidths;
    std::vector<CharacterInstance> currentLine;
    float currentLineWidth = 0.0f;
    size_t wordStartIdx = 0;
    size_t wordStartCharIdx = 0;
    float wordStartWidth = 0.0f;

    auto flushLine = [&]() {
        if (!currentLine.empty()) {
            lineWidths.push_back(currentLineWidth);
            lines.push_back(std::move(currentLine));
            currentLine.clear();
            currentLineWidth = 0.0f;
        }
        wordStartCharIdx = 0;
        wordStartWidth = 0.0f;
    };

    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);
        uint32_t glyphIndex = m_fontData->getGlyphIndex(codepoint);

        const TTF::Glyph *glyph = m_fontData->getGlyph(glyphIndex);
        if (!glyph) glyph = m_fontData->getGlyph(0);
        if (!glyph) continue;

        float advance = glyph->advanceWidth * scale + params.letterSpacing;

        if (c == ' ') {
            wordStartIdx = i + 1;
            wordStartCharIdx = currentLine.size();
            wordStartWidth = currentLineWidth + advance;
        }

        if (params.wrap && params.bounds.x > 0.0f) {
            if (currentLineWidth + advance > params.bounds.x && !currentLine.empty()) {
                if (params.truncate == TextTruncate::SPLIT_WORD || c == ' ') {
                    flushLine();
                } else if (wordStartCharIdx > 0 && wordStartCharIdx < currentLine.size()) {
                    float removeWidth = currentLineWidth - wordStartWidth;
                    currentLine.erase(currentLine.begin() + wordStartCharIdx, currentLine.end());
                    currentLineWidth -= removeWidth;
                    flushLine();
                    i = wordStartIdx - 1;
                    continue;
                } else {
                    flushLine();
                }
            }
        }

        if (params.truncate == TextTruncate::AT_END && params.bounds.x > 0.0f && !params.wrap) {
            if (currentLineWidth + advance > params.bounds.x) {
                break;
            }
        }

        if (TTF_IS_EMPTY(glyph->flags)) {
            currentLineWidth += advance;
            continue;
        }

        glm::vec2 glyphSize = (glyph->bboxMax - glyph->bboxMin) * scale;
        float xOffset = glyph->leftSideBearing * scale;
        float yOffset = (maxAscent - glyph->bboxMax.y) * scale;
        glm::vec2 paddedSize = glyphSize + glm::vec2(padding * 2.0f);

        CharacterInstance ch;
        ch.position = {currentLineWidth + xOffset - padding, yOffset - padding};
        ch.size = paddedSize;
        ch.glyphIndex = glyphIndex;
        ch.color = packedColor;
        ch.strokeColor = packedStrokeColor;
        ch.strokeThickness = params.strokeThickness;

        currentLine.push_back(ch);
        currentLineWidth += advance;
    }
    flushLine();

    float totalHeight = lines.size() * lineHeightPx;
    float startY = params.position.y;

    if (params.yAlign == TextYAlignment::CENTER) {
        startY += (params.bounds.y - totalHeight) * 0.5f;
    } else if (params.yAlign == TextYAlignment::BOTTOM) {
        startY += params.bounds.y - totalHeight;
    }

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
        float lineWidth = lineWidths[lineIdx];
        float offsetX = params.position.x;

        if (params.xAlign == TextXAlignment::CENTER) {
            offsetX += (params.bounds.x - lineWidth) * 0.5f;
        } else if (params.xAlign == TextXAlignment::RIGHT) {
            offsetX += params.bounds.x - lineWidth;
        }

        float offsetY = startY + lineIdx * lineHeightPx;

        for (auto &ch : lines[lineIdx]) {
            ch.position.x += offsetX;
            ch.position.y += offsetY;
            result.push_back(ch);
        }
    }

    return result;
}

glm::vec2 TextProcessor::measureText(const std::string &text, float letterSpacing) const
{
    if (!m_fontData || text.empty()) {
        return {0.0f, 0.0f};
    }

    float width = 0.0f;
    float maxHeight = 0.0f;

    for (size_t i = 0; i < text.size(); i++) {
        uint32_t codepoint = static_cast<uint32_t>(text[i]);
        uint32_t glyphIndex = m_fontData->getGlyphIndex(codepoint);

        const TTF::Glyph *glyph = m_fontData->getGlyph(glyphIndex);
        if (!glyph) {
            glyph = m_fontData->getGlyph(0);
        }
        if (!glyph) {
            continue;
        }

        width += glyph->advanceWidth;
        if (i < text.size() - 1) {
            width += letterSpacing;
        }

        float glyphHeight = glyph->bboxMax.y - glyph->bboxMin.y;
        maxHeight = std::max(maxHeight, glyphHeight);
    }

    return {width, maxHeight};
}

float TextProcessor::getCharAdvance(char c, float fontSize, float letterSpacing) const
{
    if (!m_fontData) {
        return 0.0f;
    }

    uint32_t codepoint = static_cast<uint32_t>(c);
    uint32_t glyphIndex = m_fontData->getGlyphIndex(codepoint);

    const TTF::Glyph *glyph = m_fontData->getGlyph(glyphIndex);
    if (!glyph) {
        glyph = m_fontData->getGlyph(0);
    }
    if (!glyph) {
        return 0.0f;
    }

    return glyph->advanceWidth * fontSize + letterSpacing;
}

} // namespace Amethyst
