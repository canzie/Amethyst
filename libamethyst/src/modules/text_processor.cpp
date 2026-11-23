/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

#include "logging/log.h"

#include "utils/packing.h"
#include "utils/profiling.h"
#include "utils/utf8.h"

#include <algorithm>
#include <cmath>

namespace Amethyst {

static constexpr uint32_t CP_TAB = 0x09u;
static constexpr uint32_t CP_LINE_FEED = 0x0Au;
static constexpr uint32_t CP_CARRIAGE_RETURN = 0x0Du;
static constexpr uint32_t CP_SPACE = 0x20u;
static constexpr uint32_t CP_ELLIPSIS = 0x2026u;
static constexpr uint32_t CP_FULL_STOP = 0x2Eu;

float TextProcessor::getCharAdvanceAtlas(FontId font, uint32_t codepoint, uint32_t pixelSize, float letterSpacing) const
{
    if (!m_glyphAtlas) {
        return 0.0f;
    }

    return m_glyphAtlas->getAdvance(font, codepoint, pixelSize) + letterSpacing;
}

// A scale that would divide by zero or poison every position falls back to 1. Window refuses
// such a scale, so one arriving here was set on a params struct by hand.
static float s_validScale(float logicalToPhysicalScale)
{
    if (!std::isfinite(logicalToPhysicalScale) || logicalToPhysicalScale <= 0.0f) {
        AM_LOG_WARN("text laid out with logical to physical scale {}, using 1", logicalToPhysicalScale);
        return 1.0f;
    }
    return logicalToPhysicalScale;
}

// Fonts rasterise at whole pixel sizes, so a physical size between two is rounded to the
// nearer one rather than down: 13px at 1.5x draws at 20px, not 19, keeping the logical size
// as close to what was asked as the grid allows. Never 0, which FreeType cannot render.
static uint32_t s_pixelSize(float physicalFontSize)
{
    if (!std::isfinite(physicalFontSize)) {
        AM_LOG_WARN("text laid out with font size {}, using 1px", physicalFontSize);
        return 1;
    }
    if (physicalFontSize < 1.0f) {
        return 1;
    }
    return static_cast<uint32_t>(std::lround(physicalFontSize));
}

// Layout runs in physical pixels, where the glyphs are rasterised: every logical length is
// scaled up here, and results are scaled back down where they leave the processor.
static TextLayoutParams s_toPhysical(const TextLayoutParams &params)
{
    float scale = s_validScale(params.logicalToPhysicalScale);
    TextLayoutParams physical = params;
    physical.position = params.position * scale;
    physical.bounds = params.bounds * scale;
    physical.fontSize = params.fontSize * scale;
    physical.letterSpacing = params.letterSpacing * scale;
    physical.strokeThickness = params.strokeThickness * scale;
    physical.logicalToPhysicalScale = 1.0f;
    return physical;
}

struct ShapedGlyph {
    const GlyphInfo *info;
    float localX;
    uint32_t codepoint;
};

// A render scale that would divide by zero or poison every position falls back to 1.
static float s_validRenderScale(float renderScale)
{
    if (!std::isfinite(renderScale) || renderScale <= 0.0f) {
        AM_LOG_WARN("text drawn at render scale {}, using 1", renderScale);
        return 1.0f;
    }
    return renderScale;
}

// The glyphs of a laid-out size, drawn that many times larger: rasterised at the whole pixel size nearest to
// what they cover on screen, so they are as sharp as an unscaled label while sitting where the layout put them.
struct RenderSize {
    float scale = 1.0f;
    uint32_t pixelSize = 0;
};

static RenderSize s_renderSize(uint32_t layoutPixelSize, float renderScale)
{
    RenderSize out;
    out.scale = renderScale;
    out.pixelSize = renderScale == 1.0f ? layoutPixelSize : s_pixelSize(static_cast<float>(layoutPixelSize) * renderScale);
    return out;
}

static const GlyphInfo *s_renderGlyph(GlyphAtlas &atlas, FontId font, const ShapedGlyph &shaped, uint32_t layoutPixelSize,
                                      const RenderSize &render)
{
    if (render.pixelSize == layoutPixelSize) {
        return shaped.info;
    }
    return atlas.getGlyph(font, shaped.codepoint, render.pixelSize);
}

// Shared so measurement and layout can never disagree on where tab stops land.
static float s_tabWidth(GlyphAtlas &atlas, FontId font, uint32_t pixelSize, float fontSize, float tabSize)
{
    float spaceAdvance = atlas.getAdvance(font, CP_SPACE, pixelSize);
    if (spaceAdvance <= 0.0f) {
        spaceAdvance = fontSize * 0.5f;
    }
    return std::max(tabSize, 1.0f) * spaceAdvance;
}

static void s_shapeText(const std::string &text, const TextLayoutParams &params, GlyphAtlas &atlas,
                        std::vector<std::vector<ShapedGlyph>> &lines, std::vector<float> &lineWidths, FontMetrics &metrics,
                        float &lineHeightPx)
{
    AM_PROFILE_FUNCTION();
    uint32_t pixelSize = s_pixelSize(params.fontSize);
    metrics = atlas.getMetrics(params.font, pixelSize);
    lineHeightPx = metrics.lineHeight * params.lineHeight;

    float tabWidth = s_tabWidth(atlas, params.font, pixelSize, params.fontSize, params.tabSize);

    std::vector<ShapedGlyph> currentLine;
    currentLine.reserve(text.size());
    float currentLineWidth = 0.0f;

    size_t wordStartIdx = 0;
    size_t wordStartGlyphIdx = 0;
    float wordStartWidth = 0.0f;

    // drops glyphs off the end of the line until an ellipsis fits after them, then places it; the font's own
    // ellipsis when it has one, three full stops otherwise
    auto appendEllipsis = [&]() {
        const GlyphInfo *ellipsis = atlas.getGlyph(params.font, CP_ELLIPSIS, pixelSize);
        uint32_t repeats = 1;
        if (ellipsis == nullptr) {
            ellipsis = atlas.getGlyph(params.font, CP_FULL_STOP, pixelSize);
            repeats = 3;
        }
        if (ellipsis == nullptr) {
            return;
        }

        const float ellipsisAdvance = ellipsis->advance + params.letterSpacing;
        const float ellipsisWidth = ellipsisAdvance * static_cast<float>(repeats);
        while (!currentLine.empty() && currentLineWidth + ellipsisWidth > params.bounds.x) {
            currentLineWidth = currentLine.back().localX;
            currentLine.pop_back();
        }
        const uint32_t ellipsisCodepoint = repeats == 1 ? CP_ELLIPSIS : CP_FULL_STOP;
        for (uint32_t i = 0; i < repeats; i++) {
            if (ellipsis->width > 0 && ellipsis->height > 0) {
                currentLine.push_back({.info = ellipsis, .localX = currentLineWidth, .codepoint = ellipsisCodepoint});
            }
            currentLineWidth += ellipsisAdvance;
        }
    };

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

        const GlyphInfo *glyphInfo = atlas.getGlyph(params.font, codepoint, pixelSize);
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

        const bool truncatesAtEnd = params.truncate == TextTruncate::AT_END || params.truncate == TextTruncate::ELLIPSIS;
        if (truncatesAtEnd && params.bounds.x > 0.0f && !params.wrap) {
            if (currentLineWidth + advance > params.bounds.x) {
                if (params.truncate == TextTruncate::ELLIPSIS) {
                    appendEllipsis();
                }
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
            gi.codepoint = codepoint;
            currentLine.push_back(gi);
        }

        currentLineWidth += advance;
        i += charBytes;
    }

    // A trailing hard break leaves an empty final line, matching editor semantics: "a\n"
    // is two lines, the second empty.
    flushLine(lastWasHardBreak);
}

FontMetrics TextProcessor::getMetricsAtlas(FontId font, float fontSize, float logicalToPhysicalScale) const
{
    if (m_glyphAtlas == nullptr) {
        return {};
    }
    float scale = s_validScale(logicalToPhysicalScale);
    FontMetrics metrics = m_glyphAtlas->getMetrics(font, s_pixelSize(fontSize * scale));
    metrics.ascender /= scale;
    metrics.descender /= scale;
    metrics.lineHeight /= scale;
    return metrics;
}

void TextProcessor::layoutTextRow(std::string_view text, const TextRowLayoutParams &params, TextRowLayout &out) const
{
    AM_PROFILE_FUNCTION();
    out.glyphs.clear();
    out.columnX.assign(text.size() + 1, 0.0f);
    out.width = 0.0f;

    if (m_glyphAtlas == nullptr) {
        return;
    }

    float scale = s_validScale(params.logicalToPhysicalScale);
    float fontSize = params.fontSize * scale;
    float letterSpacing = params.letterSpacing * scale;
    uint32_t pixelSize = s_pixelSize(fontSize);
    FontMetrics metrics = m_glyphAtlas->getMetrics(params.font, pixelSize);

    float tabWidth = s_tabWidth(*m_glyphAtlas, params.font, pixelSize, fontSize, params.tabSize);
    const RenderSize render = s_renderSize(pixelSize, s_validRenderScale(params.renderScale));

    float penX = 0.0f;
    size_t i = 0;
    while (i < text.size()) {
        Utf8::Decoded decoded = Utf8::decode(text, i);
        for (size_t byte = 0; byte < decoded.bytes && i + byte < text.size(); byte++) {
            out.columnX[i + byte] = penX;
        }

        if (decoded.codepoint == CP_TAB) {
            penX += tabWidth - std::fmod(penX, tabWidth);
            i += decoded.bytes;
            continue;
        }

        const GlyphInfo *info = m_glyphAtlas->getGlyph(params.font, decoded.codepoint, pixelSize);
        if (info == nullptr) {
            i += decoded.bytes;
            continue;
        }

        if (info->width > 0 && info->height > 0) {
            const ShapedGlyph shaped{.info = info, .localX = penX, .codepoint = decoded.codepoint};
            if (const GlyphInfo *drawn = s_renderGlyph(*m_glyphAtlas, params.font, shaped, pixelSize, render);
                drawn != nullptr && drawn->width > 0 && drawn->height > 0) {
                TextRowGlyph glyph;
                glyph.info = drawn;
                glyph.x = penX * render.scale + drawn->bearingX;
                glyph.y = metrics.ascender * render.scale - drawn->bearingY;
                out.glyphs.push_back(glyph);
            }
        }

        penX += info->advance + letterSpacing;
        i += decoded.bytes;
    }

    out.columnX[text.size()] = penX;
    out.width = penX;

    // the glyphs stay physical for the glyph buffer; the caret positions go back to logical
    if (scale != 1.0f) {
        for (float &x : out.columnX) {
            x /= scale;
        }
        out.width /= scale;
    }
}

vec2 TextProcessor::measureTextSize(const std::string &text, FontId font, float fontSize, float lineHeight,
                                    float logicalToPhysicalScale) const
{
    TextLayoutParams params;
    params.font = font;
    params.fontSize = fontSize;
    params.lineHeight = lineHeight;
    params.logicalToPhysicalScale = logicalToPhysicalScale;

    // a box is snapped to whole physical pixels, so the size is rounded up to one to never lose the last glyph
    float scale = s_validScale(logicalToPhysicalScale);
    vec2 size = measureTextAtlas(text, params);
    return vec2(std::ceil(size.x * scale), std::ceil(size.y * scale)) / scale;
}

vec2 TextProcessor::measureWrappedTextSize(const std::string &text, FontId font, float fontSize, float lineHeight, float wrapWidth,
                                           float logicalToPhysicalScale) const
{
    AM_PROFILE_FUNCTION();
    if (m_glyphAtlas == nullptr || text.empty()) {
        return {0.0f, 0.0f};
    }

    TextLayoutParams logicalParams;
    logicalParams.bounds = vec2(wrapWidth, 0.0f);
    logicalParams.font = font;
    logicalParams.fontSize = fontSize;
    logicalParams.lineHeight = lineHeight;
    logicalParams.wrap = true;
    logicalParams.logicalToPhysicalScale = logicalToPhysicalScale;

    float scale = s_validScale(logicalToPhysicalScale);
    TextLayoutParams params = s_toPhysical(logicalParams);

    std::vector<std::vector<ShapedGlyph>> lines;
    std::vector<float> lineWidths;
    FontMetrics metrics;
    float lineHeightPx = 0.0f;
    s_shapeText(text, params, *m_glyphAtlas, lines, lineWidths, metrics, lineHeightPx);

    float maxLineWidth = 0.0f;
    for (float width : lineWidths) {
        maxLineWidth = std::max(maxLineWidth, width);
    }

    // a box is snapped to whole physical pixels, so the size is rounded up to one to never lose the last glyph
    return vec2(std::ceil(maxLineWidth), std::ceil(static_cast<float>(lines.size()) * lineHeightPx)) / scale;
}

vec2 TextProcessor::measureTextAtlas(const std::string &text, const TextLayoutParams &logicalParams) const
{
    AM_PROFILE_FUNCTION();
    if (m_glyphAtlas == nullptr || text.empty()) {
        return {0.0f, 0.0f};
    }

    // measured at the size it will be drawn at, so the logical size matches the drawn glyphs
    float scale = s_validScale(logicalParams.logicalToPhysicalScale);
    TextLayoutParams params = s_toPhysical(logicalParams);

    uint32_t pixelSize = s_pixelSize(params.fontSize);
    float letterSpacing = params.letterSpacing;
    FontMetrics metrics = m_glyphAtlas->getMetrics(params.font, pixelSize);

    float tabWidth = s_tabWidth(*m_glyphAtlas, params.font, pixelSize, params.fontSize, params.tabSize);

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

        lineWidth += m_glyphAtlas->getAdvance(params.font, decoded.codepoint, pixelSize) + letterSpacing;
    }

