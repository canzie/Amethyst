#include "text/text_test_env.h"

#include "components/text_area.h"
#include "components/text_label.h"

#include <gtest/gtest.h>

#include <limits>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

/** @brief Records the scale the draw context carries when it reaches a node. */
class ScaleProbe : public UIObject {
  public:
    void draw(DrawContext &ctx) override { seen = ctx.logicalToPhysicalScale; }

    float seen = 0.0f;
};

uint32_t s_quadWidth(const GlyphQuad &quad)
{
    return (quad.posMax & 0xFFFFu) - (quad.posMin & 0xFFFFu);
}

} // namespace

TEST(LogicalToPhysicalScale, WindowIsLaidOutInLogicalPixels)
{
    TestWindow win;
    win->setFramebufferSize(vec2(1000.0f, 800.0f));
    EXPECT_EQ(win->absoluteSize, vec2(1000.0f, 800.0f));

    win->setLogicalToPhysicalScale(2.0f);
    EXPECT_EQ(win->absoluteSize, vec2(500.0f, 400.0f));

    win->setLogicalToPhysicalScale(1.25f);
    EXPECT_EQ(win->absoluteSize, vec2(800.0f, 640.0f));

    // a framebuffer resize follows the scale already set
    win->setFramebufferSize(vec2(500.0f, 500.0f));
    EXPECT_EQ(win->absoluteSize, vec2(400.0f, 400.0f));
    EXPECT_EQ(win->getFramebufferSize(), vec2(500.0f, 500.0f));
}

