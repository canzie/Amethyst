#include "common/ui_test_helpers.h"

#include "components/collapsible_header.h"
#include "components/frame.h"
#include "components/radio_button.h"
#include "components/slider.h"
#include "components/tab_bar.h"
#include "components/text_label.h"
#include "modules/style.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

// every test installs its own theme and puts the default back, so the global never leaks
struct ThemedFixture : ::testing::Test {
    TestWindow win;

    void TearDown() override { Style::install(Style()); }

    void load(const std::string &source) { ASSERT_TRUE(Style::loadFromString(source)); }
};

bool isColor(const Color3 &c, float r, float g, float b)
{
    return std::abs(c.r - r) < 1e-3f && std::abs(c.g - g) < 1e-3f && std::abs(c.b - b) < 1e-3f;
}

} // namespace

// --- reload ---------------------------------------------------------------------------------

TEST_F(ThemedFixture, ReloadRestylesExistingWidgets)
{
    load("frame { background-color: #ff0000; }");
    Frame *frame = win->add<Frame>();
    place(frame, 0, 0, 50, 50);
    win.layout();
    EXPECT_TRUE(isColor(frame->getBaseStyleProperties().backgroundColor, 1, 0, 0));

    load("frame { background-color: #0000ff; }");
    win.layout();
    EXPECT_TRUE(isColor(frame->getBaseStyleProperties().backgroundColor, 0, 0, 1));
}

