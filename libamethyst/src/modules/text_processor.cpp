/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

#include "utils/profiling.h"
#include "utils/utf8.h"

#include <algorithm>
#include <cmath>

namespace Amethyst {

static constexpr uint32_t CP_TAB = 0x09u;
static constexpr uint32_t CP_LINE_FEED = 0x0Au;
static constexpr uint32_t CP_CARRIAGE_RETURN = 0x0Du;
static constexpr uint32_t CP_SPACE = 0x20u;

float TextProcessor::getCharAdvanceAtlas(uint32_t codepoint, uint32_t pixelSize, float letterSpacing) const
{
    if (!m_glyphAtlas) {
        return 0.0f;
    }

    return m_glyphAtlas->getAdvance(codepoint, pixelSize) + letterSpacing;
}

struct ShapedGlyph {
    const GlyphInfo *info;
    float localX;
};

// Shared so measurement and layout can never disagree on where tab stops land.
static float s_tabWidth(GlyphAtlas &atlas, const TextLayoutParams &params, uint32_t pixelSize)
{
    float spaceAdvance = atlas.getAdvance(CP_SPACE, pixelSize);
    if (spaceAdvance <= 0.0f) {
        spaceAdvance = params.fontSize * 0.5f;
    }
    return std::max(params.tabSize, 1.0f) * spaceAdvance;
}

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

    float tabWidth = s_tabWidth(atlas, params, pixelSize);

    std::vector<ShapedGlyph> currentLine;
    currentLine.reserve(text.size());
    float currentLineWidth = 0.0f;

    size_t wordStartIdx = 0;
    size_t wordStartGlyphIdx = 0;
    float wordStartWidth = 0.0f;

    // force emits a row even with no glyphs on it, so blank lines do not collapse.
    auto flushLine = [&](bool force) {
        if (force || !currentLine.empty()) {
            lineWidths.push_back(currentLineWidth);
            lines.push_back(std::move(currentLine));
            currentLine.clear();
            currentLineWidth = 0.0f;
        }
        wordStartIdx = 0;
        wordStartGlyphIdx = 0;
        wordStartWidth = 0.0f;
    };

    bool lastWasHardBreak = false;
    size_t i = 0;
    while (i < text.size()) {
        Utf8::Decoded decoded = Utf8::decode(text, i);
        uint32_t codepoint = decoded.codepoint;
        size_t charBytes = decoded.bytes;

        // Hard line breaks: LF, CR and CRLF. These never reach the atlas; an unmapped
        // control codepoint would otherwise rasterize as .notdef and render as a box.
        if (codepoint == CP_LINE_FEED || codepoint == CP_CARRIAGE_RETURN) {
            if (codepoint == CP_CARRIAGE_RETURN && i + charBytes < text.size() &&
                Utf8::decode(text, i + charBytes).codepoint == CP_LINE_FEED) {
                charBytes += 1;
            }
            flushLine(true);
            lastWasHardBreak = true;
            i += charBytes;
            continue;
        }
        lastWasHardBreak = false;

        // A tab advances to the next tab stop and produces no ink, so it is resolved here
        // rather than through a glyph.
        if (codepoint == CP_TAB) {
            float advance = tabWidth - std::fmod(currentLineWidth, tabWidth);
            if (params.wrap && params.bounds.x > 0.0f && currentLineWidth + advance > params.bounds.x &&
                !currentLine.empty()) {
                flushLine(false);
            } else {
                currentLineWidth += advance;
            }
            wordStartIdx = i + charBytes;
            wordStartGlyphIdx = currentLine.size();
            wordStartWidth = currentLineWidth;
            i += charBytes;
            continue;
        }

        const GlyphInfo *glyphInfo = atlas.getGlyph(codepoint, pixelSize);
        if (!glyphInfo) {
            i += charBytes;
            continue;
        }

        float advance = glyphInfo->advance + params.letterSpacing;

        if (codepoint == CP_SPACE) {
            wordStartIdx = i + charBytes;
            wordStartGlyphIdx = currentLine.size();
            wordStartWidth = currentLineWidth + advance;
        }

        if (params.wrap && params.bounds.x > 0.0f) {
            if (currentLineWidth + advance > params.bounds.x && !currentLine.empty()) {
                if (params.truncate == TextTruncate::SPLIT_WORD || codepoint == CP_SPACE) {
                    flushLine(false);
                } else if (wordStartGlyphIdx > 0 && wordStartGlyphIdx < currentLine.size()) {
                    float removeWidth = currentLineWidth - wordStartWidth;
                    size_t resumeIdx = wordStartIdx;
                    currentLine.erase(currentLine.begin() + wordStartGlyphIdx, currentLine.end());
                    currentLineWidth -= removeWidth;
                    flushLine(false);
                    i = resumeIdx;
                    continue;
                } else {
                    flushLine(false);
                }
            }
        }

        if (params.truncate == TextTruncate::AT_END && params.bounds.x > 0.0f && !params.wrap) {
            if (currentLineWidth + advance > params.bounds.x) {
                // Skip to the next hard break so later lines still lay out; without this a
                // single long line would truncate the rest of the text away.
                while (i < text.size()) {
                    uint32_t skipped = Utf8::decode(text, i).codepoint;
                    if (skipped == CP_LINE_FEED || skipped == CP_CARRIAGE_RETURN) {
                        break;
                    }
                    i = Utf8::nextBoundary(text, i);
                }
                continue;
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

    // A trailing hard break leaves an empty final line, matching editor semantics: "a\n"
    // is two lines, the second empty.
    flushLine(lastWasHardBreak);
}

vec2 TextProcessor::measureTextAtlas(const std::string &text, uint32_t pixelSize, float letterSpacing) const
{
    TextLayoutParams params;
    params.fontSize = static_cast<float>(pixelSize);
    params.letterSpacing = letterSpacing;
    return measureTextAtlas(text, params);
}

vec2 TextProcessor::measureTextAtlas(const std::string &text, const TextLayoutParams &params) const
{
    AM_PROFILE_FUNCTION();
    if (m_glyphAtlas == nullptr || text.empty()) {
        return {0.0f, 0.0f};
    }

    uint32_t pixelSize = static_cast<uint32_t>(params.fontSize);
    float letterSpacing = params.letterSpacing;
    FontMetrics metrics = m_glyphAtlas->getMetrics(pixelSize);

    float tabWidth = s_tabWidth(*m_glyphAtlas, params, pixelSize);

    float maxWidth = 0.0f;
    float lineWidth = 0.0f;
    size_t lineCount = 1;

    size_t i = 0;
    while (i < text.size()) {
        Utf8::Decoded decoded = Utf8::decode(text, i);
        i += decoded.bytes;

        if (decoded.codepoint == CP_LINE_FEED || decoded.codepoint == CP_CARRIAGE_RETURN) {
            if (decoded.codepoint == CP_CARRIAGE_RETURN && i < text.size()) {
                Utf8::Decoded following = Utf8::decode(text, i);
                if (following.codepoint == CP_LINE_FEED) {
                    i += following.bytes;
                }
            }
            maxWidth = std::max(maxWidth, lineWidth);
            lineWidth = 0.0f;
            ++lineCount;
            continue;
        }

        if (decoded.codepoint == CP_TAB) {
            lineWidth += tabWidth - std::fmod(lineWidth, tabWidth);
            continue;
        }

        lineWidth += m_glyphAtlas->getAdvance(decoded.codepoint, pixelSize) + letterSpacing;
    }

    maxWidth = std::max(maxWidth, lineWidth);
    return {maxWidth, static_cast<float>(lineCount) * metrics.lineHeight};
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
