#include "math/math.h"

#include <cstring>

namespace Amethyst {

mat3 mat3::operator*(const mat3 &b) const
{
    mat3 r;
    for (int c = 0; c < 3; c++) {
        r[c].x = cols[0].x * b[c].x + cols[1].x * b[c].y + cols[2].x * b[c].z;
        r[c].y = cols[0].y * b[c].x + cols[1].y * b[c].y + cols[2].y * b[c].z;
        r[c].z = cols[0].z * b[c].x + cols[1].z * b[c].y + cols[2].z * b[c].z;
    }
    return r;
}

vec3 mat3::operator*(vec3 v) const
{
    return {
        cols[0].x * v.x + cols[1].x * v.y + cols[2].x * v.z,
        cols[0].y * v.x + cols[1].y * v.y + cols[2].y * v.z,
        cols[0].z * v.x + cols[1].z * v.y + cols[2].z * v.z,
    };
}

mat4 mat4::operator*(const mat4 &b) const
{
    mat4 r;
    for (int c = 0; c < 4; c++) {
        r[c].x = cols[0].x * b[c].x + cols[1].x * b[c].y + cols[2].x * b[c].z + cols[3].x * b[c].w;
        r[c].y = cols[0].y * b[c].x + cols[1].y * b[c].y + cols[2].y * b[c].z + cols[3].y * b[c].w;
        r[c].z = cols[0].z * b[c].x + cols[1].z * b[c].y + cols[2].z * b[c].z + cols[3].z * b[c].w;
        r[c].w = cols[0].w * b[c].x + cols[1].w * b[c].y + cols[2].w * b[c].z + cols[3].w * b[c].w;
    }
    return r;
}

vec4 mat4::operator*(vec4 v) const
{
    return {
        cols[0].x * v.x + cols[1].x * v.y + cols[2].x * v.z + cols[3].x * v.w,
        cols[0].y * v.x + cols[1].y * v.y + cols[2].y * v.z + cols[3].y * v.w,
        cols[0].z * v.x + cols[1].z * v.y + cols[2].z * v.z + cols[3].z * v.w,
        cols[0].w * v.x + cols[1].w * v.y + cols[2].w * v.z + cols[3].w * v.w,
    };
}

static float s_minor3(const mat4 &m, int c0, int c1, int c2, int r0, int r1, int r2)
{
    return m[c0][r0] * (m[c1][r1] * m[c2][r2] - m[c2][r1] * m[c1][r2]) -
           m[c1][r0] * (m[c0][r1] * m[c2][r2] - m[c2][r1] * m[c0][r2]) +
           m[c2][r0] * (m[c0][r1] * m[c1][r2] - m[c1][r1] * m[c0][r2]);
}

mat4 inverse(const mat4 &m)
{
    static constexpr int excl[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};

    float cof[4][4];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float sign = ((c + r) % 2 == 0) ? 1.0f : -1.0f;
            cof[c][r] = sign * s_minor3(m, excl[c][0], excl[c][1], excl[c][2], excl[r][0], excl[r][1], excl[r][2]);
        }
    }

    float det = m[0][0] * cof[0][0] + m[1][0] * cof[1][0] + m[2][0] * cof[2][0] + m[3][0] * cof[3][0];
    float invDet = 1.0f / det;

    mat4 inv;
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            inv[c][r] = cof[r][c] * invDet;
        }
    }
    return inv;
}

uint16_t packHalf1x16(float f)
{
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));

    uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
    int32_t exp32 = static_cast<int32_t>((bits >> 23) & 0xFFu);
    uint32_t mant32 = bits & 0x7FFFFFu;

    if (exp32 == 255) {
        return sign | 0x7C00u | (mant32 ? 0x0200u : 0u);
    }

    int32_t exp16 = exp32 - 127 + 15;

    if (exp16 >= 31) {
        return sign | 0x7C00u;
    }

    // round to nearest, ties to even, on the bits shifted out
    auto roundShift = [](uint32_t m, uint32_t shift) {
        uint32_t half = 1u << (shift - 1);
        uint32_t rest = m & ((1u << shift) - 1u);
        uint32_t out = m >> shift;
        if (rest > half || (rest == half && (out & 1u) != 0u)) {
            ++out;
        }
        return out;
    };

    if (exp16 <= 0) {
        // a half subnormal is m * 2^-24 and this float is mant24 * 2^(exp32 - 150), so
        // m = mant24 >> (14 - exp16); below 2^-25 everything rounds to zero
        if (exp16 < -10) {
            return sign;
        }
        mant32 |= 0x800000u;
        uint32_t shift = static_cast<uint32_t>(14 - exp16);
        // rounding up out of the subnormal range lands on 0x0400, the smallest normal, as it should
        return sign | static_cast<uint16_t>(roundShift(mant32, shift));
    }

    uint32_t mant16 = roundShift(mant32, 13);
    if (mant16 >= 0x400u) {
        exp16++;
        if (exp16 >= 31) {
            return sign | 0x7C00u;
        }
        mant16 = 0;
    }

    return sign | static_cast<uint16_t>(exp16 << 10) | static_cast<uint16_t>(mant16);
}

uint32_t packHalf2x16(vec2 v)
{
    return static_cast<uint32_t>(packHalf1x16(v.x)) | (static_cast<uint32_t>(packHalf1x16(v.y)) << 16);
}

float unpackHalf1x16(uint16_t h)
{
    uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
    uint32_t exp16 = (h >> 10) & 0x1Fu;
    uint32_t mant16 = h & 0x3FFu;

    uint32_t bits;
    if (exp16 == 0) {
        // subnormal halves are m * 2^-24, which a float holds exactly
        float magnitude = static_cast<float>(mant16) * (1.0f / 16777216.0f);
        return sign != 0u ? -magnitude : magnitude;
    }
    if (exp16 == 31) {
        bits = sign | 0x7F800000u | (mant16 << 13);
    } else {
        bits = sign | ((exp16 - 15 + 127) << 23) | (mant16 << 13);
    }
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

vec2 unpackHalf2x16(uint32_t v)
{
    return vec2(unpackHalf1x16(static_cast<uint16_t>(v & 0xFFFFu)), unpackHalf1x16(static_cast<uint16_t>(v >> 16)));
}

} // namespace Amethyst
