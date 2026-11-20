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
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

/**
 * @brief Round a logical position onto the physical pixel grid.
 *
 * Glyph bitmaps are rasterised in physical pixels, so text whose origin falls between two
 * physical pixels is sampled off its grid and blurs. At a fractional scale (1.25, 1.5) a whole
 * logical position can still land between pixels, which is why this rounds in physical space.
 * @param logical Position in logical pixels
 * @param logicalToPhysicalScale Physical pixels per logical pixel
 * @return The nearest logical position that sits on a whole physical pixel
 */
inline vec2 snapToPhysicalPixel(vec2 logical, float logicalToPhysicalScale)
{
    return vec2(std::round(logical.x * logicalToPhysicalScale), std::round(logical.y * logicalToPhysicalScale)) /
           logicalToPhysicalScale;
}

/**
 * @brief Parameters for text layout
 *
 * Positions, bounds, sizes and spacing are in logical pixels. Glyphs rasterise at fontSize
 * times logicalToPhysicalScale, so text stays sharp on a scaled display.
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
    float logicalToPhysicalScale = 1.0f; // physical pixels per logical pixel
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
    uint16_t font = FontId::INVALID;
    float fontSize = 0.0f;
    vec2 bounds = {0.0f, 0.0f};
    float lineHeight = 0.0f;
    uint32_t color = 0;
    TextXAlignment xAlign = TextXAlignment::NONE;
    TextYAlignment yAlign = TextYAlignment::NONE;
    TextTruncate truncate = TextTruncate::NONE;
    bool wrap = false;
    float logicalToPhysicalScale = 1.0f;
    vec2 origin = {0.0f, 0.0f};

    /**
     * @brief True if the shaped glyphs would be identical (every input except origin matches).
     * @param o The snapshot to compare against
     * @return Whether the cached layout can be reused by only moving it
     */
    bool matches(const TextLayoutState &o) const
    {
        return font == o.font && fontSize == o.fontSize && bounds == o.bounds && lineHeight == o.lineHeight && color == o.color &&
               xAlign == o.xAlign && yAlign == o.yAlign && truncate == o.truncate && wrap == o.wrap &&
               logicalToPhysicalScale == o.logicalToPhysicalScale;
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
 * The quad, `pos`/`size`, is in logical pixels like every other instance. The glyph side data
 * is in physical pixels, the space the glyphs were rasterised in: glyph positions are u16
 * pixels relative to `pos`, and `lineHeightPx` is the physical line pitch. `lines` partitions
 * `glyphs` into reading-order rows.
 */
struct BatchedText {
    vec2 pos = {0.0f, 0.0f};
    vec2 size = {0.0f, 0.0f};
    float lineHeightPx = 0.0f;
    std::vector<GlyphQuad> glyphs;
    std::vector<GlyphLine> lines;
};

/**
 * @brief What a row of text is laid out with, when the caller places the row itself.
 */
struct TextRowLayoutParams {
    FontId font{};
    float fontSize = 14.0f;
    float letterSpacing = 0.0f;
    float tabSize = 4.0f;                // tab stop spacing, in space advances
    float logicalToPhysicalScale = 1.0f; // physical pixels per logical pixel
};

/**
 * @brief One glyph's ink box within a row, relative to the row's top-left corner.
 *
 * In physical pixels, the same space as the glyph's atlas region in `info`, so the pair goes
 * into the glyph buffer as is.
 */
struct TextRowGlyph {
    const GlyphInfo *info = nullptr;
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief A row of text laid out on one line, with the pen position of every byte offset.
 *
 * `columnX` has one entry per byte plus one past the end, so a caret offset maps to an x
 * without a second walk. A multi-byte sequence stores its own start x in each of its byte
 * slots, so a lookup at a codepoint boundary is the left edge of that codepoint.
 *
 * `columnX` and `width` are in logical pixels, the glyphs in physical pixels.
 */
struct TextRowLayout {
    std::vector<TextRowGlyph> glyphs;
    std::vector<float> columnX;
    float width = 0.0f;
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
     * @brief Lay out one row of text on a single line, with no wrapping or alignment.
     *
     * Hard breaks are not honoured: a row is already one line. The caller places the row and
     * turns the glyphs into quads, so it decides scrolling, clipping and per-glyph colour.
     *
     * @param text The row's bytes, without a terminator
     * @param params Font and spacing to lay the row out with
     * @param out Receives the glyphs, the per-byte pen positions and the row's width
     */
    void layoutTextRow(std::string_view text, const TextRowLayoutParams &params, TextRowLayout &out) const;

    /**
     * @brief Vertical metrics of a font at a size.
     * @param font Face to read; an invalid id uses the registry default
     * @param fontSize Font size in logical pixels
     * @param logicalToPhysicalScale Physical pixels per logical pixel; the font is read at the
     * physical size, rounded to a whole pixel the same way layout rounds it
     * @return Ascender, descender and line height in logical pixels
     */
    FontMetrics getMetricsAtlas(FontId font, float fontSize, float logicalToPhysicalScale = 1.0f) const;

    /**
     * @brief Measure text dimensions using glyph atlas
     *
     * Runs the same shaper as layout, so hard breaks (LF/CR/CRLF) and tabs are honoured:
     * the width is the widest line and the height covers every line. Wrapping is not
     * applied; measure through the layout path if a wrap width matters.
     *
     * @param text The text to measure
     * @param params Layout parameters; font, fontSize, letterSpacing, lineHeight and tabSize are honoured
     * @return Width of the widest line, and the total height of all lines (lineCount * line height * lineHeight)
     */
    vec2 measureTextAtlas(const std::string &text, const TextLayoutParams &params) const;

    /**
     * @brief How large text is drawn at a display scale, so a box that size shows all of it
     * @param text The text to measure
     * @param font Face to measure against; an invalid id uses the registry default
     * @param fontSize Font size in logical pixels
     * @param lineHeight Line pitch as a multiple of the font's own
     * @param logicalToPhysicalScale Physical pixels per logical pixel of the display it is drawn on
     * @return Width of the widest line and height of all lines in logical pixels, each rounded up to a whole physical pixel
     */
    vec2 measureTextSize(const std::string &text, FontId font, float fontSize, float lineHeight, float logicalToPhysicalScale) const;

    /**
     * @brief How large text is drawn when it wraps at a width, so a box that size shows all of it
     * @param text The text to measure
     * @param font Face to measure against; an invalid id uses the registry default
     * @param fontSize Font size in logical pixels
     * @param lineHeight Line pitch as a multiple of the font's own
     * @param wrapWidth Width in logical pixels the lines wrap at
     * @param logicalToPhysicalScale Physical pixels per logical pixel of the display it is drawn on
     * @return Width of the widest line and height of all lines in logical pixels, each rounded up to a whole physical pixel
     */
    vec2 measureWrappedTextSize(const std::string &text, FontId font, float fontSize, float lineHeight, float wrapWidth,
                                float logicalToPhysicalScale) const;

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
