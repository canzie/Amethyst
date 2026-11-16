#include "common/ui_test_helpers.h"

#include "components/context_menu.h"
#include "components/context_menu_item.h"
#include "components/dropdown.h"
#include "components/frame.h"
#include "components/overlay_layer.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

constexpr int ITEM_H = 24;
constexpr float MENU_W = 180.0f;

struct MenuFixture : ::testing::Test {
    TestWindow win{800.0f, 600.0f};
    Frame *owner = nullptr;
    ContextMenu *menu = nullptr;

    void SetUp() override
    {
        owner = win->add<Frame>();
        place(owner, 10, 10, 100, 30);
        menu = owner->add<ContextMenu>();
        menu->itemHeight = static_cast<float>(ITEM_H);
        menu->popupWidth = MENU_W;
        win.layout();
    }

    void open()
    {
        menu->show(owner);
        win.layout();
    }

    // the menu opens below the owner, so item i is centred here
    int itemY(int i) const { return 40 + i * ITEM_H + ITEM_H / 2; }

    size_t overlayChildren() { return win->getOverlayLayer()->getChildren().size(); }
};

std::vector<std::unique_ptr<ContextMenu::ItemData>> s_items(std::unique_ptr<ContextMenu::ItemData> a,
                                                            std::unique_ptr<ContextMenu::ItemData> b = nullptr)
{
    std::vector<std::unique_ptr<ContextMenu::ItemData>> items;
    items.push_back(std::move(a));
    if (b != nullptr) {
        items.push_back(std::move(b));
    }
    return items;
}

std::vector<std::unique_ptr<ContextMenu::ItemData>> s_submenuItems()
{
    std::vector<std::unique_ptr<ContextMenu::ItemData>> inner;
    inner.push_back(makeActionItem("inner", [] {}));
    return s_items(makeSubmenuItem("more", std::move(inner)), makeActionItem("plain", [] {}));
}

} // namespace

TEST_F(MenuFixture, ActionRunsAndClosesMenu)
{
    int runs = 0;
    menu->setItems(s_items(makeActionItem("go", [&] { ++runs; })));
    open();
    ASSERT_TRUE(menu->isOpen());
    win.click(50, itemY(0));
    EXPECT_EQ(runs, 1);
    EXPECT_FALSE(menu->isOpen());
}

TEST_F(MenuFixture, ActionMayDestroyTheMenuOwner)
{
    bool closedSeen = false;
    menu->onClosedCb = [&] { closedSeen = true; };
    menu->setItems(s_items(makeActionItem("delete", [&] {
        win->removeChild(owner);
        owner = nullptr;
        menu = nullptr;
    })));
    open();
    win.click(50, itemY(0));
    EXPECT_EQ(owner, nullptr);
    EXPECT_TRUE(closedSeen);
    win.layout();
}

TEST_F(MenuFixture, SubmenuActionMayDestroyTheMenuOwner)
{
    std::vector<std::unique_ptr<ContextMenu::ItemData>> inner;
    inner.push_back(makeActionItem("delete", [&] {
        win->removeChild(owner);
        owner = nullptr;
    }));
    menu->setItems(s_items(makeSubmenuItem("more", std::move(inner))));
    open();
    win.move(50, itemY(0));
    win.layout();

    // the submenu opens to the right of its row, its first item level with it
    win.click(static_cast<int>(10 + MENU_W + 40), itemY(0));
    EXPECT_EQ(owner, nullptr);
    win.layout();
}

TEST_F(MenuFixture, ToggleMayDestroyTheMenuOwner)
{
    menu->setItems(s_items(makeToggleItem("t", [&](bool) {
        win->removeChild(owner);
        owner = nullptr;
    })));
    open();
    win.click(50, itemY(0));
    EXPECT_EQ(owner, nullptr);
}

TEST_F(MenuFixture, SubmenusDoNotPileUpAcrossReopens)
{
    menu->setItems(s_submenuItems());
    open();
    win.move(50, itemY(0));
    win.layout();
    size_t afterFirst = overlayChildren();

    for (int i = 0; i < 5; i++) {
        menu->hide();
        win.move(400, 400);
        open();
        win.move(50, itemY(0));
        win.layout();
    }
    EXPECT_EQ(overlayChildren(), afterFirst);
}

TEST_F(MenuFixture, SubmenuFlipsAwayFromTheScreenEdge)
{
    // an owner hugging the right edge leaves no room for a submenu on the right
    place(owner, 600, 10, 100, 30);
    win.layout();
    menu->setItems(s_submenuItems());
    menu->show(owner);
    win.layout();
    float menuLeft = menu->absolutePosition.x;
    win.move(static_cast<int>(menuLeft + 20), itemY(0));
    win.layout();

    // the submenu is the menu's only ContextMenu besides itself in the overlay
    ContextMenu *submenu = nullptr;
    for (auto &child : win->getOverlayLayer()->getChildren()) {
        if (auto *cm = child->as<ContextMenu>(); cm != nullptr && cm != menu) {
            submenu = cm;
        }
    }
    ASSERT_NE(submenu, nullptr);
    ASSERT_TRUE(submenu->isOpen());
    // it opened on the left rather than being clamped back over its parent
    EXPECT_LE(submenu->absolutePosition.x + submenu->absoluteSize.x, menuLeft + 0.5f);
}

TEST(DropdownClose, ClosedCallbackFiresOnce)
{
    TestWindow win;
    auto *dropdown = win->add<Dropdown>();
    place(dropdown, 10, 10, 120, 24);
    dropdown->setItems(s_items(makeActionItem("a", [] {})));
    win.layout();

    int closed = 0;
    dropdown->onClosedCb = [&] { ++closed; };
    dropdown->open();
    win.layout();
    ASSERT_TRUE(dropdown->isOpen());
    dropdown->close();
    EXPECT_EQ(closed, 1);
}
