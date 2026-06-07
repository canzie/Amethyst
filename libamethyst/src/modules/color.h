/**
 * @file color.h
 * @brief Color types for UI rendering
 *
 * All input values are treated as sRGB (what you see on screen).
 * Internal storage is linear for correct blending math.
 * Conversion back to sRGB happens in packColor for GPU submission.
 */

#ifndef AMETHYST__COLOR_H
#define AMETHYST__COLOR_H

#include <cstdint>
#include "math/math.h"

namespace Amethyst {

inline float srgbToLinear(float srgb)
{
    return srgb <= 0.04045f ? srgb / 12.92f : pow((srgb + 0.055f) / 1.055f, 2.4f);
}

inline float linearToSrgb(float linear)
{
    return linear <= 0.0031308f ? linear * 12.92f : 1.055f * pow(linear, 1.0f / 2.4f) - 0.055f;
}

struct Color4;

struct Color3 {
    float r, g, b;

    constexpr Color3() : r(0.0f), g(0.0f), b(0.0f) {}

    Color3(float _r, float _g, float _b) : r(srgbToLinear(_r)), g(srgbToLinear(_g)), b(srgbToLinear(_b)) {}

    explicit Color3(float _v) : r(srgbToLinear(_v)), g(srgbToLinear(_v)), b(srgbToLinear(_v)) {}

    Color3(const vec3 &v) : r(srgbToLinear(v.r)), g(srgbToLinear(v.g)), b(srgbToLinear(v.b)) {}

    inline Color3(const Color4 &c);

    static Color3 fromRgb(uint8_t _r, uint8_t _g, uint8_t _b)
    {
        return Color3(_r / 255.0f, _g / 255.0f, _b / 255.0f);
    }

    static Color3 fromHex(uint32_t hex)
    {
        return Color3(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f
        );
    }

    constexpr operator vec3() const { return vec3(r, g, b); }

    bool operator==(const Color3 &other) const { return r == other.r && g == other.g && b == other.b; }
    bool operator!=(const Color3 &other) const { return !(*this == other); }

    Color3 operator+(const Color3 &other) const { return fromLinear(r + other.r, g + other.g, b + other.b); }
    Color3 operator-(const Color3 &other) const { return fromLinear(r - other.r, g - other.g, b - other.b); }
    Color3 operator*(float scalar) const { return fromLinear(r * scalar, g * scalar, b * scalar); }
    Color3 operator/(float scalar) const { return fromLinear(r / scalar, g / scalar, b / scalar); }

    Color3 &operator+=(const Color3 &other) { r += other.r; g += other.g; b += other.b; return *this; }
    Color3 &operator*=(float scalar) { r *= scalar; g *= scalar; b *= scalar; return *this; }

    static constexpr Color3 fromLinear(float _r, float _g, float _b)
    {
        Color3 c;
        c.r = _r; c.g = _g; c.b = _b;
        return c;
    }
};

struct Color4 {
    float r, g, b, a;

    constexpr Color4() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

    Color4(float _r, float _g, float _b, float _a = 1.0f)
        : r(srgbToLinear(_r)), g(srgbToLinear(_g)), b(srgbToLinear(_b)), a(_a) {}

    explicit Color4(float _v) : r(srgbToLinear(_v)), g(srgbToLinear(_v)), b(srgbToLinear(_v)), a(1.0f) {}

    constexpr Color4(const Color3 &c, float _a) : r(c.r), g(c.g), b(c.b), a(_a) {}

    Color4(const vec4 &v) : r(srgbToLinear(v.r)), g(srgbToLinear(v.g)), b(srgbToLinear(v.b)), a(v.a) {}

    static Color4 fromRgb(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
    {
        return Color4(_r / 255.0f, _g / 255.0f, _b / 255.0f, _a / 255.0f);
    }

    static Color4 fromHex(uint32_t hex, bool hasAlpha = false)
    {
        if (hasAlpha) {
            return Color4(
                ((hex >> 24) & 0xFF) / 255.0f,
                ((hex >> 16) & 0xFF) / 255.0f,
                ((hex >> 8) & 0xFF) / 255.0f,
                (hex & 0xFF) / 255.0f
            );
        }
        return Color4(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            1.0f
        );
    }

    constexpr operator vec4() const { return vec4(r, g, b, a); }
    constexpr operator vec3() const { return vec3(r, g, b); }

    bool operator==(const Color4 &other) const { return r == other.r && g == other.g && b == other.b && a == other.a; }
    bool operator!=(const Color4 &other) const { return !(*this == other); }

    Color4 operator+(const Color4 &other) const { return fromLinear(r + other.r, g + other.g, b + other.b, a + other.a); }
    Color4 operator-(const Color4 &other) const { return fromLinear(r - other.r, g - other.g, b - other.b, a - other.a); }
    Color4 operator*(float scalar) const { return fromLinear(r * scalar, g * scalar, b * scalar, a * scalar); }
    Color4 operator/(float scalar) const { return fromLinear(r / scalar, g / scalar, b / scalar, a / scalar); }

    Color4 &operator+=(const Color4 &other) { r += other.r; g += other.g; b += other.b; a += other.a; return *this; }
    Color4 &operator*=(float scalar) { r *= scalar; g *= scalar; b *= scalar; a *= scalar; return *this; }

    static constexpr Color4 fromLinear(float _r, float _g, float _b, float _a)
    {
        Color4 c;
        c.r = _r; c.g = _g; c.b = _b; c.a = _a;
        return c;
    }
};

inline Color3 operator*(float scalar, const Color3 &color) { return color * scalar; }
inline Color4 operator*(float scalar, const Color4 &color) { return color * scalar; }

inline Color3::Color3(const Color4 &c) : r(c.r), g(c.g), b(c.b) {}

} // namespace Amethyst

#endif // AMETHYST__COLOR_H
