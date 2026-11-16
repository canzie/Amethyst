#include "text/text_test_env.h"

#include "components/number_input.h"
#include "components/text_input.h"

#include <gtest/gtest.h>

#include <clocale>
#include <cstdint>
#include <limits>
#include <string>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct TextFieldFixture : ::testing::Test {
    TextEnv env;
    TestWindow win;
    TextInput *field = nullptr;
    std::string clipboard;

    void SetUp() override
    {
        field = win->add<TextInput>();
        place(field, 10, 10, 300, 30);
        win.layout();
        InputInterface::onSetClipboardText = [this](const std::string &text) { clipboard = text; };
        InputInterface::onGetClipboardText = [this] { return clipboard; };
    }

    void TearDown() override
    {
        InputInterface::onSetClipboardText = nullptr;
        InputInterface::onGetClipboardText = nullptr;
    }

    void frame()
    {
        win->tick(0.016f);
        field->draw(env.ctx);
    }

    float textLeft() const { return field->absoluteContentPosition.x; }
    float midY() const { return field->absoluteContentPosition.y + field->absoluteContentSize.y * 0.5f; }
};

} // namespace

TEST_F(TextFieldFixture, TypesAndDeletes)
{
    field->focus();
    typeText("hello");
    pressKey(KEY_BACKSPACE);
    frame();
    EXPECT_EQ(field->getText(), "hell");
}

// regression: keys were drained before characters, so Home typed between letters in one
// frame ran before any of them
TEST_F(TextFieldFixture, KeysAndCharactersKeepTheirOrder)
{
    field->focus();
    typeText("bc");
    pressKey(KEY_HOME);
    typeText("a");
    frame();
    EXPECT_EQ(field->getText(), "abc");
}

TEST(InputQueue, LegacyPollsStillWorkIndependently)
{
    InputInterface::clearKeyEvents();
    InputInterface::onChar('x');
    InputInterface::onKey(KEY_ENTER, 0, KEY_ACTION_PRESS, 0);

    KeyEvent key{};
    ASSERT_TRUE(InputInterface::pollKeyEvent(key));
    EXPECT_EQ(key.key, KEY_ENTER);
    uint32_t cp = 0;
    ASSERT_TRUE(InputInterface::pollCharEvent(cp));
    EXPECT_EQ(cp, static_cast<uint32_t>('x'));
    TextInputEvent event;
    EXPECT_FALSE(InputInterface::pollTextInputEvent(event));
}

TEST(InputQueue, MergedPollFollowsArrivalOrder)
{
    InputInterface::clearKeyEvents();
    InputInterface::onChar('a');
    InputInterface::onKey(KEY_ENTER, 0, KEY_ACTION_PRESS, 0);
    InputInterface::onChar('b');

    TextInputEvent event;
    ASSERT_TRUE(InputInterface::pollTextInputEvent(event));
    EXPECT_EQ(event.kind, TextInputEvent::Kind::CHAR);
    EXPECT_EQ(event.codepoint, static_cast<uint32_t>('a'));
    ASSERT_TRUE(InputInterface::pollTextInputEvent(event));
    EXPECT_EQ(event.kind, TextInputEvent::Kind::KEY);
    EXPECT_EQ(event.key.key, KEY_ENTER);
    ASSERT_TRUE(InputInterface::pollTextInputEvent(event));
    EXPECT_EQ(event.codepoint, static_cast<uint32_t>('b'));
    EXPECT_FALSE(InputInterface::pollTextInputEvent(event));
}

TEST(InputQueue, FullQueueDropsNewestInsteadOfEverything)
{
    InputInterface::clearKeyEvents();
    for (int i = 0; i < 1000; i++) {
        InputInterface::onChar('q');
    }
    uint32_t cp = 0;
    size_t drained = 0;
    while (InputInterface::pollCharEvent(cp)) {
        ++drained;
    }
    EXPECT_GT(drained, 200u);
}

// regression: a click followed by a tiny drag left an empty anchor that made the next
// Backspace "delete the selection" (nothing) instead of a character
TEST_F(TextFieldFixture, BackspaceAfterClickDeletesACharacter)
{
    field->setText("abc");
    field->draw(env.ctx);

    int32_t x = static_cast<int32_t>(textLeft() + 250.0f); // well right of the text: caret at end
    int32_t y = static_cast<int32_t>(midY());
    win.press(x, y);
    win.move(x + 1, y);
    win.release(x + 1, y);
    ASSERT_TRUE(field->isFocused());
    EXPECT_FALSE(field->hasSelection());

    pressKey(KEY_BACKSPACE);
    frame();
    EXPECT_EQ(field->getText(), "ab");

    pressKey(KEY_DELETE);
    pressKey(KEY_HOME);
    pressKey(KEY_DELETE);
    frame();
    EXPECT_EQ(field->getText(), "b");
}

