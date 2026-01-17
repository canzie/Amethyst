/**
 * @file text_processor.h
 * @brief Text layout and CharacterInstance generation
 */

#ifndef AMETHYST_TEXT_PROCESSOR_H
#define AMETHYST_TEXT_PROCESSOR_H

#include "components/common.h"
#include "parsers/ttf/ttf_types.h"

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
     * @brief Set the font data to use for layout
     */
    void setFontData(const TTF::FontData *fontData) { m_fontData = fontData; }

    /**
     * @brief Layout a single line of text
     * @param text The text to layout
     * @param params Layout parameters
     * @return Vector of CharacterInstance data for rendering
     */
    std::vector<CharacterInstance> layoutText(const std::string &text, const TextLayoutParams &params) const;

    /**
     * @brief Measure text dimensions at fontSize = 1.0
     * @return Width and height of text
     */
    glm::vec2 measureText(const std::string &text, float letterSpacing = 0.0f) const;

  private:
    const TTF::FontData *m_fontData = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_PROCESSOR_H
