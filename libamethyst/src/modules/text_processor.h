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
    float fontSize = 14.0f;
    Color4 color = {0.0f, 0.0f, 0.0f, 1.0f};
    float letterSpacing = 0.0f;
    float lineHeight = 1.0f;
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

  private:
    const TTF::FontData *m_fontData = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_PROCESSOR_H
