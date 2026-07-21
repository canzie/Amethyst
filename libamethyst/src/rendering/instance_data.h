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
};

struct InstanceData {
    alignas(8) vec2 translation;            // 8B position
    alignas(8) vec2 scale;                  // 8B size
    alignas(16) vec4 clipRect;              // 16B clip rect (keep as float)
    uint32_t fillColor = 0xFFFFFFFF;        // 4B packed RGBA
    uint32_t borderColor = 0;               // 4B packed RGBA
    uint32_t shapeData[4] = {0, 0, 0, 0};   // 16B packed half2 pairs for triangle/line/text UV
    uint32_t rotationBorderThickness = 0;   // 4B: rotation(16 bits) | borderThickness(16 bits half)
    uint32_t primitiveMode = 0;             // 4B: unused(16 bits) | primitiveType(8) | borderMode(8)
    uint32_t cornerRadii = 0;               // 4B: per-corner radii tl|tr|br|bl (uint8), for lines its thickness(16 half)
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

    void setCornerRadii(const uvec4 &radii) { cornerRadii = packU8x4(min(radii, uvec4(255u))); }

    void setPrimitiveType(PrimitiveType type)
    {
        primitiveMode = (primitiveMode & 0xFF00FFFFu) | (static_cast<uint32_t>(type) << 16);
    }

    void setBorderMode(BorderMode mode) { primitiveMode = (primitiveMode & 0x00FFFFFFu) | (static_cast<uint32_t>(mode) << 24); }

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
};

} // namespace Amethyst

#endif // AMETHYST__INSTANCE_DATA_H
