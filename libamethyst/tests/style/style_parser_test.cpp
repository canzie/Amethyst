#include "modules/style.h"
#include "parsers/am_theme/am_theme_parser.h"

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

using namespace Amethyst;

namespace {

Style parse(const std::string &source)
{
    std::optional<Style> style = AmThemeParser::parseString(source);
    EXPECT_TRUE(style.has_value());
    return style.has_value() ? std::move(*style) : Style();
}

std::vector<StyleKey> tokens(std::initializer_list<std::string_view> names)
{
    std::vector<StyleKey> out;
    for (std::string_view n : names) {
        out.push_back(Style::classToken(n));
    }
    return out;
}

void expectColor(const Color3 &c, float r, float g, float b)
{
    EXPECT_NEAR(c.r, r, 1e-3f);
    EXPECT_NEAR(c.g, g, 1e-3f);
    EXPECT_NEAR(c.b, b, 1e-3f);
}

StylePart inheritedPart(ComponentPart part, ComponentType owner, std::initializer_list<std::string_view> classes)
{
    StylePart p(part, owner);
    p.inheritedClasses = tokens(classes);
    return p;
}

} // namespace

// --- cascade order ---------------------------------------------------------------------------

TEST(StyleCascade, RepeatedSelectorDoesNotJumpPastRulesBetween)
{
    // CSS: the .b block comes after the first .a block, and the second .a block sets no background,
    // so blue must win
    Style s = parse(".a { background-color: #ff0000; } .b { background-color: #0000ff; } .a { border-pixel-size: 1px; }");
    auto classes = tokens({"a", "b"});
    BaseStyleProperties r = s.getBaseStyle(ComponentType::FRAME, classes);
    expectColor(r.backgroundColor, 0, 0, 1);
    EXPECT_FLOAT_EQ(r.borderPixelSize, 1.0f);
}

TEST(StyleCascade, LaterDeclarationWinsAtEqualSpecificity)
{
    Style s = parse(".a { background-color: #ff0000; } .b { background-color: #0000ff; } .a { background-color: #00ff00; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"a", "b"})).backgroundColor, 0, 1, 0);
}

TEST(StyleCascade, ClassOrderOnTheNodeDoesNotMatter)
{
    Style s = parse(".a { background-color: #ff0000; } .b { background-color: #0000ff; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"b", "a"})).backgroundColor, 0, 0, 1);
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"a", "b"})).backgroundColor, 0, 0, 1);
}

// --- specificity ----------------------------------------------------------------------------

TEST(StyleSpecificity, TypeClassBeatsClassRegardlessOfOrder)
{
    Style s = parse("frame.x { background-color: #00ff00; } .x { background-color: #ff0000; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"x"})).backgroundColor, 0, 1, 0);
}

TEST(StyleSpecificity, PseudoStateAddsToClassSpecificity)
{
    Style s = parse(".x:hover { background-color: #00ff00; } frame.x { background-color: #ff0000; }");
    // (0,2,0) beats (0,1,1)
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"x"}), GUI_STATE_HOVERED).backgroundColor, 0, 1, 0);
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"x"})).backgroundColor, 1, 0, 0);
}

TEST(StyleSpecificity, ClassPartBeatsTypePart)
{
    Style s = parse(".x#tab { background-color: #00ffff; } tab-bar#tab { background-color: #0000ff; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"x"}), GUI_STATE_NONE, ComponentPart::TAB).backgroundColor, 0, 1, 1);
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, GUI_STATE_NONE, ComponentPart::TAB).backgroundColor, 0, 0, 1);
}

TEST(StyleSpecificity, DerivedTypeBeatsBaseType)
{
    Style s = parse("text-button.x { background-color: #00ff00; } ui-button.x { background-color: #ff0000; }");
    expectColor(s.getBaseStyle(ComponentType::TEXT_BUTTON, tokens({"x"})).backgroundColor, 0, 1, 0);
}

// --- parts ----------------------------------------------------------------------------------

TEST(StyleParts, TypePartMatchesOnlyThatOwner)
{
    Style s = parse("collapsible-header#header { background-color: #ff0000; }");
    BaseStyleProperties dflt = Style().getBaseStyle(ComponentType::FRAME);
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, 0, StylePart(ComponentPart::HEADER, ComponentType::COLLAPSIBLE_HEADER))
                    .backgroundColor,
                1, 0, 0);
    Color3 tableHeader =
        s.getBaseStyle(ComponentType::FRAME, {}, 0, StylePart(ComponentPart::HEADER, ComponentType::TABLE)).backgroundColor;
    expectColor(tableHeader, dflt.backgroundColor.r, dflt.backgroundColor.g, dflt.backgroundColor.b);
}

