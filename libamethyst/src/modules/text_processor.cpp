/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

#include "utils/profiling.h"

#include <algorithm>
#include <cmath>

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
        return {(uint32_t(c & UTF8_4BYTE_BITS) << 18) | (uint32_t(b(1) & UTF8_CONT_BITS) << 12) |
                    (uint32_t(b(2) & UTF8_CONT_BITS) << 6) | (b(3) & UTF8_CONT_BITS),
                4};
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

    for (size_t i = 0; i < text.size();) {
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

struct ShapedGlyph {
    const GlyphInfo *info;
    float localX;
};

static uint16_t s_clampU16(float v)
{
    if (v <= 0.0f) {
        return 0;
    }
    if (v >= 65535.0f) {
        return 65535;
    }
    return static_cast<uint16_t>(v + 0.5f);
}

static void s_shapeText(const std::string &text, const TextLayoutParams &params, GlyphAtlas &atlas,
                        std::vector<std::vector<ShapedGlyph>> &lines, std::vector<float> &lineWidths, FontMetrics &metrics,
                        float &lineHeightPx)
{
    AM_PROFILE_FUNCTION();
    uint32_t pixelSize = static_cast<uint32_t>(params.fontSize);
    metrics = atlas.getMetrics(pixelSize);
    lineHeightPx = metrics.lineHeight * params.lineHeight;

    std::vector<ShapedGlyph> currentLine;
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
        const GlyphInfo *glyphInfo = atlas.getGlyph(codepoint, pixelSize);

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
            ShapedGlyph gi;
            gi.info = glyphInfo;
            gi.localX = currentLineWidth;
            currentLine.push_back(gi);
        }

        currentLineWidth += advance;
        i += charBytes;
    }
    flushLine();
}

std::vector<InstanceData> TextProcessor::layoutTextAtlas(const std::string &text, const TextLayoutParams &params) const
{
    AM_PROFILE_FUNCTION();
    std::vector<InstanceData> result;

    if (!m_glyphAtlas || text.empty()) {
        return result;
    }

    std::vector<std::vector<ShapedGlyph>> lines;
    std::vector<float> lineWidths;
    FontMetrics metrics;
    float lineHeightPx = 0.0f;
    s_shapeText(text, params, *m_glyphAtlas, lines, lineWidths, metrics, lineHeightPx);

    result.reserve(text.size());

    float atlasWidth = static_cast<float>(m_glyphAtlas->getWidth());
    float atlasHeight = static_cast<float>(m_glyphAtlas->getHeight());

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

            float posX = std::round(offsetX + gi.localX + glyphInfo->bearingX);
            float posY = std::round(baseline - glyphInfo->bearingY);

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
            inst.setTextRich(true);

            result.push_back(inst);
        }
    }

    return result;
}

BatchedText TextProcessor::layoutTextBatched(const std::string &text, const TextLayoutParams &params) const
{
    AM_PROFILE_FUNCTION();
    BatchedText out;

    if (m_glyphAtlas == nullptr || text.empty()) {
        return out;
    }

    std::vector<std::vector<ShapedGlyph>> lines;
    std::vector<float> lineWidths;
    FontMetrics metrics;
    float lineHeightPx = 0.0f;
    s_shapeText(text, params, *m_glyphAtlas, lines, lineWidths, metrics, lineHeightPx);

    if (lines.empty()) {
        return out;
    }

    float totalHeight = lines.size() * lineHeightPx;
    float startY = params.position.y;

    if (params.yAlign == TextYAlignment::CENTER) {
        startY += (params.bounds.y - totalHeight) * 0.5f;
    } else if (params.yAlign == TextYAlignment::BOTTOM) {
        startY += params.bounds.y - totalHeight;
    }

    float maxLineWidth = 0.0f;
    for (float w : lineWidths) {
        if (w > maxLineWidth) {
            maxLineWidth = w;
        }
    }

    // The render quad is sized from nominal font metrics (ascender/descender), but glyph ink
    // isn't bound by those metrics (accented caps, descenders on g/y/p/q/j, etc). Pad the quad
    // by the actual overshoot of the first/last line so that ink isn't clipped by the quad edge.
    float topOvershoot = 0.0f;
    for (const auto &gi : lines.front()) {
        topOvershoot = std::max(topOvershoot, gi.info->bearingY - metrics.ascender);
    }
    float bottomOvershoot = 0.0f;
    for (const auto &gi : lines.back()) {
        float glyphBottom = gi.info->height - gi.info->bearingY;
        bottomOvershoot = std::max(bottomOvershoot, glyphBottom + metrics.descender);
    }
    float topPad = std::ceil(std::max(topOvershoot, 0.0f));
    float bottomPad = std::ceil(std::max(bottomOvershoot, 0.0f));

    float boxX = std::round(params.position.x);
    float boxY = std::round(startY) - topPad;
    out.pos = vec2(boxX, boxY);
    out.size = vec2(std::max(params.bounds.x, maxLineWidth), totalHeight + topPad + bottomPad);
    out.lineHeightPx = lineHeightPx;

    size_t glyphTotal = 0;
    for (const auto &line : lines) {
        glyphTotal += line.size();
    }
    out.glyphs.reserve(glyphTotal);
    out.lines.reserve(lines.size());

    {
        AM_PROFILE_SCOPE("emit batched glyphs");
        for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
            float lineWidth = lineWidths[lineIdx];
            float offsetX = params.position.x;

            if (params.xAlign == TextXAlignment::CENTER) {
                offsetX += (params.bounds.x - lineWidth) * 0.5f;
            } else if (params.xAlign == TextXAlignment::RIGHT) {
                offsetX += params.bounds.x - lineWidth;
            }

            float baseline = startY + lineIdx * lineHeightPx + metrics.ascender;

            GlyphLine glyphLine;
            glyphLine.glyphStart = static_cast<uint32_t>(out.glyphs.size());
            glyphLine.glyphCount = 0;

            for (const auto &gi : lines[lineIdx]) {
                const GlyphInfo *glyphInfo = gi.info;

                float relX = offsetX + gi.localX + glyphInfo->bearingX - boxX;
                float relY = baseline - glyphInfo->bearingY - boxY;

                GlyphQuad quad;
                quad.posMin = packU16x2(s_clampU16(relX), s_clampU16(relY));
                quad.posMax = packU16x2(s_clampU16(relX + glyphInfo->width), s_clampU16(relY + glyphInfo->height));
                quad.uvMin = packU16x2(glyphInfo->x, glyphInfo->y);
                quad.uvMax = packU16x2(static_cast<uint16_t>(glyphInfo->x + glyphInfo->width),
                                       static_cast<uint16_t>(glyphInfo->y + glyphInfo->height));

                out.glyphs.push_back(quad);
                glyphLine.glyphCount++;
            }

            out.lines.push_back(glyphLine);
        }
    }

    return out;
}

} // namespace Amethyst