    maxWidth = std::max(maxWidth, lineWidth);
    // same line pitch as s_shapeText, so a measured block is exactly as tall as its layout
    return vec2(maxWidth, static_cast<float>(lineCount) * metrics.lineHeight * params.lineHeight) / scale;
}

std::vector<InstanceData> TextProcessor::layoutTextAtlas(const std::string &text, const TextLayoutParams &logicalParams) const
{
    AM_PROFILE_FUNCTION();
    std::vector<InstanceData> result;

    if (!m_glyphAtlas || text.empty()) {
        return result;
    }

    float scale = s_validScale(logicalParams.logicalToPhysicalScale);
    TextLayoutParams params = s_toPhysical(logicalParams);

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
    const uint32_t layoutPixelSize = s_pixelSize(params.fontSize);
    const RenderSize render = s_renderSize(layoutPixelSize, s_validRenderScale(logicalParams.renderScale));
    // each glyph becomes an instance of its own, which the node maps into the window, render.scale times larger
    const float toLayout = scale * render.scale;

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
            const GlyphInfo *glyphInfo = s_renderGlyph(*m_glyphAtlas, params.font, gi, layoutPixelSize, render);
            if (glyphInfo == nullptr || glyphInfo->width == 0 || glyphInfo->height == 0) {
                continue;
            }

            float posX = std::round((offsetX + gi.localX) * render.scale + glyphInfo->bearingX);
            float posY = std::round(baseline * render.scale - glyphInfo->bearingY);

            float uvMinX = glyphInfo->x / atlasWidth;
            float uvMinY = glyphInfo->y / atlasHeight;
            float uvMaxX = (glyphInfo->x + glyphInfo->width) / atlasWidth;
            float uvMaxY = (glyphInfo->y + glyphInfo->height) / atlasHeight;

            float centerX = posX + glyphInfo->width * 0.5f;
            float centerY = posY + glyphInfo->height * 0.5f;

            // placed on whole physical pixels above, then handed back as a logical instance
            InstanceData inst{};
            inst.translation = vec2(centerX, centerY) / toLayout;
            inst.scale = vec2(glyphInfo->width, glyphInfo->height) / toLayout;
            inst.fillColor = packedColor;
            inst.setUvRect(vec4(uvMinX, uvMinY, uvMaxX, uvMaxY));
            inst.setPrimitiveType(PRIMITIVE_TEXT);
            inst.textureId = m_glyphAtlas->getTextureId(glyphInfo->page).id;
            inst.setTextRich(true);

            result.push_back(inst);
        }
    }

    return result;
}

