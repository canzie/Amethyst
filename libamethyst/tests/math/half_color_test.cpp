#include "math/math.h"
#include "modules/color.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>

using namespace Amethyst;

namespace {

// exact decode of a binary16 value; every half is representable as a float
float unpackHalf(uint16_t h)
{
    int sign = (h & 0x8000u) != 0 ? -1 : 1;
    int exp = (h >> 10) & 0x1F;
    int mant = h & 0x3FF;
    if (exp == 0) {
        return static_cast<float>(sign) * std::ldexp(static_cast<float>(mant), -24);
    }
    if (exp == 31) {
        return mant == 0 ? static_cast<float>(sign) * INFINITY : NAN;
    }
    return static_cast<float>(sign) * std::ldexp(static_cast<float>(mant | 0x400), exp - 25);
}

// reference rounding: nearest half by value, ties to the even encoding, computed in double
uint16_t referencePack(float f)
{
    if (std::isnan(f)) {
        return 0x7E00;
    }
    uint16_t sign = std::signbit(f) ? 0x8000 : 0;
    double a = std::fabs(static_cast<double>(f));
    // largest finite half is 65504; anything at or past the midpoint to 65536 overflows
    if (a >= 65520.0) {
        return sign | 0x7C00;
    }
    uint16_t best = 0;
    double bestErr = INFINITY;
    // binary search over the monotonic positive encodings
    uint16_t lo = 0, hi = 0x7BFF;
    while (hi - lo > 1) {
        uint16_t mid = static_cast<uint16_t>((lo + hi) / 2);
        if (static_cast<double>(unpackHalf(mid)) <= a) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    for (uint16_t c : {lo, hi}) {
        double err = std::fabs(static_cast<double>(unpackHalf(c)) - a);
        if (err < bestErr || (err == bestErr && (c & 1u) == 0u)) {
            best = c;
            bestErr = err;
        }
    }
    return sign | best;
}

} // namespace

TEST(HalfPack, EveryHalfRoundTrips)
{
    for (uint32_t h = 0; h < 0x10000; ++h) {
        uint16_t half = static_cast<uint16_t>(h);
        int exp = (half >> 10) & 0x1F;
        if (exp == 31 && (half & 0x3FF) != 0) {
            continue; // NaN payloads are not preserved bit for bit
        }
        ASSERT_EQ(packHalf1x16(unpackHalf(half)), half) << std::hex << h;
    }
}

TEST(HalfPack, MidpointsRoundToEven)
{
    for (uint16_t h = 0; h < 0x7BFF; ++h) {
        double mid = (static_cast<double>(unpackHalf(h)) + static_cast<double>(unpackHalf(static_cast<uint16_t>(h + 1)))) / 2.0;
        float f = static_cast<float>(mid);
        if (static_cast<double>(f) != mid) {
            continue;
        }
        uint16_t expect = (h & 1u) == 0u ? h : static_cast<uint16_t>(h + 1);
        ASSERT_EQ(packHalf1x16(f), expect) << std::hex << h;
    }
}

TEST(HalfPack, SubnormalsMatchReference)
{
    // the old code shifted subnormals by 1 - exp instead of 14 - exp, producing garbage here
    EXPECT_EQ(packHalf1x16(std::ldexp(1.0f, -24)), 0x0001);
    EXPECT_EQ(packHalf1x16(std::ldexp(1.0f, -15)), 0x0200);
    EXPECT_EQ(packHalf1x16(std::ldexp(1.0f, -14)), 0x0400);
    EXPECT_EQ(packHalf1x16(std::ldexp(1.0f, -25)), 0x0000);
    EXPECT_EQ(packHalf1x16(std::ldexp(1.5f, -25)), 0x0001);
    EXPECT_EQ(packHalf1x16(-std::ldexp(3.0f, -24)), 0x8003);
}

TEST(HalfPack, RandomFloatsMatchReference)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> bits;
    for (int i = 0; i < 200000; ++i) {
        uint32_t b = bits(rng);
        // bias towards the half range so most samples are interesting
        b = (b & 0x807FFFFFu) | ((100u + (b >> 23) % 60u) << 23);
        float f;
        std::memcpy(&f, &b, sizeof(f));
        ASSERT_EQ(packHalf1x16(f), referencePack(f)) << f;
    }
}

TEST(HalfPack, Specials)
{
    EXPECT_EQ(packHalf1x16(INFINITY), 0x7C00);
    EXPECT_EQ(packHalf1x16(-INFINITY), 0xFC00);
    EXPECT_EQ(packHalf1x16(1e10f), 0x7C00);
    EXPECT_EQ(packHalf1x16(65504.0f), 0x7BFF);
    EXPECT_EQ(packHalf1x16(65520.0f), 0x7C00);
    EXPECT_EQ(packHalf1x16(-0.0f), 0x8000);
    EXPECT_EQ(packHalf1x16(NAN) & 0x7C00, 0x7C00);
    EXPECT_NE(packHalf1x16(NAN) & 0x3FF, 0);
}

TEST(ColorConvert, HsvRoundTrip)
{
    for (float h = 0.0f; h < 1.0f; h += 0.05f) {
        for (float s : {0.25f, 0.5f, 1.0f}) {
            for (float v : {0.25f, 0.75f, 1.0f}) {
                vec3 back = rgbToHsv(hsvToRgb(h, s, v));
                EXPECT_NEAR(back.x, h, 1e-4f);
                EXPECT_NEAR(back.y, s, 1e-4f);
                EXPECT_NEAR(back.z, v, 1e-4f);
            }
        }
    }
}

TEST(ColorConvert, GrayHasNoHue)
{
    vec3 hsv = rgbToHsv(Color3(0.5f, 0.5f, 0.5f));
    EXPECT_FLOAT_EQ(hsv.y, 0.0f);
    EXPECT_FLOAT_EQ(hsv.z, 0.5f);
}
