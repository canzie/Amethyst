#include "text/text_test_env.h"

#include "parsers/freetype/font_loader.h"

#include <gtest/gtest.h>

#include <cmath>
#include <utility>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct ProcessorFixture : ::testing::Test {
    TextEnv env;
};

} // namespace

TEST_F(ProcessorFixture, FontsLoad)
{
    ASSERT_TRUE(env.roboto.isValid());
    ASSERT_TRUE(env.openSans.isValid());
    EXPECT_NE(env.roboto, env.openSans);
    FontMetrics m = env.processor.getMetricsAtlas(env.roboto, 16);
    EXPECT_GT(m.lineHeight, 0.0f);
    EXPECT_GT(m.ascender, 0.0f);
}

// regression: measurement used to ignore params.lineHeight while layout applied it
TEST_F(ProcessorFixture, MeasureHeightUsesLayoutLinePitch)
{
    FontMetrics m = env.processor.getMetricsAtlas(env.roboto, 16);

    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;
    params.lineHeight = 1.5f;

    vec2 size = env.processor.measureTextAtlas("one\ntwo\nthree", params);
    EXPECT_FLOAT_EQ(size.y, 3.0f * m.lineHeight * 1.5f);

    // and it matches the height the batched layout reserves for its lines
    BatchedText batched = env.processor.layoutTextBatched("one\ntwo\nthree", params);
    EXPECT_FLOAT_EQ(batched.lineHeightPx * static_cast<float>(batched.lines.size()), size.y);
}

TEST_F(ProcessorFixture, MeasureCountsTrailingBreakAndCrlf)
{
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;
    params.lineHeight = 1.0f;
    float line = env.processor.getMetricsAtlas(env.roboto, 16).lineHeight;

    EXPECT_FLOAT_EQ(env.processor.measureTextAtlas("a\n", params).y, 2.0f * line);
    EXPECT_FLOAT_EQ(env.processor.measureTextAtlas("a\r\nb", params).y, 2.0f * line);
    EXPECT_FLOAT_EQ(env.processor.measureTextAtlas("a\rb", params).y, 2.0f * line);
}

TEST_F(ProcessorFixture, RowLayoutPlacesTabsOnStops)
{
    TextRowLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;
    params.tabSize = 4.0f;

    float space = env.atlas.getAdvance(env.roboto, ' ', 16);
    float tab = 4.0f * space;

    TextRowLayout row;
    env.processor.layoutTextRow("a\tb\t", params, row);
    ASSERT_EQ(row.columnX.size(), 5u);
    EXPECT_FLOAT_EQ(row.columnX[0], 0.0f);
    EXPECT_FLOAT_EQ(row.columnX[2], tab);
    EXPECT_FLOAT_EQ(row.columnX[4], 2.0f * tab);
    EXPECT_FLOAT_EQ(row.width, 2.0f * tab);
}

TEST_F(ProcessorFixture, RowLayoutMultiByteSlotsShareStartX)
{
    TextRowLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;

    TextRowLayout row;
    env.processor.layoutTextRow("a\xC3\xA9z", params, row);
    ASSERT_EQ(row.columnX.size(), 5u);
    EXPECT_FLOAT_EQ(row.columnX[1], row.columnX[2]);
    EXPECT_GT(row.columnX[3], row.columnX[1]);
}

TEST_F(ProcessorFixture, MeasureWidthAgreesWithRowLayout)
{
    const char *text = "Tab\tstops and \xC3\xA9";
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 18.0f;

    TextRowLayoutParams rowParams;
    rowParams.font = env.roboto;
    rowParams.fontSize = 18.0f;

    TextRowLayout row;
    env.processor.layoutTextRow(text, rowParams, row);
    EXPECT_NEAR(env.processor.measureTextAtlas(text, params).x, row.width, 0.01f);
}

TEST_F(ProcessorFixture, DifferentFontsMeasureDifferently)
{
    vec2 roboto = env.processor.measureTextAtlas("The quick brown fox", env.roboto, 16);
    vec2 openSans = env.processor.measureTextAtlas("The quick brown fox", env.openSans, 16);
    EXPECT_NE(roboto.x, openSans.x);
}

// regression: the cache key had no font, so a family-only style change kept the old glyphs
TEST(TextLayoutState, FontIsPartOfTheKey)
{
    TextLayoutState a;
    a.font = 0;
    a.fontSize = 14.0f;
    TextLayoutState b = a;
    EXPECT_TRUE(a.matches(b));
    b.font = 1;
    EXPECT_FALSE(a.matches(b));
    b = a;
    b.origin = {10.0f, 20.0f};
    EXPECT_TRUE(a.matches(b));
}

// regression: reloading a face kept the cached pixel size, so setPixelSize(same) skipped the
// new face and it reported zero metrics
TEST(FontLoaderTest, ReloadThenSamePixelSizeAppliesToNewFace)
{
    FontLoader loader;
    ASSERT_TRUE(loader.loadFont(robotoPath()));
    loader.setPixelSize(16);
    float before = loader.getMetrics().lineHeight;
    ASSERT_GT(before, 0.0f);

    ASSERT_TRUE(loader.loadFont(openSansPath()));
    loader.setPixelSize(16);
    EXPECT_GT(loader.getMetrics().lineHeight, 0.0f);
    EXPECT_GT(loader.getAdvance('M'), 0.0f);
}

// regression: move-assignment dropped the old Impl without FT_Done_Face (LeakSanitizer
// reports it at exit under the sanitizer build)
TEST(FontLoaderTest, MoveAssignOverLoadedLoaderReleasesOldFace)
{
    FontLoader a;
    ASSERT_TRUE(a.loadFont(robotoPath()));
    FontLoader b;
    ASSERT_TRUE(b.loadFont(openSansPath()));
    a = std::move(b);
    EXPECT_TRUE(a.isLoaded());
    EXPECT_EQ(a.getDescription().family, "Open Sans");

    FontLoader c(std::move(a));
    EXPECT_TRUE(c.isLoaded());
}
