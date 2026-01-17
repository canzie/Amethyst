/**
 * @file draw_context.h
 * @brief Context for passing registries to draw calls
 */

#ifndef AMETHYST_DRAW_CONTEXT_H
#define AMETHYST_DRAW_CONTEXT_H

namespace Amethyst {

class GeometryRegistry;
class TextRegistry;
class TextProcessor;

/**
 * @brief Container for all registries used during drawing
 */
struct DrawContext {
    GeometryRegistry *geometry = nullptr;
    TextRegistry *text = nullptr;
    TextProcessor *textProcessor = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST_DRAW_CONTEXT_H
