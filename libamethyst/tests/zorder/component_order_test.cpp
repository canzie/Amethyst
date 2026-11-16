#include "text/text_test_env.h"

#include "components/collapsible_header.h"
#include "components/container.h"
#include "components/frame.h"
#include "components/scrolling_frame.h"
#include "components/tab_bar.h"
#include "components/table.h"
#include "components/text_input.h"
#include "components/text_label.h"
#include "components/tree_view.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

uint32_t s_primitive(const InstanceData &data)
{
    return (data.primitiveMode >> 16) & 0xFFu;
}

struct ComponentOrderFixture : ::testing::Test {
    TextEnv env;
    TestWindow win;

    void render()
    {
        win.layout();
        win->draw(env.ctx);
        win->geometryRegistry()->flush();
        win->getOverlayLayer()->geometryRegistry()->flush();
    }

    static std::vector<Instance *> drawChildren(Instance *node)
    {
        std::vector<Instance *> out;
        node->collectSortedDrawChildren(out);
        return out;
    }

    static UIObject *drawChildNamed(Instance *node, const std::string &childName)
    {
        for (Instance *child : drawChildren(node)) {
            if (child->name == childName) {
                return child->asUiObject();
            }
        }
        return nullptr;
    }

    template <typename T> static T *drawChildOfType(Instance *node)
    {
        for (Instance *child : drawChildren(node)) {
            if (auto *typed = dynamic_cast<T *>(child); typed != nullptr) {
                return typed;
            }
        }
        return nullptr;
    }

    static const InstanceData *allocationAt(GeometryRegistry *registry, int32_t z)
    {
        for (const InstanceData &data : registry->getAllocations()) {
            if (data.zIndex == z) {
                return &data;
            }
        }
        return nullptr;
    }
};

std::unique_ptr<Instance> s_label(const std::string &text)
{
    auto lbl = std::make_unique<TextLabel>();
    lbl->setText(text);
    return lbl;
}

} // namespace

TEST_F(ComponentOrderFixture, ScrollbarsDrawOverTheContent)
{
    auto *sf = win->add<ScrollingFrame>();
    place(sf, 0, 0, 200, 100);
    sf->setScrollingFrameProperties(
        {.scrollBarVisibility = ScrollBarVisibility::ALWAYS, .canvasSize = UDim2::fromOffset(200, 400)});
    auto *content = sf->add<Frame>();
    place(content, 0, 0, 400, 400);
    content->setBaseProperties({.zIndex = 50});
    render();

    UIObject *bar = drawChildNamed(sf, "vertical bar");
    UIObject *thumb = drawChildNamed(sf, "vertical thumb");
    ASSERT_NE(bar, nullptr);
    ASSERT_NE(thumb, nullptr);
    EXPECT_LT(content->drawZ(), bar->drawZ());
    EXPECT_LT(bar->drawZ(), thumb->drawZ());
    EXPECT_NE(allocationAt(win->geometryRegistry(), thumb->drawZ()), nullptr);

    // the bars only show the position; a press on one reaches what lies below
    bool contentPressed = false;
    content->track(content->onInputBeganCb.connect([&contentPressed](const InputObject &) { contentPressed = true; }));
    win.click(195, 10);
    EXPECT_TRUE(contentPressed);
}

TEST_F(ComponentOrderFixture, LabelTextDrawsOverItsQuadAndUnderTheNextSibling)
{
    auto *label = win->add<TextLabel>();
    place(label, 0, 0, 200, 30);
    label->setText("hello");
    auto *cover = win->add<Frame>();
    place(cover, 0, 0, 200, 30);
    render();

    const InstanceData *text = allocationAt(win->geometryRegistry(), label->drawZ(1));
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(s_primitive(*text), static_cast<uint32_t>(PRIMITIVE_TEXT));
    EXPECT_LT(label->drawZ(1), cover->drawZ());
}

