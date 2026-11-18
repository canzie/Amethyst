/**
 * @file draw_context.h
 * @brief Context for passing registries to draw calls
 */

#ifndef AMETHYST_DRAW_CONTEXT_H
#define AMETHYST_DRAW_CONTEXT_H

namespace Amethyst {

class GeometryRegistry;
class TextProcessor;
class GlyphAtlas;
class SvgAtlas;

/**
 * @brief Container for all registries used during drawing
 */
struct DrawContext {
    GeometryRegistry *geometry = nullptr;
    GeometryRegistry *overlay = nullptr;
    TextProcessor *textProcessor = nullptr;
    GlyphAtlas *glyphAtlas = nullptr;
    SvgAtlas *svgAtlas = nullptr;
    // physical (framebuffer) pixels per logical pixel of the window being drawn; text and SVGs
    // rasterise at their logical size times this, so they stay sharp on a scaled display
    float logicalToPhysicalScale = 1.0f;
};

} // namespace Amethyst

#endif // AMETHYST_DRAW_CONTEXT_H
