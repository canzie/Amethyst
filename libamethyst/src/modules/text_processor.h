/**
 * @file text_processor.h
 * @brief Text layout and CharacterInstance generation
 */

#ifndef AMETHYST_TEXT_PROCESSOR_H
#define AMETHYST_TEXT_PROCESSOR_H

#include "components/common.h"
#include "modules/glyph_atlas.h"
#include "parsers/ttf/ttf_types.h"
#include "rendering/instance_data.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Amethyst {

/**
 * @brief Parameters for text layout
 */
struct TextLayoutParams {
    glm::vec2 position = {0.0f, 0.0f};
    glm::vec2 bounds = {0.0f, 0.0f};
    float fontSize = 14.0f;
    Color4 color = {0.0f, 0.0f, 0.0f, 1.0f};
    float letterSpacing = 0.0f;
    float lineHeight = 1.2f;
    float strokeThickness = 0.0f;
    Color4 strokeColor = {0.0f, 0.0f, 0.0f, 1.0f};
    TextXAlignment xAlign = TextXAlignment::LEFT;
    TextYAlignment yAlign = TextYAlignment::TOP;
    TextTruncate truncate = TextTruncate::NONE;
    bool wrap = false;
};

/**
 * @brief Processes text strings into CharacterInstance data for rendering
 */
class TextProcessor {
  public:
    /**
     * @brief Set the glyph atlas to use for layout
     */
    void setGlyphAtlas(GlyphAtlas *atlas) { m_glyphAtlas = atlas; }

    /**
     * @brief Layout text using glyph atlas (new implementation)
     * @param text The text to layout
     * @param params Layout parameters
     * @return Vector of InstanceData for rendering
     */
    std::vector<InstanceData> layoutTextAtlas(const std::string &text, const TextLayoutParams &params) const;

    /**
     * @brief Measure text dimensions using glyph atlas
     * @param text The text to measure
     * @param pixelSize Font size in pixels
     * @param letterSpacing Additional spacing between characters
     * @return Width and height of text
     */
    glm::vec2 measureTextAtlas(const std::string &text, uint32_t pixelSize, float letterSpacing = 0.0f) const;

    /**
     * @brief Get advance width for a character using glyph atlas
     * @param codepoint Unicode codepoint
     * @param pixelSize Font size in pixels
     * @param letterSpacing Additional spacing
     * @return Advance width in pixels
     */
    float getCharAdvanceAtlas(uint32_t codepoint, uint32_t pixelSize, float letterSpacing = 0.0f) const;

  private:
    const TTF::FontData *m_fontData = nullptr;
    GlyphAtlas *m_glyphAtlas = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_PROCESSOR_H