TEST_F(ComponentOrderFixture, TextFieldDrawsSelectionTextThenCaret)
{
    auto *field = win->add<TextInput>();
    place(field, 10, 10, 300, 30);
    field->setText("hello");
    field->focus();
    field->selectAll();
    render();

    GeometryRegistry *registry = win->geometryRegistry();
    const InstanceData *background = allocationAt(registry, field->drawZ(0));
    const InstanceData *selection = allocationAt(registry, field->drawZ(1));
    const InstanceData *text = allocationAt(registry, field->drawZ(2));
    const InstanceData *caret = allocationAt(registry, field->drawZ(3));
    ASSERT_NE(background, nullptr);
    ASSERT_NE(selection, nullptr);
    ASSERT_NE(text, nullptr);
    ASSERT_NE(caret, nullptr);
    EXPECT_EQ(s_primitive(*selection), static_cast<uint32_t>(PRIMITIVE_RECT));
    EXPECT_EQ(s_primitive(*text), static_cast<uint32_t>(PRIMITIVE_TEXT));
    EXPECT_EQ(s_primitive(*caret), static_cast<uint32_t>(PRIMITIVE_RECT));
}

TEST_F(ComponentOrderFixture, TabButtonsDrawOverContentAndCloseButtonOverTheTab)
{
    auto *tabs = win->add<TabBar>();
    place(tabs, 0, 0, 400, 300);
    tabs->addTab(std::make_unique<Frame>(), "one");
    tabs->addTab(std::make_unique<Frame>(), "two");
    auto *popup = win->getOverlayLayer()->add<Frame>();
    place(popup, 0, 0, 50, 50);
    render();

    std::vector<Instance *> parts = drawChildren(tabs);
    ASSERT_EQ(parts.size(), 4u);
    UIObject *content = tabs->getTabContent(0)->asUiObject();
    UIObject *button = parts.back()->asUiObject();
    ASSERT_NE(content, nullptr);
    ASSERT_NE(button, nullptr);
    EXPECT_LT(content->drawZ(), button->drawZ());

    // inside a tab: its frame, the label text on it, then the close button
    std::vector<Instance *> pieces = drawChildren(button);
    ASSERT_EQ(pieces.size(), 2u);
    UIObject *labelFrame = pieces[0]->asUiObject();
    UIObject *closeButton = pieces[1]->asUiObject();
    auto *label = labelFrame->findFirstChildOfClass<TextLabel>();
    ASSERT_NE(label, nullptr);
    EXPECT_LT(labelFrame->drawZ(), label->drawZ(1));
    EXPECT_LT(label->drawZ(1), closeButton->drawZ());

    // the buttons go into the overlay registry, under the overlay's own content
    GeometryRegistry *overlay = win->getOverlayLayer()->geometryRegistry();
    EXPECT_NE(allocationAt(overlay, labelFrame->drawZ()), nullptr);
    EXPECT_LT(labelFrame->drawZ(), popup->drawZ());
    EXPECT_EQ(overlay->getAllocations().back().zIndex, popup->drawZ());
}

TEST_F(ComponentOrderFixture, CollapsibleHeaderPartsOrder)
{
    auto *header = win->add<CollapsibleHeader>();
    place(header, 0, 0, 300, 200);
    header->setTitle("title");
    Frame *onHeader = nullptr;
    header->header([&onHeader](Frame &bg) {
        onHeader = bg.add<Frame>();
        place(onHeader, 250, 0, 30, 30);
    });
    auto *child = header->add<Frame>();
    place(child, 0, 0, 300, 50);
    render();

    std::vector<Instance *> parts = drawChildren(header);
    ASSERT_EQ(parts.size(), 3u);
    UIObject *button = parts[0]->asUiObject();
    UIObject *background = parts[1]->asUiObject();
    EXPECT_LT(header->drawZ(), button->drawZ());
    EXPECT_LT(button->drawZ(), background->drawZ());
    EXPECT_LT(background->drawZ(), onHeader->drawZ());
    EXPECT_LT(onHeader->drawZ(), child->drawZ());

    // a control on the header takes its own click; elsewhere on the header the toggle does
    bool pressed = false;
    onHeader->track(onHeader->onInputBeganCb.connect([&pressed](const InputObject &) { pressed = true; }));
    win.click(260, 10);
    EXPECT_TRUE(pressed);
    EXPECT_TRUE(static_cast<bool>(header->getCollapsibleHeaderProperties().expanded));
    win.click(100, 10);
    EXPECT_FALSE(static_cast<bool>(header->getCollapsibleHeaderProperties().expanded));

    // collapsed, the children leave the walk
    render();
    EXPECT_EQ(drawChildren(header).size(), 2u);
}