TEST_F(TextFieldFixture, CopyWithEmptyAnchorDoesNotClobberClipboard)
{
    field->setText("abc");
    field->draw(env.ctx);
    clipboard = "keep";

    int32_t y = static_cast<int32_t>(midY());
    win.press(static_cast<int32_t>(textLeft() + 250.0f), y);
    win.move(static_cast<int32_t>(textLeft() + 251.0f), y);
    win.release(static_cast<int32_t>(textLeft() + 251.0f), y);
    pressKey(KEY_C, MOD_CONTROL);
    pressKey(KEY_X, MOD_CONTROL);
    frame();
    EXPECT_EQ(clipboard, "keep");
    EXPECT_EQ(field->getText(), "abc");
}

// regression: a single-line field kept pasted line breaks, which the shaper broke onto a
// second row the caret knew nothing about
TEST_F(TextFieldFixture, SingleLineReplacesLineBreaks)
{
    field->setText("a\nb\r\nc\rd");
    EXPECT_EQ(field->getText(), "a b c d");

    field->clearText();
    field->focus();
    clipboard = "x\r\ny";
    pressKey(KEY_V, MOD_CONTROL);
    InputInterface::onChar('\n');
    frame();
    EXPECT_EQ(field->getText(), "x y");
}

// regression: caret offsets summed .notdef advances for tabs while the glyphs sat on tab
// stops, so clicking after a tab put the caret in the wrong place
TEST_F(TextFieldFixture, CaretPositionsFollowTabStops)
{
    field->setText("\t\tx");
    field->draw(env.ctx);

    TextRowLayoutParams params;
    params.fontSize = field->getTextInputProperties().text.fontSize;
    TextRowLayout row;
    env.processor.layoutTextRow("\t\tx", params, row);
    float mid = (row.columnX[2] + row.columnX[3]) * 0.5f;

    int32_t y = static_cast<int32_t>(midY());
    win.click(static_cast<int32_t>(textLeft() + mid - 2.0f), y);
    EXPECT_EQ(field->getCursorPosition(), 2u);
    win.click(static_cast<int32_t>(textLeft() + mid + 2.0f), y);
    EXPECT_EQ(field->getCursorPosition(), 3u);
    win.click(static_cast<int32_t>(textLeft() + row.columnX[1] + 1.0f), y);
    EXPECT_EQ(field->getCursorPosition(), 1u);
}

// regression: setText clamped the caret by bytes and could leave it inside a sequence
TEST_F(TextFieldFixture, SetTextKeepsCaretOnABoundary)
{
    field->setText("abc");
    field->focus();
    pressKey(KEY_HOME);
    pressKey(KEY_RIGHT);
    frame();
    ASSERT_EQ(field->getCursorPosition(), 1u);

    field->setText("\xE2\x82\xACx");
    EXPECT_EQ(field->getCursorPosition(), 0u);
}

// regression: read-only swallowed every key, including navigation and copy
TEST_F(TextFieldFixture, ReadOnlyStillSelectsAndCopies)
{
    field->setText("abc");
    field->setTextInputProperties({.readOnly = true});
    field->focus();
    pressKey(KEY_BACKSPACE);
    pressKey(KEY_END);
    pressKey(KEY_LEFT, MOD_SHIFT);
    pressKey(KEY_C, MOD_CONTROL);
    pressKey(KEY_X, MOD_CONTROL);
    typeText("zz");
    frame();
    EXPECT_EQ(field->getText(), "abc");
    EXPECT_EQ(clipboard, "c");

    pressKey(KEY_A, MOD_CONTROL);
    pressKey(KEY_C, MOD_CONTROL);
    frame();
    EXPECT_EQ(clipboard, "abc");
}

TEST_F(TextFieldFixture, MaxLengthCountsCodepointsAndTruncatesPaste)
{
    field->setTextInputProperties({.maxLength = 3});
    field->focus();
    clipboard = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
    pressKey(KEY_V, MOD_CONTROL);
    typeText("x");
    frame();
    EXPECT_EQ(field->getText(), "\xC3\xA9\xC3\xA9\xC3\xA9");

    // replacing a selection frees its room first
    pressKey(KEY_LEFT, MOD_SHIFT);
    typeText("yz");
    frame();
    EXPECT_EQ(field->getText(), "\xC3\xA9\xC3\xA9y");
}