TEST(LogicalToPhysicalScale, InvalidScalesAreIgnored)
{
    TestWindow win;
    win->setLogicalToPhysicalScale(1.5f);
    win->setLogicalToPhysicalScale(0.0f);
    win->setLogicalToPhysicalScale(-2.0f);
    win->setLogicalToPhysicalScale(std::numeric_limits<float>::quiet_NaN());
    win->setLogicalToPhysicalScale(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(win->getLogicalToPhysicalScale(), 1.5f);
}

TEST(LogicalToPhysicalScale, ReachesEveryNodeThroughTheDrawContext)
{
    TextEnv env;
    TestWindow win;
    auto *probe = win->add<ScaleProbe>();
    auto *overlayProbe = win->getOverlayLayer()->add<ScaleProbe>();
    win->setLogicalToPhysicalScale(1.5f);

    win->draw(env.ctx);
    EXPECT_FLOAT_EQ(probe->seen, 1.5f);
    EXPECT_FLOAT_EQ(overlayProbe->seen, 1.5f);
}

TEST(LogicalToPhysicalScale, SnapLandsOnWholePhysicalPixels)
{
    vec2 snapped = snapToPhysicalPixel(vec2(10.0f, 10.5f), 1.5f);
    EXPECT_FLOAT_EQ(snapped.x * 1.5f, 15.0f);
    EXPECT_FLOAT_EQ(snapped.y * 1.5f, 16.0f);

    EXPECT_EQ(snapToPhysicalPixel(vec2(3.0f, 4.0f), 1.0f), vec2(3.0f, 4.0f));
}

TEST(LogicalToPhysicalScale, MeasuredTextKeepsItsLogicalSize)
{
    TextEnv env;
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;

    vec2 at1 = env.processor.measureTextAtlas("Hello, world", params);
    params.logicalToPhysicalScale = 2.0f;
    vec2 at2 = env.processor.measureTextAtlas("Hello, world", params);

    // hinting at the bigger size moves advances by a fraction of a pixel, never by the scale
    EXPECT_NEAR(at2.x, at1.x, 2.0f);
    EXPECT_NEAR(at2.y, at1.y, 1.0f);
}

TEST(LogicalToPhysicalScale, BatchedGlyphsAreRasterisedAtThePhysicalSize)
{
    TextEnv env;
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;
    params.position = vec2(10.0f, 20.0f);

    BatchedText at1 = env.processor.layoutTextBatched("W", params);
    params.logicalToPhysicalScale = 2.0f;
    BatchedText at2 = env.processor.layoutTextBatched("W", params);

    ASSERT_EQ(at1.glyphs.size(), 1u);
    ASSERT_EQ(at2.glyphs.size(), 1u);

    // the glyph data is physical: twice the pixels, give or take hinting
    EXPECT_NEAR(static_cast<float>(s_quadWidth(at2.glyphs[0])), 2.0f * static_cast<float>(s_quadWidth(at1.glyphs[0])), 2.0f);
    EXPECT_NEAR(at2.lineHeightPx, 2.0f * at1.lineHeightPx, 2.0f);

    // the quad is logical and stays where it was
    EXPECT_EQ(at2.pos.x, 10.0f);
    EXPECT_NEAR(at2.size.x, at1.size.x, 1.0f);
    EXPECT_NEAR(at2.size.y, at1.size.y, 1.0f);
}

TEST(LogicalToPhysicalScale, RowCaretPositionsStayLogical)
{
    TextEnv env;
    TextRowLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 16.0f;

    TextRowLayout at1;
    TextRowLayout at2;
    env.processor.layoutTextRow("abc", params, at1);
    params.logicalToPhysicalScale = 2.0f;
    env.processor.layoutTextRow("abc", params, at2);

    ASSERT_EQ(at2.columnX.size(), at1.columnX.size());
    for (size_t i = 0; i < at1.columnX.size(); i++) {
        EXPECT_NEAR(at2.columnX[i], at1.columnX[i], 1.0f);
    }
    EXPECT_NEAR(at2.width, at1.width, 1.0f);

    // the glyph boxes are physical, for the glyph buffer
    ASSERT_EQ(at2.glyphs.size(), 3u);
    EXPECT_NEAR(at2.glyphs[2].x, 2.0f * at1.glyphs[2].x, 2.0f);
}

TEST(LogicalToPhysicalScale, MetricsStayLogical)
{
    TextEnv env;
    FontMetrics at1 = env.processor.getMetricsAtlas(env.roboto, 16);
    FontMetrics at2 = env.processor.getMetricsAtlas(env.roboto, 16, 2.0f);
    EXPECT_NEAR(at2.lineHeight, at1.lineHeight, 1.0f);
    EXPECT_NEAR(at2.ascender, at1.ascender, 1.0f);
}

TEST(LogicalToPhysicalScale, LabelReshapesWhenTheScaleChanges)
{
    TextEnv env;
    TestWindow win;
    auto *label = win->add<TextLabel>();
    place(label, 0, 0, 200, 40);
    label->setText("W");
    win.layout();

    // the label's slice is the only one, so its first glyph is the W
    auto firstGlyphWidth = [&]() {
        const GlyphBuffer *buffer = env.geometry->getGlyphBuffer();
        return s_quadWidth(buffer->glyphData()[buffer->sliceData()[0].firstGlyph]);
    };

    label->draw(env.ctx);
    ASSERT_TRUE(env.geometry->getGlyphBuffer()->hasLiveSlices());
    uint32_t widthAt1 = firstGlyphWidth();
    vec2 logicalSize = label->getTextSize();

    // nothing about the label changed but the scale, which alone must reshape it
    env.ctx.logicalToPhysicalScale = 2.0f;
    label->markDirty();
    label->draw(env.ctx);
    EXPECT_NEAR(static_cast<float>(firstGlyphWidth()), 2.0f * static_cast<float>(widthAt1), 2.0f);
    EXPECT_EQ(label->getTextSize(), logicalSize);
}

TEST(LogicalToPhysicalScale, PhysicalFontSizeRoundsToTheNearestPixel)
{
    TextEnv env;
    // 13 x 1.5 = 19.5, which draws at 20px rather than being cut down to 19
    float at20 = env.processor.getMetricsAtlas(env.roboto, 20.0f).lineHeight;
    float at19 = env.processor.getMetricsAtlas(env.roboto, 19.0f).lineHeight;
    ASSERT_NE(at20, at19);
    EXPECT_FLOAT_EQ(env.processor.getMetricsAtlas(env.roboto, 13.0f, 1.5f).lineHeight * 1.5f, at20);
}

TEST(LogicalToPhysicalScale, TinyFontSizesStillRasteriseAtOnePixel)
{
    TextEnv env;
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 0.2f;
    params.logicalToPhysicalScale = 0.5f;
    EXPECT_GE(env.processor.measureTextAtlas("a", params).y, 0.0f);
    EXPECT_GT(env.processor.getMetricsAtlas(env.roboto, 0.1f).lineHeight, 0.0f);
}