TEST_F(ThemedFixture, ReloadReachesTabFrames)
{
    load("tab-bar#tab { background-color: #ff0000; }");
    TabBar *bar = win->add<TabBar>();
    place(bar, 0, 0, 400, 300);
    Frame *tab = nullptr;
    bar->addTab(std::make_unique<Frame>(), [&tab](Frame &f) { tab = &f; });
    win.layout();
    ASSERT_NE(tab, nullptr);
    EXPECT_TRUE(isColor(tab->getBaseStyleProperties().backgroundColor, 1, 0, 0));

    load("tab-bar#tab { background-color: #00ff00; }");
    win.layout();
    EXPECT_TRUE(isColor(tab->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}

// --- padding --------------------------------------------------------------------------------

TEST_F(ThemedFixture, ThemePaddingAppliesUnlessSetOnTheInstance)
{
    load("frame { padding: 6px; }");
    Frame *themed = win->add<Frame>();
    Frame *pinned = win->add<Frame>();
    pinned->setBaseProperties({.padding = UDim4::fromOffset(2.0f)});
    place(themed, 0, 0, 100, 100);
    place(pinned, 0, 0, 100, 100);
    win.layout();
    EXPECT_FLOAT_EQ(themed->getBaseProperties().padding.top.offset, 6.0f);
    EXPECT_FLOAT_EQ(themed->absoluteContentSize.x, 88.0f);
    EXPECT_FLOAT_EQ(pinned->getBaseProperties().padding.top.offset, 2.0f);
}

TEST_F(ThemedFixture, PaddingStartsInsideAnInsetBorder)
{
    load(".bordered { border-pixel-size: 2px; padding: 3px; }");
    Frame *frame = win->add<Frame>();
    frame->setClasses({"bordered"});
    place(frame, 0, 0, 100, 100);
    win.layout();
    EXPECT_FLOAT_EQ(frame->absoluteContentPosition.x, 5.0f);
    EXPECT_FLOAT_EQ(frame->absoluteContentSize.x, 90.0f);
}

TEST_F(ThemedFixture, AnOutlineBorderLeavesTheContentBoxAlone)
{
    load(".outlined { border-pixel-size: 2px; border-mode: outline; padding: 3px; }");
    Frame *frame = win->add<Frame>();
    frame->setClasses({"outlined"});
    place(frame, 0, 0, 100, 100);
    win.layout();
    EXPECT_FLOAT_EQ(frame->absoluteContentPosition.x, 3.0f);
    EXPECT_FLOAT_EQ(frame->absoluteContentSize.x, 94.0f);
}

// --- pseudo-state re-resolve in arrange overrides -------------------------------------------

TEST_F(ThemedFixture, SliderRestylesWhenItsStateChanges)
{
    load("slider { background-color: #ff0000; } slider:disabled { background-color: #00ff00; }");
    SliderFloat *slider = win->add<SliderFloat>();
    place(slider, 0, 0, 200, 20);
    win.layout();
    EXPECT_TRUE(isColor(slider->getBaseStyleProperties().backgroundColor, 1, 0, 0));

    slider->setBaseProperties({.interactable = false});
    win.layout();
    EXPECT_TRUE(isColor(slider->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}

// --- class propagation onto tabs (todo: ".abc and .abc#tab overwrite each other") -----------

struct TabFixture : ThemedFixture {
    TabBar *bar = nullptr;
    Frame *tab = nullptr;

    void build(std::function<void(Frame &)> extra = {})
    {
        bar = win->add<TabBar>();
        place(bar, 0, 0, 400, 300);
        bar->addTab(std::make_unique<Frame>(), [this, extra](Frame &f) {
            tab = &f;
            if (extra) {
                extra(f);
            }
        });
        win.layout();
    }
};

TEST_F(TabFixture, BarClassAndClassPartBothApply)
{
    load(".abc { background-color: #ff0000; border-pixel-size: 4px; } .abc#tab { background-color: #00ff00; }");
    build();
    bar->addClass("abc");
    win.layout();
    EXPECT_TRUE(isColor(bar->getBaseStyleProperties().backgroundColor, 1, 0, 0));
    EXPECT_FLOAT_EQ(bar->getBaseStyleProperties().borderPixelSize, 4.0f);
    EXPECT_TRUE(isColor(tab->getBaseStyleProperties().backgroundColor, 0, 1, 0));
    // .abc is the bar's rule, not the tabs'
    EXPECT_FLOAT_EQ(tab->getBaseStyleProperties().borderPixelSize, 0.0f);
}

TEST_F(TabFixture, ClassPartReachesTheTabLabelText)
{
    load(".abc#tab { text-color: #00ff00; }");
    bar = win->add<TabBar>();
    bar->addClass("abc");
    place(bar, 0, 0, 400, 300);
    TextLabel *label = nullptr;
    bar->addTab(std::make_unique<Frame>(), [&label](Frame &f) { label = f.add<TextLabel>(); });
    win.layout();
    ASSERT_NE(label, nullptr);
    Color4 c = label->getTextStyleProperties().textColor;
    EXPECT_NEAR(c.g, 1.0f, 1e-3f);
    EXPECT_NEAR(c.r, 0.0f, 1e-3f);
}

TEST_F(TabFixture, RemovingTheBarClassLeavesTheTabs)
{
    load(".abc#tab { background-color: #00ff00; }");
    build();
    bar->addClass("abc");
    win.layout();
    ASSERT_TRUE(isColor(tab->getBaseStyleProperties().backgroundColor, 0, 1, 0));

    bar->removeClass("abc");
    win.layout();
    EXPECT_FALSE(isColor(tab->getBaseStyleProperties().backgroundColor, 0, 1, 0));
    EXPECT_TRUE(tab->getInheritedClasses().empty());
}

TEST_F(TabFixture, ClassesSetInLabelSetupSurviveBarChanges)
{
    load(".mine { border-pixel-size: 3px; }");
    build([](Frame &f) { f.addClass("mine"); });
    bar->addClass("abc");
    bar->removeClass("abc");
    win.layout();
    EXPECT_TRUE(tab->hasClass("mine"));
    EXPECT_FLOAT_EQ(tab->getBaseStyleProperties().borderPixelSize, 3.0f);
}

TEST_F(TabFixture, StructurallyNonInteractableTabIsNotDisabled)
{
    load(".abc#tab { background-color: #00ff00; } .abc#tab:disabled { background-color: #0000ff; }");
    build();
    bar->addClass("abc");
    win.layout();
    EXPECT_TRUE(isColor(tab->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}

TEST_F(ThemedFixture, PropagationKeepsAChildsOwnPart)
{
    load("#indicator { background-color: #ff0000; } #header { background-color: #00ff00; }");
    Frame *header = win->add<Frame>();
    Frame *indicator = header->add<Frame>();
    Frame *plain = header->add<Frame>();
    header->bindPart(ComponentPart::HEADER);
    indicator->bindPart(ComponentPart::INDICATOR);
    header->propagateClassesToChildren();
    EXPECT_EQ(indicator->getPart(), ComponentPart::INDICATOR);
    EXPECT_EQ(plain->getPart(), ComponentPart::HEADER);
    EXPECT_TRUE(isColor(indicator->getBaseStyleProperties().backgroundColor, 1, 0, 0));
    EXPECT_TRUE(isColor(plain->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}

// --- radio buttons --------------------------------------------------------------------------

TEST(RadioGroupTest, StartsOnTheFirstValueAndNotifiesChanges)
{
    RadioGroup group;
    EXPECT_EQ(group.value, 0);
    int changes = 0;
    auto conn = group.onChanged.connect([&] { ++changes; });
    group.select(0);
    EXPECT_EQ(changes, 0);
    group.select(2);
    EXPECT_EQ(group.value, 2);
    EXPECT_EQ(changes, 1);
}

TEST_F(ThemedFixture, DefaultThemeGivesSelectedRadioALook)
{
    ASSERT_TRUE(Style::load(std::string(AMETHYST_TEST_ASSETS) + "/theme.ams"));
    BaseStyleProperties idle = Style::instance().getBaseStyle(ComponentType::RADIO_BUTTON);
    BaseStyleProperties active = Style::instance().getBaseStyle(ComponentType::RADIO_BUTTON, {}, GUI_STATE_ACTIVE);
    EXPECT_NE(idle.backgroundColor, active.backgroundColor);
}

// --- collapsible header: inherits like TabBar -------------------------------------------------

namespace {

struct HeaderFixture : ThemedFixture {
    CollapsibleHeader *header = nullptr;
    Frame *bar = nullptr;

    void build()
    {
        header = win->add<CollapsibleHeader>();
        place(header, 0, 0, 300, 200);
        header->header([this](Frame &f) {
            bar = &f;
            f.addClass("mine");
        });
        win.layout();
    }
};

} // namespace

TEST_F(HeaderFixture, ClassPartReachesTheHeader)
{
    load(".abc#header { background-color: #00ff00; }");
    build();
    header->addClass("abc");
    win.layout();
    EXPECT_TRUE(isColor(bar->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}

TEST_F(HeaderFixture, OwnClassesOnHeaderPiecesSurviveARestyle)
{
    load(".mine { border-pixel-size: 3px; }");
    build();
    header->addClass("abc");
    win.layout();
    EXPECT_FLOAT_EQ(bar->getBaseStyleProperties().borderPixelSize, 3.0f);
}

TEST_F(HeaderFixture, RemovingTheHeaderClassLeavesItsPieces)
{
    load(".abc#header { background-color: #00ff00; }");
    build();
    header->addClass("abc");
    win.layout();
    header->removeClass("abc");
    win.layout();
    EXPECT_FALSE(isColor(bar->getBaseStyleProperties().backgroundColor, 0, 1, 0));
}
