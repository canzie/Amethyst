#ifndef AMETHYST__MATH_H
#define AMETHYST__MATH_H

#include "utils/am_assert.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace Amethyst {

struct vec3;
struct vec4;

struct vec2 {
    union {
        float x;
        float r;
    };
    union {
        float y;
        float g;
    };

    vec2() = default;
    constexpr vec2(float x, float y) : x(x), y(y) {}
    explicit constexpr vec2(float s) : x(s), y(s) {}

    template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, vec2> &&
             requires(const T &t) {
                 float(t.x);
                 float(t.y);
             })
    vec2(const T &v) : x(float(v.x)), y(float(v.y))
    {
    }

    template <typename T>
    requires(!std::is_same_v<T, vec2> && requires { T{0.0f, 0.0f}; })
    operator T() const
    {
        return {x, y};
    }

    vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
    vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
    vec2 operator*(vec2 o) const { return {x * o.x, y * o.y}; }
    vec2 operator/(vec2 o) const { return {x / o.x, y / o.y}; }
    vec2 operator*(float s) const { return {x * s, y * s}; }
    vec2 operator/(float s) const { return {x / s, y / s}; }
    vec2 operator-() const { return {-x, -y}; }
    vec2 &operator+=(vec2 o)
    {
        x += o.x;
        y += o.y;
        return *this;
    }
    vec2 &operator-=(vec2 o)
    {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    vec2 &operator*=(float s)
    {
        x *= s;
        y *= s;
        return *this;
    }
    vec2 &operator/=(float s)
    {
        x /= s;
        y /= s;
        return *this;
    }
    bool operator==(vec2 o) const { return x == o.x && y == o.y; }
    bool operator!=(vec2 o) const { return !(*this == o); }

    float &operator[](int i) { return i == 0 ? x : y; }
    float operator[](int i) const { return i == 0 ? x : y; }
};
inline vec2 operator*(float s, vec2 v)
{
    return v * s;
}

struct vec3 {
    union {
        float x;
        float r;
    };
    union {
        float y;
        float g;
    };
    union {
        float z;
        float b;
    };

    vec3() = default;
    constexpr vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit constexpr vec3(float s) : x(s), y(s), z(s) {}
    explicit vec3(vec4 v);

