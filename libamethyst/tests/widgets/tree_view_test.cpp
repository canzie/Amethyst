#include "common/ui_test_helpers.h"

#include "components/container.h"
#include "components/text_label.h"
#include "components/tree_view.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

constexpr int ROW_H = 20;

// a full-size container, which passes input on, so row clicks land on the row
std::unique_ptr<Instance> s_cell(const std::string &)
{
    auto container = std::make_unique<Container>();
    container->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    return container;
}

// how TreeViewScope builds a text cell: a full-size container with a label inside
std::unique_ptr<Instance> s_labelCell(const std::string &text)
{
    auto container = std::make_unique<Container>();
    container->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    auto *lbl = container->add<TextLabel>();
    lbl->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    lbl->setText(text);
    return container;
}

struct TreeFixture : ::testing::Test {
    TestWindow win;
    TreeView *tree = nullptr;

    void SetUp() override
    {
        tree = win->add<TreeView>();
        place(tree, 0, 0, 300, 200);
        tree->setTreeViewProperties({.rowHeight = static_cast<float>(ROW_H)});
        tree->addColumn({.header = "name"});
        build();
        win.layout();
    }

    // root 0 { child 1, child 2 }, root 3 { child 4 }, root 5
    void build()
    {
        const uint16_t depths[] = {0, 1, 1, 0, 1, 0};
        for (uint16_t d : depths) {
            tree->addRow(d);
            tree->nextCell(s_cell("row"));
        }
    }

    void buildMany(int count)
    {
        for (int i = 0; i < count; i++) {
            tree->addRow(0);
            tree->nextCell(s_cell("row"));
        }
    }

    int rowY(int visible) const { return visible * ROW_H + ROW_H / 2; }

    // the disclosure triangle of a depth-0 row sits at the left edge, past its padding
    static constexpr int DISCLOSURE_X = 12;
};

} // namespace

// labels consume clicks, so text covering the row hides it from the row; disabled until the row can
// observe input inside it without taking it from the cell content (see todo.md)
TEST_F(TreeFixture, DISABLED_ClickOnCellTextSelectsRow)
{
    tree->clear();
    for (int i = 0; i < 3; i++) {
        tree->addRow(0);
        tree->nextCell(s_labelCell("row"));
    }
    win.layout();
    int clicked = -1;
    tree->onRowClicked = [&](uint32_t row) { clicked = static_cast<int>(row); };
    win.click(150, rowY(2));
    EXPECT_EQ(clicked, 2);
    EXPECT_EQ(tree->selectedRow, 2);
}

TEST_F(TreeFixture, DisclosureTogglesRow)
{
    win.click(DISCLOSURE_X, rowY(0));
    EXPECT_FALSE(tree->isExpanded(0));
    win.layout();

    // rows 1 and 2 are hidden now, so visible row 1 is logical row 3
    int clicked = -1;
    tree->onRowClicked = [&](uint32_t row) { clicked = static_cast<int>(row); };
    win.click(150, rowY(1));
    EXPECT_EQ(clicked, 3);
}

TEST_F(TreeFixture, ToggleCallbackMayClearAndRebuild)
{
    int toggles = 0;
    tree->onRowToggled = [&](uint32_t, bool) {
        tree->onRowToggled = [&](uint32_t, bool) { toggles += 100; };
        tree->clear();
        build();
        ++toggles;
    };
    win.click(DISCLOSURE_X, rowY(0));
    win.layout();
    EXPECT_EQ(toggles, 1);
    EXPECT_EQ(tree->rowCount(), 6u);

    win.click(DISCLOSURE_X, rowY(0));
    EXPECT_EQ(toggles, 101);
}

TEST_F(TreeFixture, RowClickCallbackMayClearAndRebuild)
{
    int clicks = 0;
    tree->onRowClicked = [&](uint32_t) {
        tree->onRowClicked = [&](uint32_t) { clicks += 100; };
        tree->clear();
        build();
        ++clicks;
    };
    win.click(150, rowY(1));
    win.layout();
    EXPECT_EQ(clicks, 1);

    win.click(150, rowY(1));
    EXPECT_EQ(clicks, 101);
}

TEST_F(TreeFixture, RowClickCallbackMayDestroyTree)
{
    tree->onRowClicked = [&](uint32_t) {
        win->removeChild(tree);
        tree = nullptr;
    };
    win.click(150, rowY(1));
    EXPECT_EQ(tree, nullptr);
}

TEST_F(TreeFixture, ClearDropsSelectionAndHover)
{
    win.move(150, rowY(1));
    win.click(150, rowY(1));
    ASSERT_EQ(tree->selectedRow, 1);
    ASSERT_EQ(tree->hoveredRow, 1);

    tree->clear();
    EXPECT_EQ(tree->selectedRow, TreeView::NO_ROW_SELECTION);
    EXPECT_EQ(tree->hoveredRow, TreeView::NO_ROW_SELECTION);
}

TEST_F(TreeFixture, HoverFollowsScroll)
{
    tree->clear();
    buildMany(40);
    win.layout();

    win.move(150, rowY(1));
    ASSERT_EQ(tree->hoveredRow, 1);

    // the slot under the resting cursor now shows a later row
    win.scroll(-1.0f, 150, rowY(1));
    win.layout();
    EXPECT_GT(tree->hoveredRow, 1);

    int clicked = -1;
    tree->onRowClicked = [&](uint32_t row) { clicked = static_cast<int>(row); };
    win.click(150, rowY(1));
    EXPECT_EQ(clicked, tree->hoveredRow);
}

TEST(TreeColumns, FixedSizingStillSplitsByWeight)
{
    TestWindow win;
    auto *tree = win->add<TreeView>();
    place(tree, 0, 0, 300, 200);
    tree->setTreeViewProperties({.showDisclosureTriangles = false});
    tree->addColumn({.header = "a", .sizing = TreeColumnSizing::FIXED, .weight = 1.0f});
    tree->addColumn({.header = "b", .weight = 2.0f});
    tree->addRow(0);
    for (int c = 0; c < 2; c++) {
        auto cell = std::make_unique<TestNode>();
        cell->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        tree->nextCell(std::move(cell));
    }
    win.layout();
    EXPECT_FLOAT_EQ(tree->getCell(0, 0)->asUiObject()->absoluteSize.x, 100.0f);
    EXPECT_FLOAT_EQ(tree->getCell(0, 1)->asUiObject()->absoluteSize.x, 200.0f);
}