TEST_F(TextFieldFixture, ReplacingSelectionFiresOneChange)
{
    int changes = 0;
    field->setText("abc");
    field->onTextChanged = [&](const std::string &) { ++changes; };
    field->focus();
    pressKey(KEY_A, MOD_CONTROL);
    typeText("q");
    frame();
    EXPECT_EQ(field->getText(), "q");
    EXPECT_EQ(changes, 1);
}

// regression: focusing a second input left the first focused, both draining one key queue
TEST_F(TextFieldFixture, FocusingAnotherInputUnfocusesTheFirst)
{
    TextInput *other = win->add<TextInput>();
    place(other, 10, 60, 300, 30);
    win.layout();

    field->focus();
    other->focus();
    EXPECT_FALSE(field->isFocused());
    EXPECT_TRUE(other->isFocused());
    EXPECT_EQ(UIInput::getFocusedInput(), other);

    typeText("k");
    win->tick(0.016f);
    EXPECT_EQ(field->getText(), "");
    EXPECT_EQ(other->getText(), "k");

    other->reparent(nullptr);
    EXPECT_EQ(UIInput::getFocusedInput(), nullptr);
}

TEST_F(TextFieldFixture, EnterHandlerThatDestroysFieldStopsTheDrain)
{
    field->onEnterPressed = [this] { field->reparent(nullptr); };
    field->focus();
    pressKey(KEY_ENTER);
    typeText("x");
    win->tick(0.016f);
    InputInterface::clearKeyEvents();
    SUCCEED();
}

namespace {

struct NumberFixture : ::testing::Test {
    TestWindow win;
    NumberInput *number = nullptr;

    void SetUp() override
    {
        number = win->add<NumberInput>();
        place(number, 0, 0, 100, 30);
        win.layout();
    }
};

} // namespace

TEST_F(NumberFixture, RejectsNonNumericTyping)
{
    number->focus();
    typeText("-1a2.5.3-");
    win->tick(0.016f);
    EXPECT_EQ(number->getText(), "-12.53");
}

TEST_F(NumberFixture, ParsesPartialAndEmptyBuffers)
{
    for (const char *text : {"", "-", ".", "-."}) {
        number->setText(text);
        EXPECT_EQ(number->asDouble(), 0.0) << text;
        EXPECT_EQ(number->asInt64(), 0) << text;
    }
    number->setText("1.");
    EXPECT_EQ(number->asDouble(), 1.0);
    number->setText("1.50 m"); // formatted display text set programmatically
    EXPECT_EQ(number->asDouble(), 1.5);
}

TEST_F(NumberFixture, Int64TruncatesTowardZero)
{
    number->setText("1.9");
    EXPECT_EQ(number->asInt64(), 1);
    number->setText("-1.9");
    EXPECT_EQ(number->asInt64(), -1);
    number->setText("42");
    EXPECT_EQ(number->asInt64(), 42);
}

// regression: strtoll overflow was unchecked
TEST_F(NumberFixture, Int64ClampsOnOverflow)
{
    number->setText("99999999999999999999");
    EXPECT_EQ(number->asInt64(), std::numeric_limits<int64_t>::max());
    number->setText("-99999999999999999999");
    EXPECT_EQ(number->asInt64(), std::numeric_limits<int64_t>::min());
}

TEST_F(NumberFixture, DoubleClampsOnOverflow)
{
    std::string huge = "1" + std::string(400, '0');
    number->setText(huge);
    EXPECT_EQ(number->asDouble(), std::numeric_limits<double>::max());
    number->setText("-" + huge);
    EXPECT_EQ(number->asDouble(), -std::numeric_limits<double>::max());
}

// regression: strtod honours LC_NUMERIC, so a comma-decimal locale read "1.5" as 1
TEST_F(NumberFixture, DoubleIgnoresTheCLocale)
{
    const char *previous = std::setlocale(LC_NUMERIC, nullptr);
    std::string saved = previous != nullptr ? previous : "C";
    const char *candidates[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "nl_BE.UTF-8"};
    bool switched = false;
    for (const char *name : candidates) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            switched = true;
            break;
        }
    }

    number->setText("1.5");
    double value = number->asDouble();
    std::setlocale(LC_NUMERIC, saved.c_str());

    if (!switched) {
        GTEST_SKIP() << "no comma-decimal locale installed";
    }
    EXPECT_EQ(value, 1.5);
}