TEST(StyleParts, BarePartMatchesEveryOwner)
{
    Style s = parse("#header { background-color: #ff0000; } table#header { background-color: #00ff00; }");
    expectColor(
        s.getBaseStyle(ComponentType::FRAME, {}, 0, StylePart(ComponentPart::HEADER, ComponentType::TREE_VIEW)).backgroundColor, 1,
        0, 0);
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, 0, StylePart(ComponentPart::HEADER, ComponentType::TABLE)).backgroundColor,
                0, 1, 0);
}

TEST(StyleParts, TypeClassPart)
{
    Style s = parse("tab-bar.x#tab { background-color: #00ff00; } .x#tab { background-color: #ff0000; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, 0, inheritedPart(ComponentPart::TAB, ComponentType::TAB_BAR, {"x"}))
                    .backgroundColor,
                0, 1, 0);
}

TEST(StyleParts, InheritedClassesOnlyMatchPartRules)
{
    Style s = parse(".abc { background-color: #ff0000; border-pixel-size: 5px; } .abc#tab { background-color: #00ff00; }");
    StylePart tab = inheritedPart(ComponentPart::TAB, ComponentType::TAB_BAR, {"abc"});
    BaseStyleProperties r = s.getBaseStyle(ComponentType::FRAME, {}, 0, tab);
    expectColor(r.backgroundColor, 0, 1, 0);
    // the bar-level .abc declaration must not leak onto the tab
    EXPECT_FLOAT_EQ(r.borderPixelSize, 0.0f);

    BaseStyleProperties bar = s.getBaseStyle(ComponentType::TAB_BAR, tokens({"abc"}));
    expectColor(bar.backgroundColor, 1, 0, 0);
    EXPECT_FLOAT_EQ(bar.borderPixelSize, 5.0f);
}

TEST(StyleParts, CacheSeparatesInheritedClassesAndOwners)
{
    Style s = parse(".a#header { background-color: #ff0000; } table#header { background-color: #00ff00; }");
    StylePart withA = inheritedPart(ComponentPart::HEADER, ComponentType::TREE_VIEW, {"a"});
    StylePart without(ComponentPart::HEADER, ComponentType::TREE_VIEW);
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, 0, withA).backgroundColor, 1, 0, 0);
    EXPECT_NE(s.getBaseStyle(ComponentType::FRAME, {}, 0, without).backgroundColor, Color3(1, 0, 0));
    expectColor(s.getBaseStyle(ComponentType::FRAME, {}, 0, StylePart(ComponentPart::HEADER, ComponentType::TABLE)).backgroundColor,
                0, 1, 0);
}

// --- pseudo-states on bare types ------------------------------------------------------------

TEST(StyleTypeState, BareTypePseudoState)
{
    Style s = parse("radio-button { background-color: #333333; } radio-button:active { background-color: #4772b3; }");
    expectColor(s.getBaseStyle(ComponentType::RADIO_BUTTON).backgroundColor, 0.2f, 0.2f, 0.2f);
    expectColor(s.getBaseStyle(ComponentType::RADIO_BUTTON, {}, GUI_STATE_ACTIVE).backgroundColor, 0x47 / 255.0f, 0x72 / 255.0f,
                0xb3 / 255.0f);
    // type:state is (0,1,1) and outranks a bare class at (0,1,0)
    Style t = parse("radio-button:active { background-color: #ff0000; } .x { background-color: #00ff00; }");
    expectColor(t.getBaseStyle(ComponentType::RADIO_BUTTON, tokens({"x"}), GUI_STATE_ACTIVE).backgroundColor, 1, 0, 0);
}

TEST(StyleTypeState, BaseTypeStateAppliesToDerived)
{
    Style s = parse("ui-button:hover { background-color: #ff0000; }");
    expectColor(s.getBaseStyle(ComponentType::TEXT_BUTTON, {}, GUI_STATE_HOVERED).backgroundColor, 1, 0, 0);
}

// --- padding --------------------------------------------------------------------------------

