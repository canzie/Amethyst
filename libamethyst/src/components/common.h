/*
 * Common types for Amethyst UI system
 */

#ifndef AMETHYST__COMMON_H
#define AMETHYST__COMMON_H

#include <cstdint>
#include <glm/glm.hpp>

namespace Amethyst {

using Color3 = glm::vec3;
using Color4 = glm::vec4;
using Degrees = float;
using LayoutOrder = uint32_t;

struct UnifiedDimension {
    float scale = 0.0f;  // relative to parent (0.0 - 1.0)
    float offset = 0.0f; // absolute pixels

    float resolve(float parentSize) const { return scale * parentSize + offset; }
    bool operator==(const UnifiedDimension &) const = default;
};
using UDim = UnifiedDimension;

struct UnifiedDimension2 {
    glm::vec2 scale = {0.0f, 0.0f};
    glm::vec2 offset = {0.0f, 0.0f};

    static UnifiedDimension2 fromScale(float scaleX, float scaleY) { return {{scaleX, scaleY}, {0.0f, 0.0f}}; }

    static UnifiedDimension2 fromOffset(float offsetX, float offsetY) { return {{0.0f, 0.0f}, {offsetX, offsetY}}; }

    glm::vec2 resolve(const glm::vec2 &parentSize) const { return scale * parentSize + offset; }

    bool operator==(const UnifiedDimension2 &) const = default;
};
using UDim2 = UnifiedDimension2;

enum class AutomaticSize {
    NONE, // default
    X,    // Fit childs contents along x-axis
    Y,    // Fit childs contents along x-axis
    XY    // Fit childs contents along x and y-axis
};

enum class BorderMode {
    OUTLINE,
    MIDDLE,
    INSET
};

enum class GuiState {
    IDLE,
    HOVER,
    PRESS,
    NON_INTERCTABLE
};

enum class ZIndexBehavior {
    GLOBAL,
    SIBLING // relative to its siblings
};

enum class TextDirection {
    AUTO,
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT
};

} // namespace Amethyst

#endif // AMETHYST__COMMON_H
