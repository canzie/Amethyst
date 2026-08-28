/*
 * Text button
 */

#ifndef AMETHYST__TEXT_BUTTON_H
#define AMETHYST__TEXT_BUTTON_H

#include "components/common.h"
#include "components/properties.h"
#include "components/ui_button.h"
#include "modules/glyph_buffer.h"
#include "modules/text_processor.h"

#include <string>

namespace Amethyst {

struct Font;
struct GeometryAllocation;

class TextButton : public UIButton {
  public:
    TextButton();
    virtual ~TextButton();

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    bool setTextStyleProperties(const TextStylePropertiesArgs &props);
    const TextStyleProperties &getTextStyleProperties() const { return m_textStyle; }

    void setText(std::string text);
    const std::string &getText() const { return m_text; }

    vec2 getTextSize() const { return m_textSize; }

  protected:
    TextStyleProperties m_textStyle;
    std::string m_text;

  private:
    /**
     * @brief Re-shapes or repositions the text glyphs based on what changed since last draw.
     * @param ctx The draw context providing the text processor and geometry registry
     */
    void updateTextGeometry(DrawContext &ctx);

    /**
     * @brief Shifts the text quad in place (cheap path, only the origin moved).
     * @param ctx The draw context providing the geometry registry
     * @param delta The amount to move the quad by
     * @param visible Whether the text should be visible
     */
    void repositionGlyphs(DrawContext &ctx, vec2 delta, bool visible);

    /**
     * @brief Lays the text out from scratch and re-uploads the glyph slice (expensive path).
     * @param ctx The draw context providing the text processor and geometry registry
     * @param effectiveFontSize The resolved font size after textScaled adjustment
     * @param zIndex The z-index to assign to the text quad
     * @param visible Whether the text should be visible
     */
    void reshapeGlyphs(DrawContext &ctx, float effectiveFontSize, int32_t zIndex, bool visible);

    /**
     * @brief Release the text quad and its glyph slice.
     * @param ctx The draw context providing the geometry registry
     */
    void releaseText(DrawContext &ctx);

    vec2 m_textSize = {0.0f, 0.0f};
    GeometryAllocation *m_textAlloc = nullptr;
    GlyphSliceHandle m_glyphSlice;
    TextLayoutState m_textLayout;
    FontId m_font;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_BUTTON_H
