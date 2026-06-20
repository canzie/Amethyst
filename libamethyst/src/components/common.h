/*
 * Common types for Amethyst UI system
 */

#ifndef AMETHYST__COMMON_H
#define AMETHYST__COMMON_H

#include "modules/color.h"

#include "math/math.h"
#include <cstddef>
#include <cstdint>

namespace Amethyst {

using Degrees = float;
using am_bool = int8_t;
static_assert(static_cast<bool>(am_bool{0}) == false, "am_bool: 0 must convert to false");
static_assert(static_cast<bool>(am_bool{1}) == true, "am_bool: a positive value must convert to true");

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

/**
 * @brief Opaque buffer handle for backend GPU buffers.
 * Backend provides these via createBuffer(), core passes them back into buffer ops.
 */
struct AmBufferId {
    uint32_t id = UINT32_MAX;

    bool isValid() const { return id != UINT32_MAX; }
    bool operator==(const AmBufferId &) const = default;
};

enum class AmBufferUsage {
    STORAGE,
    INDEX
};
enum class AmBufferMemory {
    DEVICE_LOCAL,
    HOST_VISIBLE
};
enum class AmTextureFormat {
    R8,
    RGBA8
};

struct AmBufferDesc {
    size_t initialCapacity = 0;
    AmBufferUsage usage = AmBufferUsage::STORAGE;
    AmBufferMemory memory = AmBufferMemory::HOST_VISIBLE;
    uint32_t shaderBinding = UINT32_MAX; // descriptor slot, UINT32_MAX = not shader-visible
};

struct AmTextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    AmTextureFormat format = AmTextureFormat::R8;
};

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
    vec2 scale = {0.0f, 0.0f};
    vec2 offset = {0.0f, 0.0f};

    UnifiedDimension2() = default;
    UnifiedDimension2(vec2 _scale, vec2 _offset) : scale(_scale), offset(_offset) {};
    UnifiedDimension2(float scaleX, float offsetX, float scaleY, float offsetY)
    {
        scale = vec2(scaleX, scaleY);
        offset = vec2(offsetX, offsetY);
    }

    static UnifiedDimension2 fromScale(float scaleX, float scaleY) { return {{scaleX, scaleY}, {0.0f, 0.0f}}; }
    static UnifiedDimension2 fromScale(float _scale) { return {{_scale, _scale}, {0.0f, 0.0f}}; }
    static UnifiedDimension2 fromOffset(float offsetX, float offsetY) { return {{0.0f, 0.0f}, {offsetX, offsetY}}; }
    static UnifiedDimension2 fromOffset(float _offset) { return {{0.0f, 0.0f}, {_offset, _offset}}; }

    vec2 resolve(const vec2 &parentSize) const { return scale * parentSize + offset; }

    bool operator==(const UnifiedDimension2 &) const = default;
};
using UDim2 = UnifiedDimension2;

struct UnifiedDimension4 {
    UDim top;
    UDim right;
    UDim bottom;
    UDim left;

    vec4 resolve(vec2 parentSize) const
    {
        return vec4(top.resolve(parentSize.y), right.resolve(parentSize.x), bottom.resolve(parentSize.y),
                    left.resolve(parentSize.x));
    }

    bool operator==(const UnifiedDimension4 &) const = default;
};
using UDim4 = UnifiedDimension4;

enum class EventResult {
    CONSUMED,
    PROPAGATE,
};

enum class AutomaticSize {
    NONE,
    OFF, // default
    X,   // Fit childs contents along x-axis
    Y,   // Fit childs contents along x-axis
    XY,  // Fit childs contents along x and y-axis
};

enum class BorderMode {
    NONE,
    OUTLINE,
    MIDDLE,
    INSET,
};

enum class GuiState {
    NONE,
    IDLE,
    HOVER,
    PRESS,
    NON_INTERCTABLE,
};

enum class StartCorner {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

enum class ZIndexBehavior {
    NONE,
    GLOBAL,
    SIBLING,
};

enum class TextDirection {
    AUTO,
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT
};

enum class TextXAlignment {
    NONE,
    LEFT,
    CENTER,
    RIGHT,
};

enum class TextYAlignment {
    NONE,
    TOP,
    CENTER,
    BOTTOM,
};

enum class TextTruncate {
    NONE,
    OFF,
    AT_END,
    SPLIT_WORD,
};

enum PrimitiveType : uint8_t {
    PRIMITIVE_RECT,
    PRIMITIVE_CIRCLE,
    PRIMITIVE_TRIANGLE,
    PRIMITIVE_LINE,
    PRIMITIVE_TEXT,
    PRIMITIVE_CANVAS_LINE,
    PRIMITIVE_CANVAS_TRI,
    PRIMITIVE_CANVAS_QUAD,
    PRIMITIVE_CANVAS_CIRCLE,
    PRIMITIVE_CANVAS_ELLIPSE,
    PRIMITIVE_SVG,
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

enum class ImageScaleType {
    NONE,
    STRETCH,
    TILE,
    FIT,
    CROP,
};

enum class ValueScale {
    LINEAR,
    LOGARITHMIC,
};

enum class ColorModel {
    HSV,
    HSL,
};

enum class ColorPickerShape {
    SQUARE,
    TRIANGLE,
    WHEEL,
};

enum class ScrollBarVisibility {
    NONE,
    ALWAYS,
    AUTO,
    NEVER,
};

enum class ScrollAxis {
    NONE,
    X,
    Y,
    XY,
};

enum class DropdownDirection {
    NONE,
    DOWN,
    UP,
    LEFT,
    RIGHT,
};

enum class PopupPlacement {
    BELOW,
    ABOVE,
    LEFT,
    RIGHT,
};

enum class TabBarMode {
    NONE,
    INSIDE,
    OUTSIDE,
};

enum class TabBarVisibility {
    NONE,
    AUTO,
    NEVER,
    ALWAYS,
};

enum class TabCloseButtonVisibility {
    NONE,
    HIDDEN,
    ALWAYS,
    ACTIVE_ONLY,
    HOVERED_OR_ACTIVE,
};

enum class TabBarPosition {
    NONE,
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
};

enum class DragMode {
    NONE,
    FREE,
    HORIZONTAL,
    VERTICAL,
    SOFT_HORIZONTAL,
    SOFT_VERTICAL,
};

enum class TableSeparatorMode {
    NONE,
    OFF,
    ROWS,
    COLUMNS,
    BOTH,
};

/**
 * @brief Per-character render data for text (GPU-aligned, 40 bytes)
 */
struct CharacterInstance {
    alignas(8) vec2 position;
    alignas(8) vec2 size;
    uint32_t glyphIndex;
    uint32_t color;
    uint32_t strokeColor;
    float strokeThickness;
};
static_assert(sizeof(CharacterInstance) == 32, "CharacterInstance must be 32 bytes");

} // namespace Amethyst

#endif // AMETHYST__COMMON_H