BatchedText TextProcessor::layoutTextBatched(const std::string &text, const TextLayoutParams &logicalParams) const
{
    AM_PROFILE_FUNCTION();
    BatchedText out;

    if (m_glyphAtlas == nullptr || text.empty()) {
        return out;
    }

    float scale = s_validScale(logicalParams.logicalToPhysicalScale);
    TextLayoutParams params = s_toPhysical(logicalParams);

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

    const uint32_t layoutPixelSize = s_pixelSize(params.fontSize);
    const RenderSize render = s_renderSize(layoutPixelSize, s_validRenderScale(logicalParams.renderScale));
    const auto glyphFor = [&](const ShapedGlyph &shaped) {
        return s_renderGlyph(*m_glyphAtlas, params.font, shaped, layoutPixelSize, render);
    };

    // The render quad is sized from nominal font metrics (ascender/descender), but glyph ink
    // isn't bound by those metrics (accented caps, descenders on g/y/p/q/j, etc). Pad the quad
    // by the actual overshoot of the first/last line so that ink isn't clipped by the quad edge.
    // Measured in the pixels the glyphs are drawn at, and taken back to the layout below.
    float topOvershoot = 0.0f;
    for (const auto &gi : lines.front()) {
        if (const GlyphInfo *info = glyphFor(gi); info != nullptr) {
            topOvershoot = std::max(topOvershoot, info->bearingY - metrics.ascender * render.scale);
        }
    }
    float bottomOvershoot = 0.0f;
    for (const auto &gi : lines.back()) {
        if (const GlyphInfo *info = glyphFor(gi); info != nullptr) {
            float glyphBottom = info->height - info->bearingY;
            bottomOvershoot = std::max(bottomOvershoot, glyphBottom + metrics.descender * render.scale);
        }
    }
    float topPad = std::ceil(std::max(topOvershoot, 0.0f));
    float bottomPad = std::ceil(std::max(bottomOvershoot, 0.0f));

    // the box sits on whole physical pixels so every glyph inside it does too; the quad goes
    // back to logical pixels while the glyph data below stays physical. Drawn scaled, the box is
    // put on the window's pixel grid after it is mapped there instead.
    float boxX = std::round(params.position.x);
    float boxY = std::round(startY) - topPad / render.scale;
    out.pos = vec2(boxX, boxY) / scale;
    out.size = vec2(std::max(params.bounds.x, maxLineWidth), totalHeight + (topPad + bottomPad) / render.scale) / scale;
    out.lineHeightPx = lineHeightPx * render.scale;

    size_t glyphTotal = 0;
    for (const auto &line : lines) {
        glyphTotal += line.size();
    }
    out.glyphs.reserve(glyphTotal);
    out.lines.reserve(lines.size());

    uint32_t glyphColor = packColor(params.color);

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
                const GlyphInfo *glyphInfo = glyphFor(gi);
                if (glyphInfo == nullptr || glyphInfo->width == 0 || glyphInfo->height == 0) {
                    continue;
                }

                // the pen positions are the layout's, drawn render.scale times as far apart
                float relX = (offsetX + gi.localX - boxX) * render.scale + glyphInfo->bearingX;
                float relY = (baseline - boxY) * render.scale - glyphInfo->bearingY;

                GlyphQuad quad;
                quad.posMin = packU16x2(clampToU16(relX), clampToU16(relY));
                quad.posMax = packU16x2(clampToU16(relX + glyphInfo->width), clampToU16(relY + glyphInfo->height));
                quad.uvMin = packU16x2(glyphInfo->x, glyphInfo->y);
                quad.uvMax = packU16x2(static_cast<uint16_t>(glyphInfo->x + glyphInfo->width),
                                       static_cast<uint16_t>(glyphInfo->y + glyphInfo->height));
                quad.color = glyphColor;
                quad.textureId = m_glyphAtlas->getTextureId(glyphInfo->page).id;

                out.glyphs.push_back(quad);
                glyphLine.glyphCount++;
            }

            out.lines.push_back(glyphLine);
        }
    }

    return out;
}

} // namespace Amethyst
