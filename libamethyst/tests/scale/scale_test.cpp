#include "common/ui_test_helpers.h"
#include "text/text_test_env.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/extensions/ui_list_layout.h"
#include "components/scrolling_frame.h"
#include "components/slider.h"
#include "rendering/instance_data.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct ScaleFixture : ::testing::Test {
    TestWindow win;
};

void expectRect(const vec4 &rect, float x0, float y0, float x1, float y1)
{
    EXPECT_FLOAT_EQ(rect.x, x0);
    EXPECT_FLOAT_EQ(rect.y, y0);
    EXPECT_FLOAT_EQ(rect.z, x1);
    EXPECT_FLOAT_EQ(rect.w, y1);
}

// a press counter that also remembers the position it was handed
class PressRecorder : public TestNode {
  public:
    int presses = 0;
    vec2 lastPosition = vec2(-1.0f);

  protected:
    EventResult onInputBegan(const InputObject &input) override
    {
        presses++;
        lastPosition = vec2(input.position.x, input.position.y);
        return EventResult::CONSUMED;
    }
};

} // namespace

TEST_F(ScaleFixture, ScalesAroundTheAnchorPoint)
{
    auto *node = win->add<TestNode>();
    node->setBaseProperties({.anchorPoint = vec2(0.5f), .position = UDim2::fromOffset(100, 100), .size = UDim2::fromOffset(50, 50), .scale = 2.0f});
    win.layout();

    // layout does not see the scale
    EXPECT_EQ(node->absolutePosition, vec2(75.0f, 75.0f));
    EXPECT_EQ(node->absoluteSize, vec2(50.0f, 50.0f));

    EXPECT_FLOAT_EQ(node->absoluteScale, 2.0f);
    expectRect(node->screenRect(), 50.0f, 50.0f, 150.0f, 150.0f);
}

TEST_F(ScaleFixture, NestedScalesMultiply)
{
    auto *outer = win->add<TestNode>();
    outer->setBaseProperties({.position = UDim2::fromOffset(0, 0), .size = UDim2::fromOffset(200, 200), .scale = 2.0f});
    auto *inner = outer->add<TestNode>();
    inner->setBaseProperties({.position = UDim2::fromOffset(10, 10), .size = UDim2::fromOffset(20, 20), .scale = 0.5f});
    auto *leaf = inner->add<TestNode>();
    place(leaf, 0, 0, 20, 20);
    win.layout();

    // inner shrinks around (10, 10) to 10x10, then outer doubles everything from the origin
    EXPECT_FLOAT_EQ(inner->absoluteScale, 1.0f);
    expectRect(inner->screenRect(), 20.0f, 20.0f, 40.0f, 40.0f);
    expectRect(leaf->screenRect(), 20.0f, 20.0f, 40.0f, 40.0f);
    EXPECT_EQ(leaf->toLayout(leaf->toScreen(vec2(13.0f, 17.0f))), vec2(13.0f, 17.0f));
}

TEST_F(ScaleFixture, ListLayoutIgnoresAScaledChild)
{
    auto *list = win->add<TestNode>();
    place(list, 0, 0, 300, 300);
    list->addExtension<UIListLayout>();
    TestNode *items[3];
    for (uint32_t i = 0; i < 3; i++) {
        items[i] = list->add<TestNode>();
        items[i]->setBaseProperties({.layoutOrder = i, .size = UDim2::fromOffset(100, 40)});
    }
    win.layout();
    const float before = items[2]->absolutePosition.y;

    items[1]->setBaseProperties({.scale = 1.5f});
    win.layout();
    EXPECT_FLOAT_EQ(items[2]->absolutePosition.y, before);
    EXPECT_FLOAT_EQ(items[1]->absoluteSize.y, 40.0f);
    EXPECT_FLOAT_EQ(items[1]->screenRect().w - items[1]->screenRect().y, 60.0f);
}

TEST_F(ScaleFixture, HitTestingUsesTheDrawnRectAndHandsOverLayoutPositions)
{
    auto *node = win->add<PressRecorder>();
    node->setBaseProperties({.position = UDim2::fromOffset(100, 100), .size = UDim2::fromOffset(50, 50), .scale = 2.0f});
    win.layout();

    // drawn over (100,100)-(200,200): outside the layout rect, inside the drawn one
    win.click(180, 180);
    EXPECT_EQ(node->presses, 1);
    EXPECT_EQ(node->lastPosition, vec2(140.0f, 140.0f));

    // past the drawn rect misses
    win.click(210, 210);
    EXPECT_EQ(node->presses, 1);
}

TEST_F(ScaleFixture, ClipRectsAreInWindowCoordinates)
{
    auto *outer = win->add<TestNode>();
    outer->setBaseProperties({.position = UDim2::fromOffset(10, 10), .size = UDim2::fromOffset(100, 100), .scale = 2.0f});
    auto *child = outer->add<TestNode>();
    place(child, 0, 0, 500, 500);
    win.layout();

    // the outer node clips its children to where it is drawn
    expectRect(child->clipRect, 10.0f, 10.0f, 210.0f, 210.0f);
}

TEST_F(ScaleFixture, DragMovesByTheDistanceInTheParentsLayout)
{
    auto *outer = win->add<TestNode>();
    outer->setBaseProperties({.position = UDim2::fromOffset(0, 0), .size = UDim2::fromOffset(400, 400), .scale = 2.0f});
    auto *handle = outer->add<TestNode>();
    place(handle, 10, 10, 20, 20);
    handle->addExtension<UIDragDetector>();
    win.layout();

    // drawn at (20,20)-(60,60); dragging 40 window pixels is 20 in the parent's layout
    win.press(30, 30);
    win.move(70, 30);
    win.release(70, 30);
    win.layout();
    EXPECT_FLOAT_EQ(handle->getBaseProperties().position.offset.x, 30.0f);
    EXPECT_FLOAT_EQ(handle->getBaseProperties().position.offset.y, 10.0f);
}

