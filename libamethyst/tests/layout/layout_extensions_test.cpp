#include "common/ui_test_helpers.h"

#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct LayoutFixture : ::testing::Test {
    TestWindow win;
    TestNode *box = nullptr;

    void SetUp() override
    {
        box = win->add<TestNode>();
        place(box, 0, 0, 300, 300);
    }

    TestNode *child(float w, float h, uint32_t order = 0)
    {
        auto *node = box->add<TestNode>();
        node->setBaseProperties({.layoutOrder = order, .size = UDim2::fromOffset(w, h)});
        return node;
    }

    void resize(float w, float h)
    {
        place(box, 0, 0, w, h);
        win.layout();
    }
};

} // namespace

TEST_F(LayoutFixture, ListFillShrinksWithTheContainer)
{
    auto *list = box->addExtension<UIListLayout>();
    list->verticalFlex = UiFlexAlignment::FILL;
    TestNode *a = child(100, 50);
    TestNode *b = child(100, 50);
    win.layout();
    EXPECT_FLOAT_EQ(a->absoluteSize.y, 150.0f);
    EXPECT_FLOAT_EQ(b->absolutePosition.y, 150.0f);

    resize(300, 100);
    EXPECT_FLOAT_EQ(a->absoluteSize.y, 50.0f);
    EXPECT_FLOAT_EQ(b->absolutePosition.y, 50.0f);
    // no overlap
    EXPECT_LE(a->absolutePosition.y + a->absoluteSize.y, b->absolutePosition.y);

    resize(300, 300);
    EXPECT_FLOAT_EQ(a->absoluteSize.y, 150.0f);
}

TEST_F(LayoutFixture, ListFillKeepsScaleSizes)
{
    auto *list = box->addExtension<UIListLayout>();
    list->verticalFlex = UiFlexAlignment::FILL;
    // half the container each, cross axis by scale too
    TestNode *a = box->add<TestNode>();
    a->setBaseProperties({.size = UDim2::fromScale(0.5f, 0.25f)});
    TestNode *b = box->add<TestNode>();
    b->setBaseProperties({.size = UDim2::fromScale(0.5f, 0.25f)});
    win.layout();
    EXPECT_FLOAT_EQ(a->absoluteSize.x, 150.0f);
    EXPECT_FLOAT_EQ(a->absoluteSize.y, 150.0f);

    // the width still follows the container: the scale size was not overwritten for good
    resize(200, 300);
    EXPECT_FLOAT_EQ(a->absoluteSize.x, 100.0f);
    EXPECT_FLOAT_EQ(b->absoluteSize.x, 100.0f);
}

TEST_F(LayoutFixture, ListSkipsHiddenChildren)
{
    box->addExtension<UIListLayout>();
    TestNode *a = child(100, 40);
    TestNode *hidden = child(100, 40);
    hidden->setBaseProperties({.visible = false});
    TestNode *c = child(100, 40);
    win.layout();
    EXPECT_FLOAT_EQ(a->absolutePosition.y, 0.0f);
    EXPECT_FLOAT_EQ(c->absolutePosition.y, 40.0f);
}

TEST_F(LayoutFixture, ListKeepsInsertionOrderForEqualKeys)
{
    box->addExtension<UIListLayout>();
    std::vector<TestNode *> nodes;
    for (int i = 0; i < 20; i++) {
        nodes.push_back(child(10, 10, 0));
    }
    win.layout();
    for (size_t i = 0; i < nodes.size(); i++) {
        EXPECT_FLOAT_EQ(nodes[i]->absolutePosition.y, static_cast<float>(i) * 10.0f);
    }
}

TEST_F(LayoutFixture, ListSortsByLayoutOrder)
{
    box->addExtension<UIListLayout>();
    TestNode *second = child(10, 10, 2);
    TestNode *first = child(10, 10, 1);
    win.layout();
    EXPECT_FLOAT_EQ(first->absolutePosition.y, 0.0f);
    EXPECT_FLOAT_EQ(second->absolutePosition.y, 10.0f);
}

TEST_F(LayoutFixture, GridAlignsRight)
{
    auto *grid = box->addExtension<UIGridLayout>();
    grid->fillDirection = FillDirection::FILL_HORIZONTAL;
    grid->cellSize = UDim2::fromOffset(50, 50);
    grid->fillDirectionMaxCells = 2;
    grid->horizontalAlignment = HorizontalAlignment::ALIGN_RIGHT;
    TestNode *a = child(1, 1);
    TestNode *b = child(1, 1);
    win.layout();
    // the two-cell row sits against the right edge, cells side by side
    EXPECT_FLOAT_EQ(a->absolutePosition.x, 200.0f);
    EXPECT_FLOAT_EQ(b->absolutePosition.x, 250.0f);
}

TEST_F(LayoutFixture, GridAlignsVertically)
{
    auto *grid = box->addExtension<UIGridLayout>();
    grid->fillDirection = FillDirection::FILL_HORIZONTAL;
    grid->cellSize = UDim2::fromOffset(50, 50);
    grid->fillDirectionMaxCells = 2;
    grid->verticalAlignment = VerticalAlignment::ALIGN_CENTER_V;
    TestNode *a = child(1, 1);
    child(1, 1);
    TestNode *c = child(1, 1);
    win.layout();
    // two rows of 50 centred in 300
    EXPECT_FLOAT_EQ(a->absolutePosition.y, 100.0f);
    EXPECT_FLOAT_EQ(c->absolutePosition.y, 150.0f);

    grid->verticalAlignment = VerticalAlignment::ALIGN_BOTTOM;
    box->markDirty();
    win.layout();
    EXPECT_FLOAT_EQ(c->absolutePosition.y, 250.0f);
}

TEST_F(LayoutFixture, GridCentresWholeRows)
{
    auto *grid = box->addExtension<UIGridLayout>();
    grid->fillDirection = FillDirection::FILL_HORIZONTAL;
    grid->cellSize = UDim2::fromOffset(100, 50);
    grid->fillDirectionMaxCells = 2;
    grid->horizontalAlignment = HorizontalAlignment::ALIGN_CENTER_H;
    TestNode *a = child(1, 1);
    child(1, 1);
    TestNode *c = child(1, 1);
    win.layout();
    EXPECT_FLOAT_EQ(a->absolutePosition.x, 50.0f);
    // the lone cell of the last row is centred on its own
    EXPECT_FLOAT_EQ(c->absolutePosition.x, 100.0f);
}

TEST(AnchorPoint, CentresTheConstrainedSize)
{
    TestWindow win;
    auto *node = win->add<TestNode>();
    // asks for 200x100 but an aspect ratio of 1 makes it 100x100
    node->setBaseProperties({
        .anchorPoint = {0.5f, 0.5f},
        .position = UDim2::fromScale(0.5f, 0.5f),
        .size = UDim2::fromOffset(200, 100),
    });
    auto *ar = node->addExtension<UIAspectRatioConstraint>();
    ar->aspectRatio = 1.0f;
    win.layout();
    ASSERT_FLOAT_EQ(node->absoluteSize.x, node->absoluteSize.y);
    vec2 centre = node->absolutePosition + node->absoluteSize * 0.5f;
    EXPECT_FLOAT_EQ(centre.x, 400.0f);
    EXPECT_FLOAT_EQ(centre.y, 300.0f);
}