    template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, vec3> &&
             requires(const T &t) {
                 float(t.x);
                 float(t.y);
                 float(t.z);
             })
    vec3(const T &v) : x(float(v.x)), y(float(v.y)), z(float(v.z))
    {
    }

    template <typename T>
    requires(!std::is_same_v<T, vec3> && requires { T{0.0f, 0.0f, 0.0f}; })
    operator T() const
    {
        return {x, y, z};
    }

    vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    vec3 operator/(vec3 o) const { return {x / o.x, y / o.y, z / o.z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3 &operator+=(vec3 o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    vec3 &operator-=(vec3 o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    vec3 &operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    vec3 &operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
    bool operator==(vec3 o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(vec3 o) const { return !(*this == o); }

    float &operator[](int i)
    {
        if (i == 0) {
            return x;
        }
        if (i == 1) {
            return y;
        }
        return z;
    }
    float operator[](int i) const
    {
        if (i == 0) {
            return x;
        }
        if (i == 1) {
            return y;
        }
        return z;
    }
};
inline vec3 operator*(float s, vec3 v)
{
    return v * s;
}

struct vec4 {
    union {
        float x;
        float r;
    };
    union {
        float y;
        float g;
    };
    union {
        float z;
        float b;
    };
    union {
        float w;
        float a;
    };

    vec4() = default;
    constexpr vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    explicit constexpr vec4(float s) : x(s), y(s), z(s), w(s) {}
    constexpr vec4(vec3 v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, vec4> &&
             requires(const T &t) {
                 float(t.x);
                 float(t.y);
                 float(t.z);
                 float(t.w);
             })
    vec4(const T &v) : x(float(v.x)), y(float(v.y)), z(float(v.z)), w(float(v.w))
    {
    }

    template <typename T>
    requires(!std::is_same_v<T, vec4> && requires { T{0.0f, 0.0f, 0.0f, 0.0f}; })
    operator T() const
    {
        return {x, y, z, w};
    }

    vec4 operator+(vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    vec4 operator-(vec4 o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    vec4 operator*(vec4 o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    vec4 operator/(vec4 o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
    vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    vec4 operator-() const { return {-x, -y, -z, -w}; }
    vec4 &operator+=(vec4 o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        w += o.w;
        return *this;
    }
    vec4 &operator-=(vec4 o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        w -= o.w;
        return *this;
    }
    vec4 &operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }
    vec4 &operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }
    bool operator==(vec4 o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(vec4 o) const { return !(*this == o); }

    float &operator[](int i)
    {
        if (i == 0) {
            return x;
        }
        if (i == 1) {
            return y;
        }
        if (i == 2) {
            return z;
        }
        return w;
    }
    float operator[](int i) const
    {
        if (i == 0) {
            return x;
        }
        if (i == 1) {
            return y;
        }
        if (i == 2) {
            return z;
        }
        return w;
    }
};
inline vec4 operator*(float s, vec4 v)
{
    return v * s;
}

struct uvec4 {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    uint32_t w = 0;

    uvec4() = default;
    constexpr uvec4(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : x(x), y(y), z(z), w(w) {}

    template <typename T>
    requires std::is_arithmetic_v<T>
    constexpr uvec4(T s) : x(uint32_t(s)), y(uint32_t(s)), z(uint32_t(s)), w(uint32_t(s))
    {
    }

    bool operator==(const uvec4 &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const uvec4 &o) const { return !(*this == o); }

    uint32_t &operator[](size_t i)
    {
        AM_ASSERT(i < 4, "uvec4 index out of range");
        return (&x)[i];
    }
    uint32_t operator[](size_t i) const
    {
        AM_ASSERT(i < 4, "uvec4 index out of range");
        return (&x)[i];
    }
};

inline vec3::vec3(vec4 v) : x(v.x), y(v.y), z(v.z) {}

struct mat3 {
    vec3 cols[3];

    mat3() = default;
    explicit mat3(float d) : cols{vec3(d, 0, 0), vec3(0, d, 0), vec3(0, 0, d)} {}
    mat3(vec3 c0, vec3 c1, vec3 c2) : cols{c0, c1, c2} {}

    template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, mat3> && requires(const T &t) { t[0][0]; })
    mat3(const T &m)
    {
        for (int c = 0; c < 3; c++) {
            for (int r = 0; r < 3; r++) {
                cols[c][r] = float(m[c][r]);
            }
        }
    }

    template <typename T>
    requires(!std::is_same_v<T, mat3> && requires(T t) { t[0][0] = 0.0f; })
    operator T() const
    {
        T m;
        for (int c = 0; c < 3; c++) {
            for (int r = 0; r < 3; r++) {
                m[c][r] = cols[c][r];
            }
        }
        return m;
    }

    vec3 &operator[](int c) { return cols[c]; }
    const vec3 &operator[](int c) const { return cols[c]; }

    bool operator==(const mat3 &o) const { return cols[0] == o.cols[0] && cols[1] == o.cols[1] && cols[2] == o.cols[2]; }
    bool operator!=(const mat3 &o) const { return !(*this == o); }
    mat3 operator*(const mat3 &b) const;
    vec3 operator*(vec3 v) const;
};

struct mat4 {
    vec4 cols[4];

    mat4() = default;
    explicit mat4(float d) : cols{vec4(d, 0, 0, 0), vec4(0, d, 0, 0), vec4(0, 0, d, 0), vec4(0, 0, 0, d)} {}
    mat4(vec4 c0, vec4 c1, vec4 c2, vec4 c3) : cols{c0, c1, c2, c3} {}

    template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, mat4> && requires(const T &t) { t[0][0]; })
    mat4(const T &m)
    {
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                cols[c][r] = float(m[c][r]);
            }
        }
    }

    template <typename T>
    requires(!std::is_same_v<T, mat4> && requires(T t) { t[0][0] = 0.0f; })
    operator T() const
    {
        T m;
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                m[c][r] = cols[c][r];
            }
        }
        return m;
    }

    vec4 &operator[](int c) { return cols[c]; }
    const vec4 &operator[](int c) const { return cols[c]; }

    bool operator==(const mat4 &o) const
    {
        return cols[0] == o.cols[0] && cols[1] == o.cols[1] && cols[2] == o.cols[2] && cols[3] == o.cols[3];
    }
    bool operator!=(const mat4 &o) const { return !(*this == o); }
    mat4 operator*(const mat4 &b) const;
    vec4 operator*(vec4 v) const;
};

inline float dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}
inline float dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline float dot(vec4 a, vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline vec3 cross(vec3 a, vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(vec2 v)
{
    return std::sqrt(dot(v, v));
}
inline float length(vec3 v)
{
    return std::sqrt(dot(v, v));
}

inline vec2 normalize(vec2 v)
{
    return v / length(v);
}
inline vec3 normalize(vec3 v)
{
    return v / length(v);
}

inline float abs(float v)
{
    return std::abs(v);
}
inline vec2 abs(vec2 v)
{
    return {std::abs(v.x), std::abs(v.y)};
}
inline vec3 abs(vec3 v)
{
    return {std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

inline float min(float a, float b)
{
    return std::min(a, b);
}
inline float max(float a, float b)
{
    return std::max(a, b);
}
inline vec2 min(vec2 a, vec2 b)
{
    return {std::min(a.x, b.x), std::min(a.y, b.y)};
}
inline vec2 max(vec2 a, vec2 b)
{
    return {std::max(a.x, b.x), std::max(a.y, b.y)};
}
inline vec3 min(vec3 a, vec3 b)
{
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
inline vec3 max(vec3 a, vec3 b)
{
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}
inline vec4 min(vec4 a, vec4 b)
{
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w)};
}
inline vec4 max(vec4 a, vec4 b)
{
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w)};
}
inline uvec4 min(uvec4 a, uvec4 b)
{
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w)};
}
inline uvec4 max(uvec4 a, uvec4 b)
{
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w)};
}

inline float clamp(float v, float lo, float hi)
{
    return std::min(std::max(v, lo), hi);
}
inline vec2 clamp(vec2 v, vec2 lo, vec2 hi)
{
    return {clamp(v.x, lo.x, hi.x), clamp(v.y, lo.y, hi.y)};
}
inline vec3 clamp(vec3 v, vec3 lo, vec3 hi)
{
    return {clamp(v.x, lo.x, hi.x), clamp(v.y, lo.y, hi.y), clamp(v.z, lo.z, hi.z)};
}

inline float pow(float x, float y)
{
    return std::pow(x, y);
}
inline float fract(float x)
{
    return x - std::floor(x);
}
inline float radians(float deg)
{
    return deg * (3.14159265358979323846f / 180.0f);
}

template <typename T> constexpr T pi()
{
    return T(3.14159265358979323846);
}

mat4 inverse(const mat4 &m);
uint16_t packHalf1x16(float v);
uint32_t packHalf2x16(vec2 v);

} // namespace Amethyst

#endif // AMETHYST__MATH_H
