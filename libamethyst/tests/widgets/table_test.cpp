#include "common/ui_test_helpers.h"

#include "components/container.h"
#include "components/table.h"
#include "components/text_label.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

std::unique_ptr<Instance> s_label(const std::string &text)
{
    auto lbl = std::make_unique<TextLabel>();
    lbl->setText(text);
    return lbl;
}

// how TableScope builds a cell: a full-size container with the content inside
std::unique_ptr<Instance> s_scopedLabelCell(const std::string &text)
{
    auto container = std::make_unique<Container>();
    container->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    auto *lbl = container->add<TextLabel>();
    lbl->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    lbl->setText(text);
    return container;
}

UIObject *s_cellObj(Table *table, uint32_t row, uint32_t col)
{
    return table->getCell(row, col)->asUiObject();
}

struct TableFixture : ::testing::Test {
    TestWindow win;
    Table *table = nullptr;

    void SetUp() override
    {
        table = win->add<Table>();
        place(table, 0, 0, 400, 300);
        table->setTableProperties({.rowHeight = 20.0f, .headerHeight = 20.0f});
        table->addColumn({.header = "a"});
        table->addColumn({.header = "b"});
        fill(5);
        win.layout();
    }

    void fill(int rows)
    {
        for (int r = 0; r < rows; r++) {
            table->addRow();
            table->nextCell(s_label("r" + std::to_string(r)));
            table->nextCell(s_label("x"));
        }
    }

    void fillScoped(int rows)
    {
        for (int r = 0; r < rows; r++) {
            table->addRow();
            table->nextCell(s_scopedLabelCell("a"));
            table->nextCell(s_scopedLabelCell("b"));
        }
    }

    // centre of a body row on screen
    int rowY(int visual) const { return 20 + visual * 20 + 10; }
};

} // namespace

TEST_F(TableFixture, ClickSelectsRow)
{
    int selected = -1;
    table->onRowSelected = [&](uint32_t row) { selected = static_cast<int>(row); };
    win.click(300, rowY(2));
    EXPECT_EQ(selected, 2);
    EXPECT_EQ(table->selectedRow(), 2);
}

TEST_F(TableFixture, RowCallbackMayClearAndRebuild)
{
    int calls = 0;
    table->onRowSelected = [&](uint32_t) {
        // reassigning the running callback must not free the code still executing
        table->onRowSelected = [&](uint32_t) { calls += 100; };
        table->clear();
        fill(3);
        ++calls;
    };
    win.click(300, rowY(1));
    win.layout();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(table->rowCount(), 3u);
    EXPECT_EQ(table->selectedRow(), Table::NO_ROW_SELECTION);

    // the rebuilt rows take clicks again
    win.click(300, rowY(2));
    EXPECT_EQ(calls, 101);
}

TEST_F(TableFixture, DoubleClickCallbackMayClear)
{
    int activated = 0;
    table->onRowSelected = [&](uint32_t) { table->clear(); };
    table->onRowActivated = [&](uint32_t) { ++activated; };
    win->onMouseButton(MOUSE_BUTTON_1, MOUSE_ACTION_PRESS, MOD_DOUBLE_CLICK, 300, rowY(1));
    win.release(300, rowY(1));
    EXPECT_EQ(activated, 1);
}

TEST_F(TableFixture, RowCallbackMayDestroyTable)
{
    table->onRowSelected = [&](uint32_t) {
        win->removeChild(table);
        table = nullptr;
    };
    win.click(300, rowY(1));
    EXPECT_EQ(table, nullptr);
}

TEST_F(TableFixture, ClickOnCellTextSelectsRow)
{
    table->clear();
    fillScoped(4);
    win.layout();

    int selected = -1;
    table->onRowSelected = [&](uint32_t row) { selected = static_cast<int>(row); };
    win.click(50, rowY(3));
    EXPECT_EQ(selected, 3);
    EXPECT_EQ(table->selectedRow(), 3);
}

TEST_F(TableFixture, InteractiveCellKeepsItsClick)
{
    table->clear();
    table->addRow();
    auto button = std::make_unique<TestButton>();
    button->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    int clicks = 0;
    button->track(button->onMouseButton1ClickCb.connect([&] {
        ++clicks;
    }));
    table->nextCell(std::move(button));
    table->nextCell(s_scopedLabelCell("b"));
    win.layout();

    int selected = -1;
    table->onRowSelected = [&](uint32_t row) { selected = static_cast<int>(row); };
    win.click(50, rowY(0));
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(selected, -1);
}

