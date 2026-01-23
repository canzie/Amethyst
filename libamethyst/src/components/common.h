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

/**
 * @brief Opaque texture handle for bindless textures.
 * Backend provides these via registerTexture(), core just passes them through.
 */
struct AmTextureId {
    uint32_t id = UINT32_MAX;

    bool isValid() const { return id != UINT32_MAX; }
    explicit operator bool() const { return isValid(); }
    bool operator==(const AmTextureId &) const = default;
};

constexpr AmTextureId AM_INVALID_TEXTURE{UINT32_MAX};
using LayoutOrder = uint32_t;

struct UnifiedDimension {
    float scale = 0.0f;  // relative to parent (0.0 - 1.0)
    float offset = 0.0f; // absolute pixels

    static UnifiedDimension fromScale(float _scale) { return {_scale, 0.0f}; }
    static UnifiedDimension fromOffset(float _offset) { return {0.0f, _offset}; }

    float resolve(float parentSize) const { return scale * parentSize + offset; }
    bool operator==(const UnifiedDimension &) const = default;
};
using UDim = UnifiedDimension;

struct UnifiedDimension2 {
    glm::vec2 scale = {0.0f, 0.0f};
    glm::vec2 offset = {0.0f, 0.0f};

    UnifiedDimension2() = default;
    UnifiedDimension2(glm::vec2 _scale, glm::vec2 _offset) : scale(_scale), offset(_offset) {};
    UnifiedDimension2(float scaleX, float offsetX, float scaleY, float offsetY)
    {
        scale = glm::vec2(scaleX, scaleY);
        offset = glm::vec2(offsetX, offsetY);
    }

    static UnifiedDimension2 fromScale(float scaleX, float scaleY) { return {{scaleX, scaleY}, {0.0f, 0.0f}}; }
    static UnifiedDimension2 fromScale(float _scale) { return {{_scale, _scale}, {0.0f, 0.0f}}; }
    static UnifiedDimension2 fromOffset(float offsetX, float offsetY) { return {{0.0f, 0.0f}, {offsetX, offsetY}}; }
    static UnifiedDimension2 fromOffset(float _offset) { return {{0.0f, 0.0f}, {_offset, _offset}}; }

    glm::vec2 resolve(const glm::vec2 &parentSize) const { return scale * parentSize + offset; }

    bool operator==(const UnifiedDimension2 &) const = default;
};
using UDim2 = UnifiedDimension2;

struct UnifiedDimension4 {
    UDim top;
    UDim right;
    UDim bottom;
    UDim left;

    glm::vec4 resolve(glm::vec2 parentSize) const
    {
        return glm::vec4(top.resolve(parentSize.y), right.resolve(parentSize.x), bottom.resolve(parentSize.y),
                         left.resolve(parentSize.x));
    }
};
using UDim4 = UnifiedDimension4;

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
    SIBLING
};

enum class TextDirection {
    AUTO,
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT
};

enum class TextXAlignment {
    LEFT,
    CENTER,
    RIGHT,
};

enum class TextYAlignment {
    TOP,
    CENTER,
    BOTTOM,
};

enum class TextTruncate {
    NONE,
    AT_END,
    SPLIT_WORD,
};

enum PrimitiveType : uint8_t {
    PRIMITIVE_RECT,
    PRIMITIVE_CIRCLE,
    PRIMITIVE_TRIANGLE,
    PRIMITIVE_LINE,
    PRIMITIVE_COUNT,
};

enum class FillDirection {
    FILL_HORIZONTAL,
    FILL_VERTICAL,
};

enum class HorizontalAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER_H,
    ALIGN_RIGHT,
};

enum class VerticalAlignment {
    ALIGN_TOP,
    ALIGN_CENTER_V,
    ALIGN_BOTTOM,
};

enum class SortOrder {
    SORT_NAME,
    SORT_LAYOUT_ORDER,
};

enum class UiFlexAlignment {
    NONE,
    FILL,
    SPACE_AROUND,
    SPACE_BETWEEN,
    SPACE_EVENLY
};

enum class ItemLineAlignment {
    AUTOMATIC,
    START,
    CENTER,
    END,
    STRETCH
};

enum class LabelSide {
    LEFT,
    RIGHT,
    TOP,
    BOTTOM
};

struct InstanceData {
    alignas(16) glm::mat4 transform;
    alignas(16) Color4 fillColor;
    alignas(16) Color4 borderColor;
    float borderThickness;
    float cornerRadius;
    uint32_t primitiveType;
    uint32_t borderMode;
    uint32_t textureId = UINT32_MAX;
    uint32_t visible = 1;
    int32_t zIndex = 0;
};

inline constexpr uint32_t packColor(const Color4 &c)
{
    uint32_t r = static_cast<uint32_t>(c.r * 255.0f) & 0xFF;
    uint32_t g = static_cast<uint32_t>(c.g * 255.0f) & 0xFF;
    uint32_t b = static_cast<uint32_t>(c.b * 255.0f) & 0xFF;
    uint32_t a = static_cast<uint32_t>(c.a * 255.0f) & 0xFF;
    return r | (g << 8) | (b << 16) | (a << 24);
}

inline constexpr uint32_t packColor(const Color3 &c)
{
    return packColor(Color4(c, 1.0f));
}

inline constexpr uint32_t packColor(int r, int g, int b, int a)
{
    return packColor(Color4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
}

/**
 * @brief Per-character render data for text (GPU-aligned, 40 bytes)
 */
struct CharacterInstance {
    alignas(8) glm::vec2 position;
    alignas(8) glm::vec2 size;
    uint32_t glyphIndex;
    uint32_t color;
    uint32_t strokeColor;
    float strokeThickness;
};
static_assert(sizeof(CharacterInstance) == 32, "CharacterInstance must be 32 bytes");

} // namespace Amethyst

#endif // AMETHYST__COMMON_H
