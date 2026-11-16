#include "common/ui_test_helpers.h"

#include "components/color_picker.h"
#include "components/drag.h"
#include "components/slider.h"
#include "components/ui_image.h"
#include "utils/number_format.h"

#include <gtest/gtest.h>

#include <climits>
#include <cmath>

using namespace Amethyst;
using namespace AmethystTest;

// --- format validation ----------------------------------------------------------------------

TEST(NumberFormatTest, AcceptsMatchingConversions)
{
    auto f = NumberFormat::parse("%.2f ms", NumberKind::FLOATING);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->format(1.5), "1.50 ms");

    auto i = NumberFormat::parse("%d px", NumberKind::INTEGER);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(i->format(42LL), "42 px");
    EXPECT_EQ(i->format(LLONG_MIN), "-9223372036854775808 px");

    auto pct = NumberFormat::parse("%5.1f%%", NumberKind::FLOATING);
    ASSERT_TRUE(pct.has_value());
    EXPECT_EQ(pct->format(12.34), " 12.3%");

    // wrong or missing length modifiers are rewritten to the argument actually passed
    auto ll = NumberFormat::parse("%hhd", NumberKind::INTEGER);
    ASSERT_TRUE(ll.has_value());
    EXPECT_EQ(ll->format(1000LL), "1000");
    auto hex = NumberFormat::parse("0x%04X", NumberKind::INTEGER);
    ASSERT_TRUE(hex.has_value());
    EXPECT_EQ(hex->format(255LL), "0x00FF");
}

TEST(NumberFormatTest, RejectsUnsafeFormats)
{
    for (const char *bad : {"", "plain", "%s", "%d %d", "%n", "%*d", "%.*f", "%999d", "%", "%l", "%f"}) {
        EXPECT_FALSE(NumberFormat::parse(bad, NumberKind::INTEGER).has_value()) << bad;
    }
    for (const char *bad : {"%d", "%x", "%s", "%f %f", "%c", "%p"}) {
        EXPECT_FALSE(NumberFormat::parse(bad, NumberKind::FLOATING).has_value()) << bad;
    }
}

TEST(NumberFormatTest, LongOutputIsNotTruncated)
{
    auto f = NumberFormat::parse("%99.2f", NumberKind::FLOATING);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->format(1.0).size(), 99u);
    auto big = NumberFormat::parse("%f", NumberKind::FLOATING);
    ASSERT_TRUE(big.has_value());
    EXPECT_GT(big->format(1e300).size(), 300u);
}

TEST(SliderFormat, InvalidFormatFallsBackToDefault)
{
    TestWindow win;
    SliderInt *slider = win->add<SliderInt>();
    int v = 7;
    slider->value = &v;
    slider->setFormat("%s");
    EXPECT_EQ(slider->formatValue(), "7");
    slider->setFormat("%03d");
    EXPECT_EQ(slider->formatValue(), "007");

    SliderFloat *sf = win->add<SliderFloat>();
    float fv = 0.5f;
    sf->value = &fv;
    sf->setFormat("%d");
    EXPECT_EQ(sf->formatValue(), "0.500");
}

TEST(DragFormat, InvalidFormatFallsBackToDefault)
{
    TestWindow win;
    DragInt *d = win->add<DragInt>();
    int64_t v = 12;
    d->value = &v;
    d->setFormat("%d items");
    EXPECT_EQ(d->formatValue(), "12 items");
    d->setFormat("%f");
    EXPECT_EQ(d->formatValue(), "12");

    DragFloat *f = win->add<DragFloat>();
    double dv = 2.0;
    f->value = &dv;
    f->setFormat("%s%s");
    EXPECT_EQ(f->formatValue(), "2.000");
}

// --- slider ranges --------------------------------------------------------------------------

TEST(SliderRange, ReversedAndExtremeIntRangesAreSafe)
{
    TestWindow win;
    SliderInt *slider = win->add<SliderInt>();
    place(slider, 0, 0, 200, 20);
    int v = 5;
    slider->value = &v;

    // min > max used to hand std::clamp an inverted range (UB) and max - min + 1 overflowed
    slider->min = 10;
    slider->max = 0;
    slider->markDirty();
    win.layout();

    slider->min = INT_MIN;
    slider->max = INT_MAX;
    slider->markDirty();
    win.layout();
    EXPECT_EQ(v, 5);
}