TEST(StylePadding, ShorthandResolves)
{
    Style s = parse("frame { padding: 1px 2px 3px 4px; } .p { padding-left: 10% + 2px; }");
    UDim4 p = s.getPadding(ComponentType::FRAME);
    EXPECT_FLOAT_EQ(p.top.offset, 1.0f);
    EXPECT_FLOAT_EQ(p.right.offset, 2.0f);
    EXPECT_FLOAT_EQ(p.bottom.offset, 3.0f);
    EXPECT_FLOAT_EQ(p.left.offset, 4.0f);
    UDim4 q = s.getPadding(ComponentType::FRAME, tokens({"p"}));
    EXPECT_FLOAT_EQ(q.left.scale, 0.1f);
    EXPECT_FLOAT_EQ(q.left.offset, 2.0f);
}

// --- value parsing robustness ---------------------------------------------------------------

struct ColorCase {
    const char *value;
    bool valid;
    float r, g, b, a;
};

class StyleColorParse : public ::testing::TestWithParam<ColorCase> {};

TEST_P(StyleColorParse, TextColor)
{
    const ColorCase &c = GetParam();
    // a valid value replaces the type-level default; an invalid one is dropped and leaves it alone
    Style s = parse(std::string("frame { text-color: #102030; } .t { text-color: ") + c.value + "; }");
    Color4 got = s.getTextStyle(ComponentType::FRAME, tokens({"t"})).textColor;
    if (c.valid) {
        EXPECT_NEAR(got.r, c.r, 1e-3f) << c.value;
        EXPECT_NEAR(got.g, c.g, 1e-3f) << c.value;
        EXPECT_NEAR(got.b, c.b, 1e-3f) << c.value;
        EXPECT_NEAR(got.a, c.a, 1e-3f) << c.value;
    } else {
        EXPECT_NEAR(got.r, 0x10 / 255.0f, 1e-3f) << c.value;
        EXPECT_NEAR(got.b, 0x30 / 255.0f, 1e-3f) << c.value;
        EXPECT_NEAR(got.a, 1.0f, 1e-3f) << c.value;
    }
}

INSTANTIATE_TEST_SUITE_P(Values, StyleColorParse,
                         ::testing::Values(ColorCase{"#f00", true, 1, 0, 0, 1}, ColorCase{"#f008", true, 1, 0, 0, 0x88 / 255.0f},
                                           ColorCase{"#00ff00", true, 0, 1, 0, 1},
                                           ColorCase{"#0000ff80", true, 0, 0, 1, 0x80 / 255.0f},
                                           ColorCase{"rgba(0, 0, 0, 0.5)", true, 0, 0, 0, 0.5f},
                                           ColorCase{"rgba(50, 50, 50, 255)", true, 50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1},
                                           ColorCase{"rgba(0, 0, 0, 50%)", true, 0, 0, 0, 0.5f},
                                           ColorCase{"rgb(300, -5, 0)", true, 1, 0, 0, 1},
                                           ColorCase{"rgb(255, 255, 255)", true, 1, 1, 1, 1}, ColorCase{"#zzz", false, 0, 0, 0, 0},
                                           ColorCase{"#12345", false, 0, 0, 0, 0}, ColorCase{"#", false, 0, 0, 0, 0},
                                           ColorCase{"rgb(1, 2)", false, 0, 0, 0, 0}, ColorCase{"rgb(1, 2, x)", false, 0, 0, 0, 0},
                                           ColorCase{"rgba(1, 2, 3, 0.5", false, 0, 0, 0, 0}, ColorCase{"red", false, 0, 0, 0, 0}));

TEST(StyleValueParse, InvalidNumbersAreDropped)
{
    Style s = parse("frame { border-pixel-size: 3px; font-size: 12px; } .t { border-pixel-size: 1x2px; font-size: abc; }");
    EXPECT_FLOAT_EQ(s.getBaseStyle(ComponentType::FRAME, tokens({"t"})).borderPixelSize, 3.0f);
    EXPECT_FLOAT_EQ(s.getTextStyle(ComponentType::FRAME, tokens({"t"})).fontSize, 12.0f);
}

TEST(StyleValueParse, NegativeCornerRadiusClampsToZero)
{
    Style s = parse(".t { corner-radius: -4px; } .u { corner-radius: 1e30px; }");
    EXPECT_EQ(s.getBaseStyle(ComponentType::FRAME, tokens({"t"})).cornerRadius, uvec4(0u));
    EXPECT_EQ(s.getBaseStyle(ComponentType::FRAME, tokens({"u"})).cornerRadius, uvec4(65535u));
}

TEST(StyleValueParse, UnknownKeywordDropped)
{
    Style s = parse("frame { text-x-alignment: center; } .t { text-x-alignment: middle; }");
    EXPECT_EQ(s.getTextStyle(ComponentType::FRAME, tokens({"t"})).textXAlignment, TextXAlignment::CENTER);
}

