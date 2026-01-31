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

/**
 * @brief Container for all registries used during drawing
 */
struct DrawContext {
    GeometryRegistry *geometry = nullptr;
    GeometryRegistry *overlay = nullptr;
    TextProcessor *textProcessor = nullptr;
    GlyphAtlas *glyphAtlas = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_DRAW_CONTEXT_H
