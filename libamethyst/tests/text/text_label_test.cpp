#include "text/text_test_env.h"

#include "components/text_button.h"
#include "components/text_label.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct LabelFixture : ::testing::Test {
    TextEnv env;
    TestWindow win;

    bool hasGlyphs() const
    {
        GlyphBuffer *buffer = env.geometry->getGlyphBuffer();
        return buffer != nullptr && buffer->hasLiveSlices();
    }
};

} // namespace

// regression: setText("") returned before touching the old glyphs, so the old text stayed up
TEST_F(LabelFixture, ClearingLabelTextReleasesItsGlyphs)
{
    TextLabel *label = win->add<TextLabel>();
    place(label, 0, 0, 200, 30);
    win.layout();

    label->setText("hello");
    label->draw(env.ctx);
    ASSERT_TRUE(hasGlyphs());
    EXPECT_GT(label->getTextSize().x, 0.0f);

    label->setText("");
    label->draw(env.ctx);
    EXPECT_FALSE(hasGlyphs());
    EXPECT_EQ(label->getTextSize().x, 0.0f);

    label->setText("again");
    label->draw(env.ctx);
    EXPECT_TRUE(hasGlyphs());
}

TEST_F(LabelFixture, ClearingButtonTextReleasesItsGlyphs)
{
    TextButton *button = win->add<TextButton>();
    place(button, 0, 0, 200, 30);
    win.layout();

    button->setText("press");
    button->draw(env.ctx);
    ASSERT_TRUE(hasGlyphs());

    button->setText("");
    button->draw(env.ctx);
    EXPECT_FALSE(hasGlyphs());
    EXPECT_EQ(button->getTextSize().x, 0.0f);
}

// the measured block uses the label's own line height, the same one its layout uses
TEST_F(LabelFixture, TextSizeHonoursLineHeight)
{
    TextLabel *label = win->add<TextLabel>();
    place(label, 0, 0, 200, 80);
    win.layout();

    label->setTextStyleProperties({.fontSize = 16.0f, .lineHeight = 2.0f});
    label->setText("a\nb");
    label->draw(env.ctx);

    float line = env.processor.getMetricsAtlas(FontId{}, 16).lineHeight;
    EXPECT_FLOAT_EQ(label->getTextSize().y, 2.0f * line * 2.0f);
}

TEST_F(LabelFixture, FontFamilyChangeRemeasures)
{
    TextLabel *label = win->add<TextLabel>();
    place(label, 0, 0, 300, 30);
    win.layout();

    label->setText("The quick brown fox");
    label->draw(env.ctx);
    float roboto = label->getTextSize().x;

    label->setTextStyleProperties({.fontFamily = "Open Sans"});
    label->draw(env.ctx);
    EXPECT_NE(label->getTextSize().x, roboto);
}
