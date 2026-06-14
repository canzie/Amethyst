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

#include "math/math.h"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>

namespace Amethyst {

constexpr uint32_t MAX_GRADIENT_STOPS = 8;

enum class GradientType {
    LINEAR,
    RADIAL,
};

inline float srgbToLinear(float srgb)
{
    return srgb <= 0.04045f ? srgb / 12.92f : pow((srgb + 0.055f) / 1.055f, 2.4f);
}

inline float linearToSrgb(float linear)
{
    return linear <= 0.0031308f ? linear * 12.92f : 1.055f * pow(linear, 1.0f / 2.4f) - 0.055f;
}

struct Color4;
struct Gradient;

bool gradientEqual(const Gradient &a, const Gradient &b);

struct Color3 {
    float r, g, b;

    Color3() : r(0.0f), g(0.0f), b(0.0f) {}

    Color3(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}

    explicit Color3(float _v) : r(_v), g(_v), b(_v) {}

    Color3(const vec3 &v) : r(v.r), g(v.g), b(v.b) {}

    inline Color3(const Color4 &c);

    static Color3 fromRgb(uint8_t _r, uint8_t _g, uint8_t _b) { return Color3(_r / 255.0f, _g / 255.0f, _b / 255.0f); }

    static Color3 fromHex(uint32_t hex)
    {
        return Color3(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f);
    }

    static Color3 fromGradient(std::shared_ptr<const Gradient> grad, const Color3 &tint = Color3(1.0f, 1.0f, 1.0f))
    {
        Color3 c = tint;
        c.m_gradient = std::move(grad);
        return c;
    }

    const std::shared_ptr<const Gradient> &getGradient() const { return m_gradient; }
    bool hasGradient() const { return m_gradient != nullptr; }

    operator vec3() const { return vec3(r, g, b); }

    bool operator==(const Color3 &other) const
    {
        if (r != other.r || g != other.g || b != other.b) {
            return false;
        }
        if (m_gradient == other.m_gradient) {
            return true;
        }
        if (m_gradient == nullptr || other.m_gradient == nullptr) {
            return false;
        }
        return gradientEqual(*m_gradient, *other.m_gradient);
    }
    bool operator!=(const Color3 &other) const { return !(*this == other); }

    Color3 operator+(const Color3 &other) const { return fromLinear(r + other.r, g + other.g, b + other.b); }
    Color3 operator-(const Color3 &other) const { return fromLinear(r - other.r, g - other.g, b - other.b); }
    Color3 operator*(float scalar) const { return fromLinear(r * scalar, g * scalar, b * scalar); }
    Color3 operator/(float scalar) const { return fromLinear(r / scalar, g / scalar, b / scalar); }

    Color3 &operator+=(const Color3 &other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        return *this;
    }
    Color3 &operator*=(float scalar)
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        return *this;
    }

    static Color3 fromLinear(float _r, float _g, float _b)
    {
        Color3 c;
        c.r = _r;
        c.g = _g;
        c.b = _b;
        return c;
    }

  private:
    std::shared_ptr<const Gradient> m_gradient;
};

struct Color4 {
    float r, g, b, a;

    Color4() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

    Color4(float _r, float _g, float _b, float _a = 1.0f) : r(_r), g(_g), b(_b), a(_a) {}

    explicit Color4(float _v) : r(_v), g(_v), b(_v), a(1.0f) {}

    Color4(const Color3 &c, float _a) : r(c.r), g(c.g), b(c.b), a(_a), m_gradient(c.getGradient()) {}

    Color4(const vec4 &v) : r(v.r), g(v.g), b(v.b), a(v.a) {}

