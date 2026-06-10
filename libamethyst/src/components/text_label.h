/*
 * Text display label
 */

#ifndef AMETHYST__TEXT_LABEL_H
#define AMETHYST__TEXT_LABEL_H

#include "components/instance.h"
#include "components/properties.h"
#include "components/ui_label.h"
#include "modules/text_processor.h"

#include <string>
#include <vector>

namespace Amethyst {

struct GeometryAllocation;

class TextLabel : public UILabel {
  public:
    TextLabel();
    virtual ~TextLabel();

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    bool setTextStyleProperties(const TextStyleProperties &props);
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
     * @brief Shifts the already-submitted glyphs in place (cheap path, only the origin moved).
     * @param ctx The draw context providing the geometry registry
     * @param delta The amount to move each glyph by
     * @param visible Whether the glyphs should be visible
     */
    void repositionGlyphs(DrawContext &ctx, vec2 delta, bool visible);

    /**
     * @brief Lays the text out from scratch and re-uploads the glyphs (expensive path).
     * @param ctx The draw context providing the text processor and geometry registry
     * @param effectiveFontSize The resolved font size after textScaled adjustment
     * @param zIndex The z-index to assign to the glyphs
     * @param visible Whether the glyphs should be visible
     */
    void reshapeGlyphs(DrawContext &ctx, float effectiveFontSize, int32_t zIndex, bool visible);

    std::vector<GeometryAllocation *> m_textAllocations;
    TextLayoutState m_textLayout;
    vec2 m_textSize = {0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_LABEL_H