TEST_F(ComponentOrderFixture, TableCellsBetweenRowBackgroundAndScrollbar)
{
    auto *table = win->add<Table>();
    place(table, 0, 0, 400, 100);
    table->setTableProperties({.rowHeight = 20.0f, .headerHeight = 20.0f});
    table->addColumn({.header = "a"});
    for (int r = 0; r < 20; r++) {
        table->addRow();
        table->nextCell(s_label("r" + std::to_string(r)));
    }
    render();

    UIObject *rowBackground = drawChildren(table).front()->asUiObject();
    auto *sf = drawChildOfType<ScrollingFrame>(table);
    ASSERT_NE(sf, nullptr);
    UIObject *thumb = drawChildNamed(sf, "vertical thumb");
    ASSERT_NE(thumb, nullptr);
    UIObject *cell = table->getCell(0, 0)->asUiObject();

    EXPECT_LT(rowBackground->drawZ(), cell->drawZ());
    EXPECT_LT(cell->drawZ(1), thumb->drawZ());
}

TEST_F(ComponentOrderFixture, TreeRowsBelowSeparatorsBelowScrollbar)
{
    auto *tree = win->add<TreeView>();
    place(tree, 0, 0, 300, 100);
    tree->setTreeViewProperties({.rowHeight = 20.0f, .showColumnSeparators = true});
    tree->addColumn({.header = "a"});
    tree->addColumn({.header = "b"});
    for (int r = 0; r < 20; r++) {
        tree->addRow(0);
        tree->nextCell(s_label("r"));
        tree->nextCell(s_label("x"));
    }
    render();

    std::vector<Instance *> parts = drawChildren(tree);
    UIObject *rowBackground = parts.front()->asUiObject();
    auto *sf = drawChildOfType<ScrollingFrame>(tree);
    ASSERT_NE(sf, nullptr);
    UIObject *thumb = drawChildNamed(sf, "vertical thumb");
    ASSERT_NE(thumb, nullptr);
    UIObject *separator = parts[parts.size() - 2]->asUiObject();
    UIObject *cell = tree->getCell(0, 0)->asUiObject();

    EXPECT_EQ(cell->parent, rowBackground);
    EXPECT_LT(rowBackground->drawZ(), cell->drawZ());
    EXPECT_LT(cell->drawZ(1), separator->drawZ());
    EXPECT_LT(separator->drawZ(), thumb->drawZ());
}

TEST_F(ComponentOrderFixture, WheelOverACellScrollsTheTree)
{
    auto *tree = win->add<TreeView>();
    place(tree, 0, 0, 300, 100);
    tree->setTreeViewProperties({.rowHeight = 20.0f});
    tree->addColumn({.header = "a"});
    for (int r = 0; r < 20; r++) {
        tree->addRow(0);
        tree->nextCell(s_label("r"));
    }
    render();

    auto *sf = drawChildOfType<ScrollingFrame>(tree);
    ASSERT_NE(sf, nullptr);
    win.scroll(-1.0f, 50, 30);
    win.layout();
    EXPECT_GT(sf->getScrollOffset().y, 0.0f);
}

TEST_F(ComponentOrderFixture, LabelTextFollowsAReorderWithoutStaleZ)
{
    auto *label = win->add<TextLabel>();
    place(label, 0, 0, 200, 30);
    label->setText("hello");
    render();
    int32_t before = label->drawZ(1);

    // a sibling sorted in front shifts the label's order; the text quad moves with it
    auto *first = win->add<Frame>();
    place(first, 0, 0, 10, 10);
    first->setBaseProperties({.zIndex = 0});
    render();

    EXPECT_NE(label->drawZ(1), before);
    EXPECT_EQ(allocationAt(win->geometryRegistry(), before), nullptr);
    const InstanceData *text = allocationAt(win->geometryRegistry(), label->drawZ(1));
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(s_primitive(*text), static_cast<uint32_t>(PRIMITIVE_TEXT));
}
