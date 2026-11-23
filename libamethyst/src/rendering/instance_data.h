/*
 * GPU instance data for UI rendering with packed format
 */

#ifndef AMETHYST__INSTANCE_DATA_H
#define AMETHYST__INSTANCE_DATA_H

#include "components/common.h"
#include "utils/packing.h"

#include "math/math.h"
#include <cstdint>

namespace Amethyst {

enum GpuInstanceFlags : uint32_t {
    INSTANCE_FLAG_VISIBLE = 0x00000001,
    INSTANCE_FLAG_TEXT_RICH = 0x00000002,
    INSTANCE_FLAG_GRADIENT = 0x00000004,
    INSTANCE_FLAG_REPEAT_X = 0x00000008,
    INSTANCE_FLAG_REPEAT_Y = 0x00000010,
    INSTANCE_FLAG_UNSNAPPED = 0x00000020, // drawn under a scale: edges and border widths are not snapped to pixels
    INSTANCE_FLAG_PIXELATED = 0x00000040, // an enlarged texture is sampled nearest, see ImageSamplingMode::PIXELATED
};

struct InstanceData {
    alignas(8) vec2 translation;            // 8B position
    alignas(8) vec2 scale;                  // 8B size
    alignas(16) vec4 clipRect;              // 16B clip rect (keep as float)
    uint32_t fillColor = 0xFFFFFFFF;        // 4B packed RGBA
    uint32_t borderColor = 0;               // 4B packed RGBA
    uint32_t shapeData[4] = {0, 0, 0, 0};   // 16B packed half2 pairs for triangle/line/text UV
    uint32_t rotationBorderThickness = 0;   // 4B: rotation(16 bits) | borderThickness(16 bits half)
    // a rect's corner radii are four 12 bit values across these two (tl | tr | br, then br | bl), see setCornerRadii();
    // other primitives keep one half float radius or thickness in the low 16 bits of cornerRadii
    uint16_t cornerRadiiHigh = 0;           // 2B: last 4 bits of br | bl
    uint8_t primitiveType = 0;              // 1B: PrimitiveType
    uint8_t borderMode = 0;                 // 1B: BorderMode
    uint32_t cornerRadii = 0;               // 4B: tl | tr | first 8 bits of br, or a half float radius / thickness
    uint32_t textureId = UINT32_MAX;        // 4B texture handle
    int32_t zIndex = 0;                     // 4B z-index for sorting
    uint32_t flags = INSTANCE_FLAG_VISIBLE; // 4B: visible(1 bit) | padding(31 bits)

    void setFillColor(const Color4 &c);
    void setBorderColor(const Color4 &c) { borderColor = packColor(c); }

    void setRotation(float radians)
    {
        uint16_t rot = packRotation(radians);
        rotationBorderThickness = (rotationBorderThickness & 0xFFFF0000u) | rot;
    }

    void setBorderThickness(float thickness)
    {
        uint16_t packed = packFloatToHalf(thickness);
        rotationBorderThickness = (rotationBorderThickness & 0x0000FFFFu) | (static_cast<uint32_t>(packed) << 16);
    }

    void setThickness(float thickness)
    {
        uint16_t packed = packFloatToHalf(thickness);
        cornerRadii = (cornerRadii & 0xFFFF0000u) | packed;
    }

    /**
     * @brief Rounds a rect's corners. When the two radii on one side add up to more than that side, all four
     * shrink by the same factor until they fit.
     *
     * Each radius is kept as a 12 bit half float without its sign and lowest 3 mantissa bits, so it stays
     * within 0.8% of what was asked at any size, up to 65504.
     * @param radii Radii in logical pixels as (top left, top right, bottom right, bottom left)
     * @param boxSize The size of the rect being rounded
     */
    void setCornerRadii(vec4 radii, vec2 boxSize);

    /**
     * @brief The corner radii setCornerRadii() stored, as they come back out of their packing.
     * @return Radii as (top left, top right, bottom right, bottom left)
     */
    vec4 getCornerRadii() const;

    void setPrimitiveType(PrimitiveType type) { primitiveType = static_cast<uint8_t>(type); }

    void setBorderMode(BorderMode mode) { borderMode = static_cast<uint8_t>(mode); }

    /**
     * @brief Maps this instance from a node's layout space into the window, at window = layout * scale + offset.
     * Lengths it carries grow with it: size, border, corner radii, shape points, line widths and repeated marks.
     * An instance moved this way is drawn as is, without being snapped to pixels.
     * @param scale How many window pixels one layout pixel covers
     * @param offset Where the layout origin lands in the window
     * @param scaleThickness Whether border and line thickness grow with the scale; false keeps them as thick in the
     * window as they were asked to be
     */
    void transform(float scale, vec2 offset, bool scaleThickness);

    void setUvRect(const vec4 &uvRect)
    {
        shapeData[0] = packHalf2x16(vec2(uvRect.x, uvRect.y));
        shapeData[1] = packHalf2x16(vec2(uvRect.z, uvRect.w));
    }

    void setShapePoint(uint32_t index, vec2 point) { shapeData[index] = packHalf2x16(point); }

    void setVisible(bool visible)
    {
        if (visible) {
            flags |= INSTANCE_FLAG_VISIBLE;
        } else {
            flags &= ~INSTANCE_FLAG_VISIBLE;
        }
    }

    void setTextRich(bool rich)
    {
        if (rich) {
            flags |= INSTANCE_FLAG_TEXT_RICH;
        } else {
            flags &= ~INSTANCE_FLAG_TEXT_RICH;
        }
    }

    void setGlyphSlice(uint32_t sliceHandle) { shapeData[0] = sliceHandle; }

    void setGradientSlot(uint32_t slot)
    {
        shapeData[1] = slot;
        flags |= INSTANCE_FLAG_GRADIENT;
    }

    /**
     * @brief Draw the shape as a mark repeated across the instance's area instead of once over all of it.
     * @param repeat The axes the mark repeats along; NONE draws the shape once over the whole area
     * @param size The size of one mark, in logical pixels
     * @param step The distance from the start of one mark to the start of the next, in logical pixels
     * @param position Where the first mark starts, relative to the instance's top left, in logical pixels
     */
    void setBackgroundRepeat(BackgroundRepeat repeat, vec2 size, vec2 step, vec2 position);

    void clearBackgroundRepeat() { flags &= ~(INSTANCE_FLAG_REPEAT_X | INSTANCE_FLAG_REPEAT_Y); }
};

} // namespace Amethyst

#endif // AMETHYST__INSTANCE_DATA_H
