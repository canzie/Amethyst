#include "modules/svg_atlas.h"

#include <gtest/gtest.h>

#include <string>

using namespace Amethyst;

static const std::string SQUARE =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect width="10" height="10" fill="#ff0000"/></svg>)";
static const std::string CIRCLE =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><circle cx="5" cy="5" r="5" fill="#00ff00"/></svg>)";

TEST(SvgAtlas, EntriesAreSeparatedByATransparentGutter)
{
    SvgAtlas atlas;
    const SvgEntry *a = atlas.loadFromData(SQUARE, 16, 16);
    ASSERT_NE(a, nullptr);
    SvgEntry first = *a;
    const SvgEntry *b = atlas.loadFromData(CIRCLE, 16, 16);
    ASSERT_NE(b, nullptr);
    SvgEntry second = *b;

    EXPECT_GE(first.atlasX, SvgAtlas::GUTTER);
    EXPECT_GE(first.atlasY, SvgAtlas::GUTTER);
    // packed side by side in the same row: the gap must hold both entries' gutters
    ASSERT_EQ(first.atlasY, second.atlasY);
    EXPECT_GE(second.atlasX, first.atlasX + first.width + 2 * SvgAtlas::GUTTER);

    // the column right of the opaque square stays transparent, so bilinear taps at its edge
    // never pick up the neighbour
    const uint8_t *pixels = atlas.getPixels();
    uint32_t gutterX = first.atlasX + first.width;
    uint32_t row = first.atlasY + first.height / 2;
    EXPECT_EQ(pixels[(row * atlas.getWidth() + gutterX) * 4 + 3], 0);
    EXPECT_GT(pixels[(row * atlas.getWidth() + gutterX - 1) * 4 + 3], 0);
}

TEST(SvgAtlas, SameDataAndSizeIsCached)
{
    SvgAtlas atlas;
    const SvgEntry *a = atlas.loadFromData(SQUARE, 8, 8);
    const SvgEntry *b = atlas.loadFromData(SQUARE, 8, 8);
    EXPECT_EQ(a, b);
    EXPECT_TRUE(atlas.isDirty());
}
