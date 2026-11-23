#include "rendering/instance_data.h"

#include "rendering/gpu_resource_hub.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Amethyst {

// the largest finite half float, 0x7BFF, with its low 3 bits dropped
static constexpr uint32_t MAX_RADIUS_BITS = 0xF7Fu;

static uint32_t s_packRadius(float radius)
{
    if (!(radius > 0.0f)) {
        return 0;
    }
    uint32_t half = packHalf1x16(radius);
    // rounds to the nearest 12 bit value; a carry into the exponent is still the next value up
    return std::min((half + 4u) >> 3, MAX_RADIUS_BITS);
}

static float s_unpackRadius(uint32_t bits)
{
    return unpackHalf1x16(static_cast<uint16_t>(bits << 3));
}

void InstanceData::setCornerRadii(vec4 radii, vec2 boxSize)
{
    radii = max(radii, vec4(0.0f));

    // a side whose two radii add up to more than its length shrinks all four by the same factor
    float factor = 1.0f;
    const float sides[4] = {radii.x + radii.y, radii.y + radii.z, radii.z + radii.w, radii.w + radii.x};
    const float lengths[4] = {boxSize.x, boxSize.y, boxSize.x, boxSize.y};
    for (int i = 0; i < 4; i++) {
        if (sides[i] > 0.0f) {
            factor = std::min(factor, std::max(lengths[i], 0.0f) / sides[i]);
        }
    }
    radii *= factor;

    const uint32_t tl = s_packRadius(radii.x);
    const uint32_t tr = s_packRadius(radii.y);
    const uint32_t br = s_packRadius(radii.z);
    const uint32_t bl = s_packRadius(radii.w);
    cornerRadii = tl | (tr << 12) | ((br & 0xFFu) << 24);
    cornerRadiiHigh = static_cast<uint16_t>((br >> 8) | (bl << 4));
}

vec4 InstanceData::getCornerRadii() const
{
    const uint32_t tl = cornerRadii & 0xFFFu;
    const uint32_t tr = (cornerRadii >> 12) & 0xFFFu;
    const uint32_t br = (cornerRadii >> 24) | ((static_cast<uint32_t>(cornerRadiiHigh) & 0xFu) << 8);
    const uint32_t bl = static_cast<uint32_t>(cornerRadiiHigh) >> 4;
    return {s_unpackRadius(tl), s_unpackRadius(tr), s_unpackRadius(br), s_unpackRadius(bl)};
}

static uint32_t s_scaleHalf2(uint32_t packed, float scale)
{
    return packHalf2x16(unpackHalf2x16(packed) * scale);
}

void InstanceData::transform(float factor, vec2 offset, bool scaleThickness)
{
    if (factor == 1.0f && offset == vec2(0.0f)) {
        return;
    }

    translation = translation * factor + offset;
    scale *= factor;
    flags |= INSTANCE_FLAG_UNSNAPPED;

    const float thicknessFactor = scaleThickness ? factor : 1.0f;
    const uint16_t border = static_cast<uint16_t>(rotationBorderThickness >> 16);
    if (border != 0) {
        setBorderThickness(unpackHalf1x16(border) * thicknessFactor);
    }

    const bool repeats = (flags & (INSTANCE_FLAG_REPEAT_X | INSTANCE_FLAG_REPEAT_Y)) != 0u;
    const auto scaleLowHalf = [this](float by) {
        setThickness(unpackHalf1x16(static_cast<uint16_t>(cornerRadii & 0xFFFFu)) * by);
    };

    switch (static_cast<PrimitiveType>(primitiveType)) {
    case PRIMITIVE_RECT:
        // the radii were already fitted to the box, which grows by the same factor, so they still fit
        setCornerRadii(getCornerRadii() * factor, vec2(std::numeric_limits<float>::max()));
        break;
    case PRIMITIVE_LINE:
        scaleLowHalf(thicknessFactor);
        break;
    case PRIMITIVE_CANVAS_CIRCLE:
        scaleLowHalf(factor);
        break;
    case PRIMITIVE_CANVAS_LINE:
        shapeData[0] = s_scaleHalf2(shapeData[0], factor);
        shapeData[1] = s_scaleHalf2(shapeData[1], factor);
        scaleLowHalf(thicknessFactor);
        break;
    case PRIMITIVE_CANVAS_BEZIER:
        for (int i = 0; i < 3; i++) {
            shapeData[i] = s_scaleHalf2(shapeData[i], factor);
        }
        scaleLowHalf(thicknessFactor);
        break;
    case PRIMITIVE_CANVAS_TRI:
        for (int i = 0; i < 3; i++) {
            shapeData[i] = s_scaleHalf2(shapeData[i], factor);
        }
        break;
    case PRIMITIVE_CANVAS_QUAD:
        for (int i = 0; i < 4; i++) {
            shapeData[i] = s_scaleHalf2(shapeData[i], factor);
        }
        break;
    case PRIMITIVE_CANVAS_ELLIPSE:
        shapeData[0] = s_scaleHalf2(shapeData[0], factor);
        break;
    case PRIMITIVE_CIRCLE:
    case PRIMITIVE_TRIANGLE:
    case PRIMITIVE_TEXT:
    case PRIMITIVE_SVG:
    case PRIMITIVE_COUNT:
        break;
    }

    // a repeated mark's size, step and start are lengths too; shapeData[1] is the gradient slot
    if (repeats) {
        shapeData[0] = s_scaleHalf2(shapeData[0], factor);
        shapeData[2] = s_scaleHalf2(shapeData[2], factor);
        shapeData[3] = s_scaleHalf2(shapeData[3], factor);
    }
}

void InstanceData::setFillColor(const Color4 &c)
{
    fillColor = packColor(c);

    if (!c.hasGradient()) {
        return;
    }

    GpuResourceHub *hub = GpuResourceHub::active();
    if (hub == nullptr) {
        return;
    }

    uint32_t slot = hub->gradients().resolveShared(c.getGradient());
    if (slot != Gradient::INVALID_SLOT) {
        setGradientSlot(slot);
    }
}

void InstanceData::setBackgroundRepeat(BackgroundRepeat repeat, vec2 size, vec2 step, vec2 position)
{
    bool repeatX = repeat == BackgroundRepeat::REPEAT || repeat == BackgroundRepeat::REPEAT_X;
    bool repeatY = repeat == BackgroundRepeat::REPEAT || repeat == BackgroundRepeat::REPEAT_Y;
    if (!repeatX && !repeatY) {
        return;
    }
    if (size.x <= 0.0f || size.y <= 0.0f || (repeatX && step.x <= 0.0f) || (repeatY && step.y <= 0.0f)) {
        return;
    }

    // moving a repeating mark by whole steps changes nothing, and a wrapped position stays small
    // enough to keep its precision as a half float
    if (repeatX) {
        position.x -= step.x * std::floor(position.x / step.x);
    }
    if (repeatY) {
        position.y -= step.y * std::floor(position.y / step.y);
    }

    shapeData[0] = packHalf2x16(size);
    shapeData[2] = packHalf2x16(step);
    shapeData[3] = packHalf2x16(position);
    if (repeatX) {
        flags |= INSTANCE_FLAG_REPEAT_X;
    }
    if (repeatY) {
        flags |= INSTANCE_FLAG_REPEAT_Y;
    }
}

} // namespace Amethyst