    static Color4 fromRgb(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
    {
        return Color4(_r / 255.0f, _g / 255.0f, _b / 255.0f, _a / 255.0f);
    }

    static Color4 fromHex(uint32_t hex, bool hasAlpha = false)
    {
        if (hasAlpha) {
            return Color4(((hex >> 24) & 0xFF) / 255.0f, ((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f,
                          (hex & 0xFF) / 255.0f);
        }
        return Color4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, 1.0f);
    }

    static Color4 fromGradient(std::shared_ptr<const Gradient> grad, const Color4 &tint = Color4(1.0f, 1.0f, 1.0f, 1.0f))
    {
        Color4 c = tint;
        c.m_gradient = std::move(grad);
        return c;
    }

    const std::shared_ptr<const Gradient> &getGradient() const { return m_gradient; }
    bool hasGradient() const { return m_gradient != nullptr; }

    operator vec4() const { return vec4(r, g, b, a); }
    operator vec3() const { return vec3(r, g, b); }

    bool operator==(const Color4 &other) const
    {
        if (r != other.r || g != other.g || b != other.b || a != other.a) {
            return false;
        }
        if (m_gradient == other.m_gradient) {
            return true;
        }
        if (m_gradient == nullptr || other.m_gradient == nullptr) {
            return false;
        }
        return gradientEqual(*m_gradient, *other.m_gradient);
    }
    bool operator!=(const Color4 &other) const { return !(*this == other); }

    Color4 operator+(const Color4 &other) const { return fromLinear(r + other.r, g + other.g, b + other.b, a + other.a); }
    Color4 operator-(const Color4 &other) const { return fromLinear(r - other.r, g - other.g, b - other.b, a - other.a); }
    Color4 operator*(float scalar) const { return fromLinear(r * scalar, g * scalar, b * scalar, a * scalar); }
    Color4 operator/(float scalar) const { return fromLinear(r / scalar, g / scalar, b / scalar, a / scalar); }

    Color4 &operator+=(const Color4 &other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
        return *this;
    }
    Color4 &operator*=(float scalar)
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        a *= scalar;
        return *this;
    }

    static Color4 fromLinear(float _r, float _g, float _b, float _a)
    {
        Color4 c;
        c.r = _r;
        c.g = _g;
        c.b = _b;
        c.a = _a;
        return c;
    }

  private:
    std::shared_ptr<const Gradient> m_gradient;
};

inline Color3 operator*(float scalar, const Color3 &color)
{
    return color * scalar;
}
inline Color4 operator*(float scalar, const Color4 &color)
{
    return color * scalar;
}

inline Color3::Color3(const Color4 &c) : r(c.r), g(c.g), b(c.b), m_gradient(c.getGradient()) {}

/**
 * @brief Convert an HSV triple to an RGB color.
 * @param h Hue in [0, 1), wrapped if out of range
 * @param s Saturation in [0, 1]
 * @param v Value in [0, 1]
 * @return The equivalent RGB color
 */
inline Color3 hsvToRgb(float h, float s, float v)
{
    h -= static_cast<float>(static_cast<int>(h));
    if (h < 0.0f) {
        h += 1.0f;
    }
    float sixth = h * 6.0f;
    int sector = static_cast<int>(sixth);
    float f = sixth - static_cast<float>(sector);
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (sector) {
    case 0:
        return Color3(v, t, p);
    case 1:
        return Color3(q, v, p);
    case 2:
        return Color3(p, v, t);
    case 3:
        return Color3(p, q, v);
    case 4:
        return Color3(t, p, v);
    default:
        return Color3(v, p, q);
    }
}

/**
 * @brief Convert an RGB color to its HSV triple.
 * @param c The RGB color to convert
 * @return A vec3 holding hue in [0, 1), saturation in [0, 1] and value in [0, 1]
 */
inline vec3 rgbToHsv(const Color3 &c)
{
    float r = c.r, g = c.g, b = c.b;
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float v = mx;
    float d = mx - mn;
    float s = mx <= 0.0f ? 0.0f : d / mx;
    float h = 0.0f;
    if (d > 0.0f) {
        if (mx == r) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (mx == g) {
            h = (b - r) / d + 2.0f;
        } else {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f;
    }
    return vec3(h, s, v);
}

/**
 * @brief A pre-packed RGBA8 color and its position along the gradient axis, as stored inside a Gradient.
 */
struct GradientStop {
    uint32_t color = 0;
    float t = 0.0f;
    bool operator==(const GradientStop &) const = default;
};

/**
 * @brief Authoring form of a stop. Holds a real color and a position and is accepted by Gradient::linear.
 */
struct GradientColorStop {
    float t;
    Color4 color;

    GradientColorStop(float _t, const Color4 &_color) : t(_t), color(_color) {}
    GradientColorStop(float _t, const Color3 &_color) : t(_t), color(_color, 1.0f) {}
};

/**
 * @brief Immutable gradient definition shared by every color that references it.
 * Build it through the factories and never mutate it after construction.
 */
struct Gradient {
    static constexpr uint32_t INVALID_SLOT = UINT32_MAX;

    GradientType type = GradientType::LINEAR;
    float angleDegrees = 0.0f;
    std::array<GradientStop, MAX_GRADIENT_STOPS> stops{};
    uint32_t stopCount = 0;
    mutable uint32_t gpuSlot = INVALID_SLOT;

    bool operator==(const Gradient &other) const
    {
        return type == other.type && angleDegrees == other.angleDegrees && stopCount == other.stopCount && stops == other.stops;
    }

    static std::shared_ptr<const Gradient> linear(float angle, std::initializer_list<GradientColorStop> stops);
};

} // namespace Amethyst

#endif // AMETHYST__COLOR_H