TEST_F(ScaleFixture, ScrollingFrameCountsAScaledChildAtItsDrawnSize)
{
    auto *frame = win->add<ScrollingFrame>();
    place(frame, 0, 0, 200, 200);
    frame->setScrollingFrameProperties({.canvasSize = UDim2::fromOffset(0, 0), .automaticCanvasSize = AutomaticSize::XY});
    auto *content = frame->add<TestNode>();
    content->setBaseProperties({.position = UDim2::fromOffset(0, 0), .size = UDim2::fromOffset(150, 150), .scale = 3.0f});
    win.layout();

    // drawn 450 tall in a 200 tall viewport, so there is room to scroll
    EXPECT_GE(frame->getMaxScroll().y, 250.0f);
}

TEST_F(ScaleFixture, SliderTakesItsValueFromWhereItIsDrawn)
{
    auto *outer = win->add<TestNode>();
    outer->setBaseProperties({.position = UDim2::fromOffset(0, 0), .size = UDim2::fromOffset(400, 400), .scale = 2.0f});
    SliderFloat *slider = outer->add<SliderFloat>();
    place(slider, 0, 0, 200, 20);
    float value = 0.0f;
    slider->value = &value;
    slider->min = 0.0f;
    slider->max = 100.0f;
    win.layout();

    // drawn across 0..400, so the middle of the window span is the middle of the range
    win.press(200, 20);
    win.release(200, 20);
    EXPECT_NEAR(value, 50.0f, 2.0f);
}

TEST(InstanceScale, CornerRadiiSurviveTheirPacking)
{
    InstanceData data{};
    data.setCornerRadii(vec4(0.0f, 4.0f, 8.5f, 1000.0f), vec2(100000.0f));
    const vec4 radii = data.getCornerRadii();
    EXPECT_FLOAT_EQ(radii.x, 0.0f);
    EXPECT_NEAR(radii.y, 4.0f, 4.0f * 0.008f);
    EXPECT_NEAR(radii.z, 8.5f, 8.5f * 0.008f);
    EXPECT_NEAR(radii.w, 1000.0f, 1000.0f * 0.008f);
}

TEST(InstanceScale, OverlappingRadiiShrinkTogether)
{
    InstanceData data{};
    // the left side holds 60 + 60 on 100, so every radius shrinks by 100/120
    data.setCornerRadii(vec4(60.0f, 10.0f, 10.0f, 60.0f), vec2(200.0f, 100.0f));
    const vec4 radii = data.getCornerRadii();
    EXPECT_NEAR(radii.x, 50.0f, 0.5f);
    EXPECT_NEAR(radii.y, 10.0f * 100.0f / 120.0f, 0.1f);
    EXPECT_NEAR(radii.w, 50.0f, 0.5f);
}

TEST(InstanceScale, TransformScalesTheLengthsItCarries)
{
    InstanceData data{};
    data.translation = vec2(10.0f, 20.0f);
    data.scale = vec2(30.0f, 40.0f);
    data.setPrimitiveType(PRIMITIVE_RECT);
    data.setBorderThickness(1.0f);
    data.setCornerRadii(vec4(4.0f), data.scale);

    InstanceData scaled = data;
    scaled.transform(2.0f, vec2(5.0f, 7.0f), true);
    EXPECT_EQ(scaled.translation, vec2(25.0f, 47.0f));
    EXPECT_EQ(scaled.scale, vec2(60.0f, 80.0f));
    EXPECT_FLOAT_EQ(unpackHalf1x16(static_cast<uint16_t>(scaled.rotationBorderThickness >> 16)), 2.0f);
    EXPECT_NEAR(scaled.getCornerRadii().x, 8.0f, 0.1f);
    EXPECT_NE(scaled.flags & INSTANCE_FLAG_UNSNAPPED, 0u);

    // a fixed thickness keeps its window width
    InstanceData fixedThickness = data;
    fixedThickness.transform(2.0f, vec2(0.0f), false);
    EXPECT_FLOAT_EQ(unpackHalf1x16(static_cast<uint16_t>(fixedThickness.rotationBorderThickness >> 16)), 1.0f);

    // no scale leaves the instance to be snapped as before
    InstanceData untouched = data;
    untouched.transform(1.0f, vec2(0.0f), true);
    EXPECT_EQ(untouched.flags & INSTANCE_FLAG_UNSNAPPED, 0u);
}

TEST(TextScale, LinesBreakTheSameAtAnyRenderScale)
{
    TextEnv env;
    TextLayoutParams params;
    params.font = env.roboto;
    params.fontSize = 14.0f;
    params.bounds = vec2(120.0f, 200.0f);
    params.wrap = true;

    BatchedText plain = env.processor.layoutTextBatched("the quick brown fox jumps over the lazy dog", params);
    params.renderScale = 1.37f;
    BatchedText scaled = env.processor.layoutTextBatched("the quick brown fox jumps over the lazy dog", params);

    ASSERT_GT(plain.lines.size(), 1u);
    ASSERT_EQ(plain.lines.size(), scaled.lines.size());
    for (size_t i = 0; i < plain.lines.size(); i++) {
        EXPECT_EQ(plain.lines[i].glyphCount, scaled.lines[i].glyphCount);
    }
    // the quad stays in layout space, only the glyphs grow
    EXPECT_FLOAT_EQ(plain.pos.x, scaled.pos.x);
    EXPECT_FLOAT_EQ(plain.size.x, scaled.size.x);
    EXPECT_FLOAT_EQ(scaled.lineHeightPx, plain.lineHeightPx * 1.37f);
}
