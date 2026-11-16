#include "text/text_test_env.h"

#include "components/text_area.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct TextAreaFixture : ::testing::Test {
    TextEnv env;
    TestWindow win;
    LineSource source{{"abc", "abcdefgh", ""}};
    TextArea *area = nullptr;

    void SetUp() override
    {
        area = win->add<TextArea>();
        place(area, 0, 0, 400, 200);
        win.layout();
        area->setSource(&source);
        area->draw(env.ctx);
        area->focus();
    }

    void frame()
    {
        win->tick(0.016f);
        area->draw(env.ctx);
    }
};

} // namespace

TEST_F(TextAreaFixture, TypesAtCursor)
{
    area->setCursorPosition({0, 3});
    typeText("xy");
    frame();
    EXPECT_EQ(source.line(0), "abcxy");
    EXPECT_EQ(area->getCursorPosition(), (TextPosition{0, 5}));
}

// regression: keys were drained before characters, so 'a', Enter, 'b' in one frame became "\nab"
TEST_F(TextAreaFixture, KeysAndCharactersKeepTheirOrder)
{
    area->setCursorPosition({2, 0});
    typeText("a");
    pressKey(KEY_ENTER);
    typeText("b");
    frame();
    EXPECT_EQ(source.line(2), "a");
    EXPECT_EQ(source.line(3), "b");
}

// regression: after an edit rowForLine still answered from the pre-edit rows, so the goal x
// for a following Down was measured on the old, shorter line
TEST_F(TextAreaFixture, CursorXAfterEditUsesTheEditedLine)
{
    area->setCursorPosition({0, 3});
    typeText("de");
    pressKey(KEY_DOWN);
    win->tick(0.016f); // no draw between the edit and the Down
    EXPECT_EQ(area->getCursorPosition(), (TextPosition{1, 5}));
}

TEST_F(TextAreaFixture, BackspaceJoinsLines)
{
    area->setCursorPosition({1, 0});
    pressKey(KEY_BACKSPACE);
    frame();
    EXPECT_EQ(source.line(0), "abcabcdefgh");
    EXPECT_EQ(area->getCursorPosition(), (TextPosition{0, 3}));
}

TEST_F(TextAreaFixture, BackspaceRemovesWholeCodepoint)
{
    area->setCursorPosition({2, 0});
    InputInterface::onChar(0x20ACu);
    frame();
    EXPECT_EQ(source.line(2), "\xE2\x82\xAC");
    pressKey(KEY_BACKSPACE);
    frame();
    EXPECT_EQ(source.line(2), "");
}

TEST_F(TextAreaFixture, SelectAllAndTypeReplaces)
{
    pressKey(KEY_A, MOD_CONTROL);
    typeText("z");
    frame();
    EXPECT_EQ(source.lineCount(), 1u);
    EXPECT_EQ(source.line(0), "z");
}

TEST_F(TextAreaFixture, ShiftSelectionAndCopy)
{
    std::string clipboard;
    InputInterface::onSetClipboardText = [&](const std::string &text) { clipboard = text; };

    area->setCursorPosition({0, 1});
    pressKey(KEY_DOWN, MOD_SHIFT);
    pressKey(KEY_C, MOD_CONTROL);
    frame();
    EXPECT_TRUE(area->hasSelection());
    EXPECT_EQ(clipboard, "bc\na");

    InputInterface::onSetClipboardText = nullptr;
}

TEST_F(TextAreaFixture, PasteStripsCarriageReturns)
{
    InputInterface::onGetClipboardText = [] { return std::string("1\r\n2"); };
    area->setCursorPosition({2, 0});
    pressKey(KEY_V, MOD_CONTROL);
    frame();
    EXPECT_EQ(source.line(2), "1");
    EXPECT_EQ(source.line(3), "2");
    EXPECT_EQ(area->getCursorPosition(), (TextPosition{3, 1}));
    InputInterface::onGetClipboardText = nullptr;
}

TEST_F(TextAreaFixture, ExternalEditClampsCursor)
{
    area->setCursorPosition({1, 8});
    source.replace(TextRange{{1, 2}, {1, 8}}, {});
    area->draw(env.ctx);
    EXPECT_EQ(area->getCursorPosition(), (TextPosition{1, 2}));
}

TEST_F(TextAreaFixture, ReadOnlyIgnoresEdits)
{
    area->setTextAreaProperties({.readOnly = true});
    area->setCursorPosition({0, 0});
    typeText("q");
    pressKey(KEY_DELETE);
    frame();
    EXPECT_EQ(source.line(0), "abc");
}