TEST(StyleSyntax, SelectorWithWhitespaceIsRejected)
{
    // ".foo :hover" used to intern a class called "foo "
    Style s = parse(".foo :hover { background-color: #ff0000; } .foo { background-color: #00ff00; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"foo"}), GUI_STATE_HOVERED).backgroundColor, 0, 1, 0);
}

TEST(StyleSyntax, UnknownAtRuleDoesNotSwallowNextRule)
{
    Style s =
        parse("@import \"other.ams\";\n.a { background-color: #ff0000; }\n@media screen { .b { background-color: #00ff00; } }\n"
              ".c { background-color: #0000ff; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"a"})).backgroundColor, 1, 0, 0);
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"c"})).backgroundColor, 0, 0, 1);
    EXPECT_NE(s.getBaseStyle(ComponentType::FRAME, tokens({"b"})).backgroundColor, Color3(0, 1, 0));
}

TEST(StyleSyntax, DoubleSlashInsideQuotesIsNotAComment)
{
    Style s = parse(".a { font-family: \"fonts//x\"; } // trailing comment\n.b { font-family: 'y'; }");
    EXPECT_EQ(s.getTextStyle(ComponentType::FRAME, tokens({"a"})).fontFamily, "fonts//x");
    EXPECT_EQ(s.getTextStyle(ComponentType::FRAME, tokens({"b"})).fontFamily, "y");
}

TEST(StyleSyntax, CommentsAndVariables)
{
    Style s = parse(
        "/* header */\n@property accent #00ff00\n.a { background-color: @accent; /* inline */ }\n.b { background-color: @nope; }");
    expectColor(s.getBaseStyle(ComponentType::FRAME, tokens({"a"})).backgroundColor, 0, 1, 0);
}

TEST(StyleSyntax, MalformedSelectorsAreSkipped)
{
    Style s = parse(
        "frame.a.b { background-color: #ff0000; } .a#tab.b { background-color: #ff0000; } bogus { background-color: #ff0000; }"
        ".a:hover:pressed { background-color: #ff0000; } .a:wiggle { background-color: #ff0000; } .a {}");
    EXPECT_NE(s.getBaseStyle(ComponentType::FRAME, tokens({"a", "b"}), GUI_STATE_HOVERED | GUI_STATE_PRESSED).backgroundColor,
              Color3(1, 0, 0));
}

TEST(StyleSyntax, TruncatedInputDoesNotCrash)
{
    const char *inputs[] = {
        "",
        "{",
        "}",
        ".a {",
        ".a { background-color: ",
        "@",
        "@property",
        "/* unterminated",
        "\"",
        ".a { b: \"c; }",
        "frame { padding: 1px 2px 3px; }",
        ",, { background-color: #fff; }",
        "#tab { x }",
        ".a { : ; ;; }",
        "@media { { } ",
        ".a { background-color: rgb(",
        ".a{corner-radius:}",
    };
    for (const char *in : inputs) {
        std::optional<Style> s = AmThemeParser::parseString(in);
        EXPECT_TRUE(s.has_value()) << in;
    }
}

TEST(StyleSyntax, RandomInputDoesNotCrash)
{
    const std::string alphabet = ".#:{};,@/*\"'()%+- \n\tabcdefrgxpz0123456789";
    std::mt19937 rng(1234);
    for (int i = 0; i < 2000; ++i) {
        std::string src;
        size_t len = rng() % 80;
        for (size_t j = 0; j < len; ++j) {
            src.push_back(alphabet[rng() % alphabet.size()]);
        }
        std::optional<Style> s = AmThemeParser::parseString(src);
        ASSERT_TRUE(s.has_value());
        s->getBaseStyle(ComponentType::FRAME, tokens({"a"}), GUI_STATE_HOVERED, ComponentPart::TAB);
    }
}

TEST(StyleSyntax, BundledThemesParseWithoutUnknownProperties)
{
    for (const char *name : {"/theme.ams", "/theme_selector_demo.ams"}) {
        std::optional<Style> s = AmThemeParser::parseFile(std::string(AMETHYST_TEST_ASSETS) + name);
        ASSERT_TRUE(s.has_value()) << name;
    }
    std::optional<Style> s = AmThemeParser::parseFile(std::string(AMETHYST_TEST_ASSETS) + "/theme.ams");
    ASSERT_TRUE(s.has_value());
    // the debug magenta table background is gone
    EXPECT_NE(s->getBaseStyle(ComponentType::TABLE).backgroundColor, Color3(1, 0, 1));
}
