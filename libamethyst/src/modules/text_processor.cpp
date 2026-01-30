/**
 * @file text_processor.cpp
 * @brief Text layout implementation
 */

#include "modules/text_processor.h"

namespace Amethyst {

glm::vec2 TextProcessor::measureTextAtlas(const std::string &text, uint32_t pixelSize, float letterSpacing) const
{
    if (!m_glyphAtlas || text.empty()) {
        return {0.0f, 0.0f};
    }

    FontMetrics metrics = m_glyphAtlas->getMetrics(pixelSize);
    float width = 0.0f;

    for (size_t i = 0; i < text.size(); i++) {
        uint32_t codepoint = static_cast<uint32_t>(text[i]);
        const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(codepoint, pixelSize);

        if (!glyphInfo) {
            continue;
        }

        width += glyphInfo->advance;
        if (i < text.size() - 1) {
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

    for (size_t i = 0; i < text.size(); i++) {
        m_glyphAtlas->getGlyph(static_cast<uint32_t>(text[i]), pixelSize);
    }

    struct GlyphInstance {
        uint32_t codepoint;
        float localX;
    };

    std::vector<std::vector<GlyphInstance>> lines;
    std::vector<float> lineWidths;
    std::vector<GlyphInstance> currentLine;
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

    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);
        const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(codepoint, pixelSize);

        if (!glyphInfo) {
            continue;
        }

        float advance = glyphInfo->advance + params.letterSpacing;

        if (c == ' ') {
            wordStartIdx = i + 1;
            wordStartGlyphIdx = currentLine.size();
            wordStartWidth = currentLineWidth + advance;
        }

        if (params.wrap && params.bounds.x > 0.0f) {
            if (currentLineWidth + advance > params.bounds.x && !currentLine.empty()) {
                if (params.truncate == TextTruncate::SPLIT_WORD || c == ' ') {
                    flushLine();
                } else if (wordStartGlyphIdx > 0 && wordStartGlyphIdx < currentLine.size()) {
                    float removeWidth = currentLineWidth - wordStartWidth;
                    currentLine.erase(currentLine.begin() + wordStartGlyphIdx, currentLine.end());
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

        if (glyphInfo->width > 0 && glyphInfo->height > 0) {
            GlyphInstance gi;
            gi.codepoint = codepoint;
            gi.localX = currentLineWidth;
            currentLine.push_back(gi);
        }

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

        float baseline = startY + lineIdx * lineHeightPx + metrics.ascender;

        for (const auto &gi : lines[lineIdx]) {
            const GlyphInfo *glyphInfo = m_glyphAtlas->getGlyph(gi.codepoint, pixelSize);

            float posX = offsetX + gi.localX + glyphInfo->bearingX;
            float posY = baseline - glyphInfo->bearingY;

            float uvMinX = glyphInfo->x / atlasWidth;
            float uvMinY = glyphInfo->y / atlasHeight;
            float uvMaxX = (glyphInfo->x + glyphInfo->width) / atlasWidth;
            float uvMaxY = (glyphInfo->y + glyphInfo->height) / atlasHeight;

            float centerX = posX + glyphInfo->width * 0.5f;
            float centerY = posY + glyphInfo->height * 0.5f;

            InstanceData inst{};
            inst.transform = glm::mat4(1.0f);
            inst.transform[0] = glm::vec4(glyphInfo->width, 0.0f, 0.0f, 0.0f);
            inst.transform[1] = glm::vec4(0.0f, glyphInfo->height, 0.0f, 0.0f);
            inst.transform[2] = glm::vec4(uvMinX, uvMinY, uvMaxX, uvMaxY);
            inst.transform[3] = glm::vec4(centerX, centerY, 0.0f, 1.0f);

            inst.fillColor = params.color;
            inst.borderColor = glm::vec4(0.0f);
            inst.borderThickness = 0.0f;
            inst.cornerRadius = 0.0f;
            inst.primitiveType = PRIMITIVE_TEXT;
            inst.borderMode = 0;
            inst.textureId = m_glyphAtlas->getTextureId().id;
            inst.zIndex = 0;

            result.push_back(inst);
        }
    }

    return result;
}

} // namespace Amethyst