TEST(SliderRange, NaNAndInfiniteFloatBoundsAreSafe)
{
    TestWindow win;
    SliderFloat *slider = win->add<SliderFloat>();
    place(slider, 0, 0, 200, 20);
    float v = NAN;
    slider->value = &v;
    slider->min = NAN;
    slider->max = INFINITY;
    slider->markDirty();
    win.layout();
    slider->min = 5.0f;
    slider->max = -5.0f;
    slider->markDirty();
    win.layout();
    SUCCEED();
}

// --- drag scrubbing -------------------------------------------------------------------------

TEST(DragScrub, LogScaleLeavesZeroAndKeepsDirection)
{
    DragFloat d;
    d.scale = ValueScale::LOGARITHMIC;
    double v = 0.0;
    d.value = &v;
    d.speed = 0.05;

    d.applyScrub(10.0f);
    EXPECT_GT(v, 0.0);
    double positive = v;
    d.applyScrub(-20.0f);
    EXPECT_LT(v, 0.0);
    EXPECT_NEAR(v, -positive, 1e-9);

    // rightward always increases, also for negatives
    double before = v;
    d.applyScrub(1.0f);
    EXPECT_GT(v, before);
}

TEST(DragScrub, LogScaleIsMultiplicativeFarFromZero)
{
    DragFloat d;
    d.scale = ValueScale::LOGARITHMIC;
    double v = 1000.0;
    d.value = &v;
    d.speed = 0.1;
    d.applyScrub(1.0f);
    EXPECT_NEAR(v / 1000.0, std::exp(0.1), 1e-3);

    v = -1000.0;
    d.applyScrub(-1.0f);
    EXPECT_NEAR(v / -1000.0, std::exp(0.1), 1e-3);
}

TEST(DragScrub, FractionalIntSpeedAccumulates)
{
    DragInt d;
    int64_t v = 0;
    d.value = &v;
    d.speed = 0.25;
    for (int i = 0; i < 8; ++i) {
        d.applyScrub(1.0f);
    }
    EXPECT_EQ(v, 2);
}

TEST(DragScrub, LogScaleIntUsesFineSpeed)
{
    DragInt d;
    d.scale = ValueScale::LOGARITHMIC;
    int64_t v = 100;
    d.value = &v;
    d.speed = 0.01;
    d.applyScrub(10.0f);
    // about e^0.1 times larger, not the e^10 an integer speed of 1 forced
    EXPECT_GT(v, 105);
    EXPECT_LT(v, 115);
}

TEST(DragScrub, Int32BindingSaturates)
{
    DragInt d;
    int32_t v = INT32_MAX - 1;
    d.valueI = &v;
    d.speed = 1000.0;
    d.applyScrub(10.0f);
    EXPECT_EQ(v, INT32_MAX);
}

// --- colour picker hue ----------------------------------------------------------------------

TEST(ColorPickerHue, GrayAndBlackKeepTheHue)
{
    TestWindow win;
    Color3Picker *picker = win->add<Color3Picker>();
    Color3 c = hsvToRgb(0.6f, 0.8f, 0.9f);
    picker->value = &c;
    picker->syncFromValue();
    EXPECT_NEAR(picker->getHsv().x, 0.6f, 1e-4f);

    c = Color3(0.5f, 0.5f, 0.5f);
    picker->syncFromValue();
    EXPECT_NEAR(picker->getHsv().x, 0.6f, 1e-4f);
    EXPECT_FLOAT_EQ(picker->getHsv().y, 0.0f);

    c = Color3(0.0f, 0.0f, 0.0f);
    picker->syncFromValue();
    EXPECT_NEAR(picker->getHsv().x, 0.6f, 1e-4f);
    EXPECT_FLOAT_EQ(picker->getHsv().z, 0.0f);
}

// --- svg tint -------------------------------------------------------------------------------

TEST(UIImageTint, GradientTintFlattensToFirstStop)
{
    Color4 plain(0.2f, 0.4f, 0.6f, 0.8f);
    EXPECT_FALSE(UIImage::svgTint(plain).hasGradient());

    auto grad = Gradient::linear(0.0f, {{0.0f, Color4(1.0f, 0.0f, 0.0f, 1.0f)}, {1.0f, Color4(0.0f, 0.0f, 1.0f, 1.0f)}});
    Color4 tinted = UIImage::svgTint(Color4::fromGradient(grad));
    EXPECT_FALSE(tinted.hasGradient());
    EXPECT_NEAR(tinted.r, 1.0f, 1e-3f);
    EXPECT_NEAR(tinted.b, 0.0f, 1e-3f);
}