TEST_F(TableFixture, WheelOverCellScrollsTheBody)
{
    table->clear();
    fillScoped(40);
    win.layout();
    float before = s_cellObj(table, 0, 0)->absolutePosition.y;

    win.scroll(-1.0f, 50, rowY(2));
    win.layout();
    EXPECT_LT(s_cellObj(table, 0, 0)->absolutePosition.y, before);

    win.scroll(1.0f, 50, rowY(2));
    win.layout();
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 0)->absolutePosition.y, before);
}

TEST_F(TableFixture, SelectionFollowsRowThroughSort)
{
    win.click(300, rowY(1));
    ASSERT_EQ(table->selectedRow(), 1);

    // reverse the order: logical row 1 now shows at visual position 3
    table->sort([](uint32_t a, uint32_t b) { return a > b; });
    win.layout();
    EXPECT_EQ(table->selectedRow(), 1);
    EXPECT_EQ(table->rowToDisplay(1), 3);
    EXPECT_EQ(table->displayToRow(0), 4);

    int selected = -1;
    table->onRowSelected = [&](uint32_t row) { selected = static_cast<int>(row); };
    win.click(300, rowY(0));
    EXPECT_EQ(selected, 4);
}

TEST_F(TableFixture, RemovingSelectedRowClearsSelection)
{
    win.click(300, rowY(2));
    ASSERT_EQ(table->selectedRow(), 2);
    table->removeRow(0);
    EXPECT_EQ(table->selectedRow(), 2);
    table->removeRow(2);
    EXPECT_EQ(table->selectedRow(), Table::NO_ROW_SELECTION);
}

TEST_F(TableFixture, ClearDropsSelection)
{
    win.click(300, rowY(2));
    table->clear();
    EXPECT_EQ(table->selectedRow(), Table::NO_ROW_SELECTION);
}

TEST_F(TableFixture, TransparentRowsAreStillSelectable)
{
    table->setTableProperties({
        .rowBackgroundColor = Color4{0.0f, 0.0f, 0.0f, 0.0f},
        .rowAlternateColor = Color4{0.0f, 0.0f, 0.0f, 0.0f},
    });
    win.layout();
    int selected = -1;
    table->onRowSelected = [&](uint32_t row) { selected = static_cast<int>(row); };
    win.click(300, rowY(1));
    EXPECT_EQ(selected, 1);
}

namespace {

Table *s_columnTable(TestWindow &win, float width, std::vector<TableColumn> cols)
{
    auto *table = win->add<Table>();
    place(table, 0, 0, width, 300);
    uint32_t count = static_cast<uint32_t>(cols.size());
    table->setColumns(std::move(cols));
    table->addRow();
    for (uint32_t c = 0; c < count; c++) {
        auto cell = std::make_unique<TestNode>();
        cell->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        table->nextCell(std::move(cell));
    }
    win.layout();
    return table;
}

} // namespace

TEST(TableColumns, FixedSizingStillSplitsByWeight)
{
    // FIXED only means the user cannot resize it; its weight is a share like any other column's
    TestWindow win;
    Table *table = s_columnTable(win, 400,
                                 {
                                     {.header = "label", .sizing = TableColumnSizing::FIXED, .weight = 0.4f},
                                     {.header = "value", .weight = 0.6f},
                                 });
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 0)->absoluteSize.x, 160.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 1)->absoluteSize.x, 240.0f);
}

TEST(TableColumns, MaxWidthHandsBackTheRest)
{
    TestWindow win;
    Table *table = s_columnTable(win, 400,
                                 {
                                     {.header = "a", .weight = 2.0f},
                                     {.header = "capped", .weight = 1.0f, .maxWidth = 50.0f},
                                     {.header = "b", .weight = 2.0f},
                                 });
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 1)->absoluteSize.x, 50.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 0)->absoluteSize.x, 175.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 2)->absoluteSize.x, 175.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 2)->absolutePosition.x, 225.0f);
}

TEST(TableColumns, MinWidthHolds)
{
    TestWindow win;
    Table *table = s_columnTable(win, 300,
                                 {
                                     {.header = "a", .weight = 1.0f, .minWidth = 200.0f},
                                     {.header = "b", .weight = 1.0f},
                                 });
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 0)->absoluteSize.x, 200.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 1)->absoluteSize.x, 100.0f);
}

TEST(TableColumns, StretchByWeight)
{
    TestWindow win;
    Table *table = s_columnTable(win, 300, {{.header = "a", .weight = 1.0f}, {.header = "b", .weight = 2.0f}});
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 0)->absoluteSize.x, 100.0f);
    EXPECT_FLOAT_EQ(s_cellObj(table, 0, 1)->absoluteSize.x, 200.0f);
}
