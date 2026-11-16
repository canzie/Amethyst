#include "common/ui_test_helpers.h"

#include "components/container.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct ButtonFixture : ::testing::Test {
    TestWindow win;
    TestButton *button = nullptr;
    int clicks = 0;
    int rightClicks = 0;

    void SetUp() override
    {
        button = win->add<TestButton>();
        place(button, 10, 10, 100, 40);
        button->onMouseButton1ClickCb = [this] {
            ++clicks;
            return EventResult::CONSUMED;
        };
        button->onMouseButton2ClickCb = [this] {
            ++rightClicks;
            return EventResult::CONSUMED;
        };
        win.layout();
    }

    bool pressed() const { return (button->getGuiState() & GUI_STATE_PRESSED) != 0; }
};

} // namespace

TEST_F(ButtonFixture, ClickInsideFires)
{
    win.click(50, 30);
    EXPECT_EQ(clicks, 1);
    EXPECT_FALSE(pressed());
}

TEST_F(ButtonFixture, ReleaseOutsideClearsPressedState)
{
    win.press(50, 30);
    EXPECT_TRUE(pressed());
    win.release(400, 400);
    EXPECT_FALSE(pressed());
    EXPECT_EQ(clicks, 0);
}

TEST_F(ButtonFixture, StaleReleaseDoesNotClickAfterDragOff)
{
    win.press(50, 30);
    win.release(400, 400);
    // a press that began elsewhere must not turn into a click over the button
    win.press(400, 400);
    win.release(50, 30);
    EXPECT_EQ(clicks, 0);
}

TEST_F(ButtonFixture, RightReleaseWithoutPressIsNoClick)
{
    win.release(50, 30, MOUSE_BUTTON_2);
    EXPECT_EQ(rightClicks, 0);
    win.click(50, 30, MOUSE_BUTTON_2);
    EXPECT_EQ(rightClicks, 1);
}

TEST_F(ButtonFixture, ClickHandlerMayDestroyItsButton)
{
    button->onMouseButton1ClickCb = [this] {
        win->removeChild(button);
        button = nullptr;
        return EventResult::CONSUMED;
    };
    win.click(50, 30);
    EXPECT_EQ(button, nullptr);
    EXPECT_TRUE(win->getChildren().empty());
}

TEST_F(ButtonFixture, InputBeganSignalMayDestroyItsNode)
{
    auto conn = button->onInputBeganCb.connect([this](const InputObject &) { win->removeChild(button); });
    win.press(50, 30);
    win.release(50, 30);
    EXPECT_TRUE(win->getChildren().empty());
}

TEST(WindowInput, HandlerDestroyingAncestorStopsDispatch)
{
    TestWindow win;
    auto *row = win->add<TestNode>();
    place(row, 0, 0, 200, 50);
    auto *cell = row->add<TestButton>();
    place(cell, 0, 0, 100, 50);
    int rowPresses = 0;
    auto rowConn = row->onInputBeganCb.connect([&](const InputObject &) { ++rowPresses; });
    cell->propagate(INTERACTION_CATEGORY_CLICK);
    auto conn = cell->onInputBeganCb.connect([&](const InputObject &) { win->removeChild(row); });
    win.layout();
    win.press(10, 10);
    EXPECT_EQ(rowPresses, 0);
    EXPECT_TRUE(win->getChildren().empty());
}

TEST(WindowHover, EnterAndLeave)
{
    TestWindow win;
    auto *node = win->add<TestNode>();
    place(node, 0, 0, 100, 100);
    win.layout();
    std::vector<bool> changes;
    auto conn = node->onHoverChanged.connect([&](bool h) { changes.push_back(h); });
    win.move(50, 50);
    win.move(300, 300);
    EXPECT_EQ(changes, (std::vector<bool>{true, false}));
}

TEST(WindowHover, RepeatedHoverDoesNotAccumulateSubscriptions)
{
    TestWindow win;
    auto *node = win->add<TestNode>();
    place(node, 0, 0, 100, 100);
    win.layout();
    const size_t before = node->onDestroy.connectionCount();
    for (int i = 0; i < 50; ++i) {
        win.move(50, 50);
        win.move(300, 300);
    }
    EXPECT_EQ(node->onDestroy.connectionCount(), before);
}

TEST(WindowHover, LeaveHandlerDestroyingAnotherHoveredNodeIsSafe)
{
    TestWindow win;
    auto *outer = win->add<TestNode>();
    place(outer, 0, 0, 200, 200);
    auto *inner = outer->add<TestNode>();
    place(inner, 0, 0, 100, 100);
    win.layout();
    win.move(50, 50);
    // leaving runs innermost first; inner's leave destroys outer (and with it inner itself)
    auto conn = inner->onHoverChanged.connect([&](bool hovered) {
        if (!hovered) {
            win->removeChild(outer);
        }
    });
    win.move(500, 500);
    EXPECT_TRUE(win->getChildren().empty());
}

TEST(WindowHover, TransparentOverlayLetsLowerSiblingHover)
{
    TestWindow win;
    auto *button = win->add<TestNode>();
    place(button, 0, 0, 100, 100);
    auto *overlay = win->add<Container>();
    place(overlay, 0, 0, 400, 400);
    overlay->setBaseProperties({.zIndex = 5});
    win.layout();
    bool hovered = false;
    auto conn = button->onHoverChanged.connect([&](bool h) { hovered = h; });
    win.move(50, 50);
    EXPECT_TRUE(hovered);
}

TEST(WindowHover, OpaqueSiblingBlocksHoverBelow)
{
    TestWindow win;
    auto *below = win->add<TestNode>();
    place(below, 0, 0, 100, 100);
    auto *above = win->add<TestNode>();
    place(above, 0, 0, 100, 100);
    above->setBaseProperties({.zIndex = 5});
    win.layout();
    bool hovered = false;
    auto conn = below->onHoverChanged.connect([&](bool h) { hovered = h; });
    win.move(50, 50);
    EXPECT_FALSE(hovered);
}

TEST(WindowHover, ScrollRefreshesHoverOnNextLayout)
{
    TestWindow win;
    auto *node = win->add<TestNode>();
    place(node, 0, 0, 100, 100);
    win.layout();
    win.move(50, 50);
    bool hovered = true;
    auto conn = node->onHoverChanged.connect([&](bool h) { hovered = h; });
    // content moves away under a still cursor, as a scroll would do
    place(node, 0, 200, 100, 100);
    win.scroll(-1.0f, 50, 50);
    win.layout();
    EXPECT_FALSE(hovered);
}

TEST(WindowLifetime, OverlayChildrenDieBeforeWindowInternals)
{
    auto win = std::make_unique<Window>();
    Window *raw = win.get();
    auto *node = win->getOverlayLayer()->add<TestNode>();
    bool sawWindow = false;
    node->onDestroy.detachedOnce([&](Instance *) {
        // the tick registry must still be usable from here
        TickHandle handle = raw->registerTick([](float) {});
        handle.unregister();
        sawWindow = true;
    });
    win.reset();
    EXPECT_TRUE(sawWindow);
}
