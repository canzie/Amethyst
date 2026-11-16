#include "common/ui_test_helpers.h"

#include "components/collapsible_header.h"
#include "components/extensions/ui_tooltip.h"
#include "components/frame.h"
#include "components/overlay_layer.h"
#include "components/scrolling_frame.h"
#include "components/tooltip.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

TEST(CollapsibleHeaderInput, ControlsOnTheHeaderTakeTheirClicks)
{
    TestWindow win;
    auto *header = win->add<CollapsibleHeader>();
    place(header, 0, 0, 300, 200);
    header->setCollapsibleHeaderProperties({.headerHeight = 30.0f});
    header->setTitle("title");

    int buttonClicks = 0;
    header->header([&](Frame &bar) {
        auto *button = bar.add<TestButton>();
        button->setBaseProperties({.position = UDim2::fromOffset(250, 5), .size = UDim2::fromOffset(40, 20)});
        button->onMouseButton1ClickCb = [&] {
            ++buttonClicks;
            return EventResult::CONSUMED;
        };
    });
    int toggles = 0;
    header->onToggled = [&](bool) { ++toggles; };
    win.layout();

    win.click(270, 15);
    EXPECT_EQ(buttonClicks, 1);
    EXPECT_EQ(toggles, 0);

    // the title text is a label, so it lets the click through to the header
    win.click(100, 15);
    EXPECT_EQ(toggles, 1);
    EXPECT_EQ(buttonClicks, 1);
}

TEST(TooltipSurface, PreviousOwnersContentIsCleared)
{
    TestWindow win;
    auto makeOwner = [&](float x, int marker) {
        auto *owner = win->add<TestNode>();
        place(owner, x, 0, 100, 100);
        auto *tip = owner->addExtension<UITooltip>();
        tip->delaySeconds = 0.0f;
        tip->build = [marker](Tooltip &surface) {
            surface.setBaseProperties({.size = UDim2::fromOffset(50, 20)});
            auto *content = surface.add<Frame>();
            content->name = "content" + std::to_string(marker);
        };
        return owner;
    };
    makeOwner(0, 1);
    makeOwner(200, 2);
    win.layout();

    auto surfaceChildren = [&]() -> size_t {
        for (auto &child : win->getOverlayLayer()->getChildren()) {
            if (auto *tooltip = child->as<Tooltip>(); tooltip != nullptr) {
                return tooltip->getChildren().size();
            }
        }
        return 0;
    };

    win.move(50, 50);
    win->tick(0.1f);
    EXPECT_EQ(surfaceChildren(), 1u);

    win.move(250, 50);
    win->tick(0.1f);
    EXPECT_EQ(surfaceChildren(), 1u);
}

TEST(ScrollBars, AlwaysShowsABarWithNothingToScroll)
{
    TestWindow win;
    auto *sf = win->add<ScrollingFrame>();
    place(sf, 0, 0, 200, 200);
    sf->setScrollingFrameProperties({.scrollBarVisibility = ScrollBarVisibility::ALWAYS});
    sf->setScrollingFrameProperties({.scrollBarThickness = 10.0f});
    sf->setScrollingFrameProperties({.canvasSize = UDim2::fromScale(1.0f, 1.0f)});
    win.layout();
    // the bar takes its strip of the viewport even though the canvas fits
    EXPECT_FLOAT_EQ(sf->absoluteContentSize.x, 190.0f);

    sf->setScrollingFrameProperties({.scrollBarVisibility = ScrollBarVisibility::AUTO});
    win.layout();
    EXPECT_FLOAT_EQ(sf->absoluteContentSize.x, 200.0f);
}
