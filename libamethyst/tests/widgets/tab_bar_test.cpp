#include "common/ui_test_helpers.h"

#include "components/frame.h"
#include "components/tab_bar.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

constexpr float TAB_W = 100.0f;
constexpr float BAR_H = 30.0f;

struct TabFixture : ::testing::Test {
    TestWindow win;
    TabBar *bar = nullptr;
    std::vector<Instance *> contents;

    void SetUp() override
    {
        bar = win->add<TabBar>();
        place(bar, 0, 0, 600, 400);
        bar->setTabBarProperties({.barThickness = BAR_H, .tabWidth = TAB_W, .tabSpacing = 0.0f, .tabOffset = 0.0f});
        for (int i = 0; i < 4; i++) {
            contents.push_back(bar->addTab(std::make_unique<Frame>(), "tab" + std::to_string(i)));
        }
        win.layout();
    }

    int tabX(int index, float offset = 0.0f) const { return static_cast<int>(offset + index * TAB_W + TAB_W / 2); }
    static int tabY() { return static_cast<int>(BAR_H / 2); }

    std::vector<Instance *> order() const
    {
        std::vector<Instance *> result;
        for (int32_t i = 0; i < bar->getTabCount(); i++) {
            result.push_back(bar->getTabContent(i));
        }
        return result;
    }
};

} // namespace

TEST_F(TabFixture, DragAcrossSeveralTabsKeepsTheirOrder)
{
    win.press(tabX(0), tabY());
    win.move(tabX(0) + 10, tabY());
    win.move(tabX(3), tabY());
    win.release(tabX(3), tabY());

    std::vector<Instance *> expected = {contents[1], contents[2], contents[3], contents[0]};
    EXPECT_EQ(order(), expected);
    EXPECT_EQ(bar->getSelectedContent(), contents[0]);
}

TEST_F(TabFixture, DragRespectsTabOffset)
{
    bar->setTabBarProperties({.tabOffset = 200.0f});
    win.layout();

    // tab 0 now sits at x 200..300, dragging it right by one tab moves it to index 1
    win.press(tabX(0, 200.0f), tabY());
    win.move(tabX(0, 200.0f) + 10, tabY());
    win.move(tabX(1, 200.0f), tabY());
    win.release(tabX(1, 200.0f), tabY());

    std::vector<Instance *> expected = {contents[1], contents[0], contents[2], contents[3]};
    EXPECT_EQ(order(), expected);
}

TEST_F(TabFixture, ClosingActiveMiddleTabSelectsNeighbour)
{
    bar->select(1);
    int changedTo = -1;
    bar->onSelectionChanged = [&](int32_t idx) { changedTo = idx; };

    auto removed = bar->removeTab(contents[1]);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(changedTo, 1);
    EXPECT_EQ(bar->getSelectedContent(), contents[2]);
}

TEST_F(TabFixture, ClosingTabBeforeSelectionReportsShiftedIndex)
{
    bar->select(2);
    std::vector<int32_t> changes;
    bar->onSelectionChanged = [&](int32_t idx) { changes.push_back(idx); };
    bar->removeTab(contents[0]);
    EXPECT_EQ(changes, std::vector<int32_t>{1});
    EXPECT_EQ(bar->getSelectedContent(), contents[2]);

    // a tab after the selection leaves it where it is
    bar->removeTab(contents[3]);
    EXPECT_EQ(changes.size(), 1u);
}

TEST_F(TabFixture, CloseCallbackMayRemoveTheTabItself)
{
    bar->setTabBarProperties({.closeButtonVisibility = TabCloseButtonVisibility::ALWAYS});
    win.layout();
    std::unique_ptr<Instance> kept;
    bar->onTabClosed = [&](Instance *content) { kept = bar->removeTab(content); };

    // the close button sits at the right end of the tab
    win.click(static_cast<int>(TAB_W - 15), tabY());
    EXPECT_NE(kept, nullptr);
    EXPECT_EQ(bar->getTabCount(), 3);
}

TEST(TabTearOff, ActiveStateMovesWithTheSelection)
{
    TestWindow win;
    auto *source = win->add<TabBar>();
    place(source, 0, 0, 600, 200);
    auto *target = win->add<TabBar>();
    place(target, 0, 300, 600, 200);
    for (auto *bar : {source, target}) {
        bar->setTabBarProperties({.barThickness = BAR_H, .tabWidth = TAB_W, .tabTearOffEnabled = true});
    }
    Instance *a = source->addTab(std::make_unique<Frame>(), "a");
    source->addTab(std::make_unique<Frame>(), "b");
    target->addTab(std::make_unique<Frame>(), "c");
    win.layout();

    std::unique_ptr<TabBar::Tab> torn;
    source->onTornOffTabReleased = [&](std::unique_ptr<TabBar::Tab> tab, vec2) { torn = std::move(tab); };

    // drag the active tab "a" straight down, past the soft lock
    win.press(50, 15);
    win.move(50, 100);
    win.move(50, 320);
    win.release(50, 320);
    ASSERT_NE(torn, nullptr);
    ASSERT_EQ(torn->content.get(), a);

    // the source bar's remaining tab took over, and the torn tab dropped its active state
    EXPECT_EQ(source->getTabCount(), 1);
    std::vector<std::unique_ptr<TabBar::Tab>> left = source->removeAllTabs();
    EXPECT_NE(left[0]->labelFrame->getGuiState() & GUI_STATE_ACTIVE, 0);
    EXPECT_EQ(torn->labelFrame->getGuiState() & GUI_STATE_ACTIVE, 0);

    // dropped into a bar that already has an active tab, it arrives inactive
    TabBar::Tab *tornTab = torn.get();
    target->addTab(std::move(torn));
    EXPECT_EQ(tornTab->labelFrame->getGuiState() & GUI_STATE_ACTIVE, 0);
}
