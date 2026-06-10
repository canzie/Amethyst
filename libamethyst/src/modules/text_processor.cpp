/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

#include "utils/profiling.h"

#define UTF8_ASCII_MASK  0x80u
#define UTF8_2BYTE_MASK  0xE0u
#define UTF8_2BYTE_MARK  0xC0u
#define UTF8_2BYTE_BITS  0x1Fu
#define UTF8_3BYTE_MASK  0xF0u
#define UTF8_3BYTE_MARK  0xE0u
#define UTF8_3BYTE_BITS  0x0Fu
#define UTF8_4BYTE_MASK  0xF8u
#define UTF8_4BYTE_MARK  0xF0u
#define UTF8_4BYTE_BITS  0x07u
#define UTF8_CONT_BITS   0x3Fu
#define UTF8_REPLACEMENT 0xFFFDu

namespace Amethyst {

// Returns {codepoint, bytes_consumed}. On invalid sequence, returns {U+FFFD, 1}.
static std::pair<uint32_t, size_t> s_decodeUtf8(const std::string &text, size_t pos)
{
    auto b = [&](size_t offset) { return static_cast<unsigned char>(text[pos + offset]); };

    unsigned char c = b(0);
    if (c < UTF8_ASCII_MASK) {
        return {c, 1};
    }
    if ((c & UTF8_2BYTE_MASK) == UTF8_2BYTE_MARK && pos + 1 < text.size()) {
        return {(uint32_t(c & UTF8_2BYTE_BITS) << 6) | (b(1) & UTF8_CONT_BITS), 2};
    }
    if ((c & UTF8_3BYTE_MASK) == UTF8_3BYTE_MARK && pos + 2 < text.size()) {
        return {(uint32_t(c & UTF8_3BYTE_BITS) << 12) | (uint32_t(b(1) & UTF8_CONT_BITS) << 6) | (b(2) & UTF8_CONT_BITS), 3};
    }
    if ((c & UTF8_4BYTE_MASK) == UTF8_4BYTE_MARK && pos + 3 < text.size()) {
        return {(uint32_t(c & UTF8_4BYTE_BITS) << 18) | (uint32_t(b(1) & UTF8_CONT_BITS) << 12) | (uint32_t(b(2) & UTF8_CONT_BITS) << 6) | (b(3) & UTF8_CONT_BITS), 4};
    }
    return {UTF8_REPLACEMENT, 1};
}

vec2 TextProcessor::measureTextAtlas(const std::string &text, uint32_t pixelSize, float letterSpacing) const
{
    AM_PROFILE_FUNCTION();
    if (!m_glyphAtlas || text.empty()) {
        return {0.0f, 0.0f};
    }

    FontMetrics metrics = m_glyphAtlas->getMetrics(pixelSize);
    float width = 0.0f;

    for (size_t i = 0; i < text.size(); ) {
        auto [codepoint, charBytes] = s_decodeUtf8(text, i);
        i += charBytes;
        const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(codepoint, pixelSize);

        if (!glyphInfo) {
            continue;
        }

        width += glyphInfo->advance;
        if (i < text.size()) {
            width += letterSpacing;
        }
    }

    return {width, metrics.lineHeight};
}

float TextProcessor::getCharAdvanceAtlas(uint32_t codepoint, uint32_t pixelSize, float letterSpacing) const
{
    if (!m_glyphAtlas) {
        return 0.0f;
    }

    const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(codepoint, pixelSize);
    if (!glyphInfo) {
        return 0.0f;
    }

    return glyphInfo->advance + letterSpacing;
}

std::vector<InstanceData> TextProcessor::layoutTextAtlas(const std::string &text, const TextLayoutParams &params) const
{
    AM_PROFILE_FUNCTION();
    std::vector<InstanceData> result;

    if (!m_glyphAtlas || text.empty()) {
        return result;
    }

    result.reserve(text.size());

    uint32_t pixelSize = static_cast<uint32_t>(params.fontSize);
    FontMetrics metrics = m_glyphAtlas->getMetrics(pixelSize);

    float atlasWidth = static_cast<float>(m_glyphAtlas->getWidth());
    float atlasHeight = static_cast<float>(m_glyphAtlas->getHeight());
    float lineHeightPx = metrics.lineHeight * params.lineHeight;

    struct GlyphInstance {
        const GlyphInfo *info;
        float localX;
    };

    std::vector<std::vector<GlyphInstance>> lines;
    std::vector<float> lineWidths;
    std::vector<GlyphInstance> currentLine;
    currentLine.reserve(text.size());
    float currentLineWidth = 0.0f;

    size_t wordStartIdx = 0;
    size_t wordStartGlyphIdx = 0;
    float wordStartWidth = 0.0f;

    auto flushLine = [&]() {
        if (!currentLine.empty()) {
            lineWidths.push_back(currentLineWidth);
            lines.push_back(std::move(currentLine));
            currentLine.clear();
            currentLineWidth = 0.0f;
        }
        wordStartGlyphIdx = 0;
        wordStartWidth = 0.0f;
    };

    size_t i = 0;
    while (i < text.size()) {
        auto [codepoint, charBytes] = s_decodeUtf8(text, i);
        const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(codepoint, pixelSize);

        if (!glyphInfo) {
            i += charBytes;
            continue;
        }

        float advance = glyphInfo->advance + params.letterSpacing;

        if (codepoint == ' ') {
            wordStartIdx = i + charBytes;
            wordStartGlyphIdx = currentLine.size();
            wordStartWidth = currentLineWidth + advance;
        }

        if (params.wrap && params.bounds.x > 0.0f) {
            if (currentLineWidth + advance > params.bounds.x && !currentLine.empty()) {
                if (params.truncate == TextTruncate::SPLIT_WORD || codepoint == ' ') {
                    flushLine();
                } else if (wordStartGlyphIdx > 0 && wordStartGlyphIdx < currentLine.size()) {
                    float removeWidth = currentLineWidth - wordStartWidth;
                    currentLine.erase(currentLine.begin() + wordStartGlyphIdx, currentLine.end());
                    currentLineWidth -= removeWidth;
                    flushLine();
                    i = wordStartIdx;
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

        if (glyphInfo->width > 0 && glyphInfo->height > 0) {
            GlyphInstance gi;
            gi.info = glyphInfo;
            gi.localX = currentLineWidth;
            currentLine.push_back(gi);
        }

        currentLineWidth += advance;
        i += charBytes;
    }
    flushLine();

    float totalHeight = lines.size() * lineHeightPx;
    float startY = params.position.y;

    if (params.yAlign == TextYAlignment::CENTER) {
        startY += (params.bounds.y - totalHeight) * 0.5f;
    } else if (params.yAlign == TextYAlignment::BOTTOM) {
        startY += params.bounds.y - totalHeight;
    }

    uint32_t packedColor = packColor(params.color);
    uint32_t textureId = m_glyphAtlas->getTextureId().id;

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
        float lineWidth = lineWidths[lineIdx];
        float offsetX = params.position.x;

        if (params.xAlign == TextXAlignment::CENTER) {
            offsetX += (params.bounds.x - lineWidth) * 0.5f;
        } else if (params.xAlign == TextXAlignment::RIGHT) {
            offsetX += params.bounds.x - lineWidth;
        }

        float baseline = startY + lineIdx * lineHeightPx + metrics.ascender;

        for (const auto &gi : lines[lineIdx]) {
            const GlyphInfo *glyphInfo = gi.info;

            float posX = offsetX + gi.localX + glyphInfo->bearingX;
            float posY = baseline - glyphInfo->bearingY;

            float uvMinX = glyphInfo->x / atlasWidth;
            float uvMinY = glyphInfo->y / atlasHeight;
            float uvMaxX = (glyphInfo->x + glyphInfo->width) / atlasWidth;
            float uvMaxY = (glyphInfo->y + glyphInfo->height) / atlasHeight;

            float centerX = posX + glyphInfo->width * 0.5f;
            float centerY = posY + glyphInfo->height * 0.5f;

            InstanceData inst{};
            inst.translation = vec2(centerX, centerY);
            inst.scale = vec2(glyphInfo->width, glyphInfo->height);
            inst.fillColor = packedColor;
            inst.setUvRect(vec4(uvMinX, uvMinY, uvMaxX, uvMaxY));
            inst.setPrimitiveType(PRIMITIVE_TEXT);
            inst.textureId = textureId;

            result.push_back(inst);
        }
    }

    return result;
}

} // namespace Amethyst
