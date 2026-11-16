#include "utils/utf8.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace Amethyst;

TEST(Utf8, DecodesValidSequences)
{
    std::string_view text = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"; // a é € 😀
    EXPECT_EQ(Utf8::decode(text, 0).codepoint, 'a');
    EXPECT_EQ(Utf8::decode(text, 1).codepoint, 0xE9u);
    EXPECT_EQ(Utf8::decode(text, 1).bytes, 2u);
    EXPECT_EQ(Utf8::decode(text, 3).codepoint, 0x20ACu);
    EXPECT_EQ(Utf8::decode(text, 3).bytes, 3u);
    EXPECT_EQ(Utf8::decode(text, 6).codepoint, 0x1F600u);
    EXPECT_EQ(Utf8::decode(text, 6).bytes, 4u);
    EXPECT_EQ(Utf8::count(text), 4u);
}

TEST(Utf8, RejectsMalformedAsOneByteReplacement)
{
    // overlong '/', surrogate half, truncated sequence
    for (std::string_view bad : {std::string_view("\xC0\xAF"), std::string_view("\xED\xA0\x80"), std::string_view("\xE2\x82")}) {
        Utf8::Decoded d = Utf8::decode(bad, 0);
        EXPECT_EQ(d.codepoint, Utf8::REPLACEMENT);
        EXPECT_EQ(d.bytes, 1u);
    }
}

TEST(Utf8, EncodeRoundTrips)
{
    for (uint32_t cp : {0x41u, 0xE9u, 0x20ACu, 0x1F600u}) {
        char buf[Utf8::MAX_SEQUENCE];
        size_t n = Utf8::encode(cp, buf);
        Utf8::Decoded d = Utf8::decode(std::string_view(buf, n), 0);
        EXPECT_EQ(d.codepoint, cp);
        EXPECT_EQ(d.bytes, n);
    }
    char buf[Utf8::MAX_SEQUENCE];
    size_t n = Utf8::encode(0xD800u, buf);
    EXPECT_EQ(Utf8::decode(std::string_view(buf, n), 0).codepoint, Utf8::REPLACEMENT);
}

TEST(Utf8, BoundariesStepWholeCodepoints)
{
    std::string_view text = "a\xE2\x82\xAC"
                            "b";
    EXPECT_EQ(Utf8::nextBoundary(text, 0), 1u);
    EXPECT_EQ(Utf8::nextBoundary(text, 1), 4u);
    EXPECT_EQ(Utf8::prevBoundary(text, 4), 1u);
    EXPECT_EQ(Utf8::prevBoundary(text, 1), 0u);
    EXPECT_EQ(Utf8::nextBoundary(text, 5), 5u);
    EXPECT_EQ(Utf8::prevBoundary(text, 0), 0u);
}

TEST(Utf8, AlignSnapsBackInsideValidSequence)
{
    std::string_view text = "a\xE2\x82\xAC"
                            "b";
    EXPECT_EQ(Utf8::alignToBoundary(text, 2), 1u);
    EXPECT_EQ(Utf8::alignToBoundary(text, 3), 1u);
    EXPECT_EQ(Utf8::alignToBoundary(text, 4), 4u);
    EXPECT_EQ(Utf8::alignToBoundary(text, 99), text.size());
}

// regression: a stray continuation byte is a one-byte unit to nextBoundary, so align must
// not pull a caret sitting on it back, or the caret can never step past it
TEST(Utf8, StrayContinuationByteIsItsOwnBoundary)
{
    std::string_view text = "a\xB0"
                            "b";
    EXPECT_EQ(Utf8::nextBoundary(text, 0), 1u);
    EXPECT_EQ(Utf8::alignToBoundary(text, 1), 1u);
    EXPECT_EQ(Utf8::nextBoundary(text, 1), 2u);
    EXPECT_EQ(Utf8::prevBoundary(text, 2), 1u);

    // a truncated sequence's tail is stray too
    std::string_view truncated = "\xE2\x82"
                                 "x";
    EXPECT_EQ(Utf8::alignToBoundary(truncated, 1), 1u);
}

TEST(Utf8, NextThenAlignAlwaysAdvances)
{
    std::string text = "x\xB0\x80\xC3\xA9\xE2\x82\xF0\x9F\x98\x80\xFFz";
    size_t pos = 0;
    size_t steps = 0;
    while (pos < text.size()) {
        size_t next = Utf8::alignToBoundary(text, Utf8::nextBoundary(text, pos));
        ASSERT_GT(next, pos) << "stuck at " << pos;
        pos = next;
        ASSERT_LT(++steps, text.size() + 1);
    }

    // and walking back visits the same boundaries in reverse
    size_t back = text.size();
    while (back > 0) {
        size_t prev = Utf8::prevBoundary(text, back);
        ASSERT_LT(prev, back);
        EXPECT_EQ(Utf8::alignToBoundary(text, prev), prev);
        back = prev;
    }
}
