/**
 * @file text_processor.h
 * @brief Text layout and CharacterInstance generation
 */

#ifndef AMETHYST_TEXT_PROCESSOR_H
#define AMETHYST_TEXT_PROCESSOR_H

#include "components/common.h"
#include "modules/glyph_atlas.h"
#include "modules/glyph_buffer.h"
#include "rendering/instance_data.h"

#include "math/math.h"
#include <string>
#include <vector>

namespace Amethyst {

/**
 * @brief Parameters for text layout
 */
struct TextLayoutParams {
    vec2 position = {0.0f, 0.0f};
    vec2 bounds = {0.0f, 0.0f};
    FontId font{};
    float fontSize = 14.0f;
    Color4 color = {0.0f, 0.0f, 0.0f, 1.0f};
    float letterSpacing = 0.0f;
    float lineHeight = 1.2f;
    float tabSize = 4.0f; // tab stop spacing, in space advances
    float strokeThickness = 0.0f;
    Color4 strokeColor = {0.0f, 0.0f, 0.0f, 1.0f};
    TextXAlignment xAlign = TextXAlignment::LEFT;
    TextYAlignment yAlign = TextYAlignment::TOP;
    TextTruncate truncate = TextTruncate::OFF;
    bool wrap = false;
};

/**
 * @brief Snapshot of the inputs a text layout was last built with, used by components to cache.
 *
 * A component rebuilds this each frame and compares with matches(): if true, only the origin
 * moved (scrolling) so cached glyphs can be shifted in place instead of re-shaped. A fontSize
 * of 0 marks the snapshot as unset, so it never matches a freshly built one. The origin travels
 * with the snapshot but is excluded from matches().
 */
struct TextLayoutState {
    float fontSize = 0.0f;
    vec2 bounds = {0.0f, 0.0f};
    float lineHeight = 0.0f;
    uint32_t color = 0;
    int32_t zIndex = 0;
    TextXAlignment xAlign = TextXAlignment::NONE;
    TextYAlignment yAlign = TextYAlignment::NONE;
    TextTruncate truncate = TextTruncate::NONE;
    bool wrap = false;
    vec2 origin = {0.0f, 0.0f};

    /**
     * @brief True if the shaped glyphs would be identical (every input except origin matches).
     * @param o The snapshot to compare against
     * @return Whether the cached layout can be reused by only moving it
     */
    bool matches(const TextLayoutState &o) const
    {
        return fontSize == o.fontSize && bounds == o.bounds && lineHeight == o.lineHeight && color == o.color &&
               zIndex == o.zIndex && xAlign == o.xAlign && yAlign == o.yAlign && truncate == o.truncate && wrap == o.wrap;
    }

    /**
     * @brief Mark the snapshot as unset so the next draw re-shapes from scratch.
     */
    void invalidate() { fontSize = 0.0f; }

    /**
     * @brief Whether the snapshot holds a usable layout (fontSize 0 means unset).
     */
    bool isValid() const { return fontSize != 0.0f; }
};

/**
 * @brief A laid-out text label reduced to a single quad plus its glyph side data.
 *
 * Glyph positions are stored inside the buffers as u16 pixels relative to `pos`; the quad
 * is `pos`/`size` in screen space. `lines` partitions `glyphs` into reading-order rows.
 */
struct BatchedText {
    vec2 pos = {0.0f, 0.0f};
    vec2 size = {0.0f, 0.0f};
    float lineHeightPx = 0.0f;
    std::vector<GlyphQuad> glyphs;
    std::vector<GlyphLine> lines;
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
     * @brief Lay out text as a single batched quad with glyph data in side buffers.
     * @param text The text to lay out.
     * @param params Layout parameters.
     * @return The bbox quad plus bbox-relative glyph quads and per-line ranges.
     */
    BatchedText layoutTextBatched(const std::string &text, const TextLayoutParams &params) const;

    /**
     * @brief Measure text dimensions using glyph atlas
     *
     * Runs the same shaper as layout, so hard breaks (LF/CR/CRLF) and tabs are honoured:
     * the width is the widest line and the height covers every line. Wrapping is not
     * applied; measure through the layout path if a wrap width matters.
     *
     * @param text The text to measure
     * @param params Layout parameters; fontSize, letterSpacing and tabSize are honoured
     * @return Width of the widest line, and the total height of all lines
     */
    vec2 measureTextAtlas(const std::string &text, const TextLayoutParams &params) const;

    /**
     * @brief Measure text at a pixel size, using default layout parameters otherwise.
     * @param text The text to measure
     * @param font Face to measure against; an invalid id uses the registry default
     * @param pixelSize Font size in pixels
     * @param letterSpacing Additional spacing between characters
     * @return Width of the widest line, and the total height of all lines
     */
    vec2 measureTextAtlas(const std::string &text, FontId font, uint32_t pixelSize, float letterSpacing = 0.0f) const;

    /**
     * @brief Get advance width for a character using glyph atlas
     * @param font Face to measure against; an invalid id uses the registry default
     * @param codepoint Unicode codepoint
     * @param pixelSize Font size in pixels
     * @param letterSpacing Additional spacing
     * @return Advance width in pixels
     */
    float getCharAdvanceAtlas(FontId font, uint32_t codepoint, uint32_t pixelSize, float letterSpacing = 0.0f) const;

  private:
    GlyphAtlas *m_glyphAtlas = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_PROCESSOR_H
