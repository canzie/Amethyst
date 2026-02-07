#include "parsers/aml/aml_parser.h"
#include "parsers/aml/aml_tokenizer.h"

#include <cstdio>
#include <string>

static int g_passed = 0;
static int g_failed = 0;
static const char *g_currentSuite = nullptr;

#define TEST_SUITE(name) g_currentSuite = name

#define ASSERT_TRUE(expr)                                                                         \
    do {                                                                                          \
        if (!(expr)) {                                                                            \
            std::printf("  FAIL [%s] %s:%d: %s\n", g_currentSuite, __FILE__, __LINE__, #expr);   \
            g_failed++;                                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT_TRUE((a) != (b))

#define RUN_TEST(fn)                                                                              \
    do {                                                                                          \
        fn();                                                                                     \
        if (g_failed == beforeFail) {                                                             \
            g_passed++;                                                                           \
            std::printf("  PASS %s\n", #fn);                                                     \
        }                                                                                        \
    } while (0)

using namespace Amethyst;

static AmlParseResult parseSource(const std::string &src)
{
    AmlTokenizer tokenizer;
    auto tokResult = tokenizer.tokenize(src);
    if (!tokResult.ok()) {
        AmlParseResult r;
        r.error = tokResult.error;
        r.errorLine = tokResult.errorLine;
        r.errorColumn = tokResult.errorColumn;
        return r;
    }
    AmlParser parser;
    return parser.parse(tokResult.tokens);
}

// --- Tokenizer suite ---

static void testTokenizeSimpleTag()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize(R"(<Frame />)");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.tokens.size(), 4u); // < IDENT /> EOF
    ASSERT_EQ(r.tokens[0].type, AmlTokenType::TAG_OPEN);
    ASSERT_EQ(r.tokens[1].type, AmlTokenType::IDENTIFIER);
    ASSERT_EQ(r.tokens[1].text, "Frame");
    ASSERT_EQ(r.tokens[2].type, AmlTokenType::SLASH_TAG_CLOSE);
    ASSERT_EQ(r.tokens[3].type, AmlTokenType::END_OF_FILE);
}

static void testTokenizeTypedValues()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize(R"(<E x=42 y=-3.14 z="hello" w=true />)");
    ASSERT_TRUE(r.ok());

    // Tokens: < E x = 42 y = -3.14 z = "hello" w = true />
    //         0 1 2 3 4  5 6 7     8 9 10     11 12 13 14
    ASSERT_EQ(r.tokens[4].type, AmlTokenType::INTEGER);
    ASSERT_EQ(r.tokens[4].text, "42");

    ASSERT_EQ(r.tokens[7].type, AmlTokenType::FLOAT);
    ASSERT_EQ(r.tokens[7].text, "-3.14");

    ASSERT_EQ(r.tokens[10].type, AmlTokenType::STRING);
    ASSERT_EQ(r.tokens[10].text, "hello");

    ASSERT_EQ(r.tokens[13].type, AmlTokenType::BOOL);
    ASSERT_EQ(r.tokens[13].text, "true");
}

static void testTokenizeArray()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize(R"(<E v=[1, 2, 3] />)");
    ASSERT_TRUE(r.ok());
    // < E v = [ 1 , 2 , 3 ] /> EOF
    ASSERT_EQ(r.tokens[4].type, AmlTokenType::BRACKET_OPEN);
    ASSERT_EQ(r.tokens[5].type, AmlTokenType::INTEGER);
    ASSERT_EQ(r.tokens[10].type, AmlTokenType::BRACKET_CLOSE);
}

static void testTokenizeComment()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize("# this is a comment\n<Frame />");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.tokens[0].type, AmlTokenType::TAG_OPEN);
    ASSERT_EQ(r.tokens[1].text, "Frame");
}

static void testTokenizeUnterminatedString()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize(R"(<E x="oops)");
    ASSERT_TRUE(!r.ok());
}

static void testTokenizeLineTracking()
{
    TEST_SUITE("tokenizer");
    AmlTokenizer tok;
    auto r = tok.tokenize("<A>\n  <B />\n</A>");
    ASSERT_TRUE(r.ok());
    // Tokens: < A > < B /> </ A >
    //         0 1 2 3 4 5  6  7 8
    auto &bOpen = r.tokens[3];
    ASSERT_EQ(bOpen.line, 2u);
}

// --- Parser suite ---

static void testParseSelfClosing()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<Frame name="test" padding=8 />)");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.roots.size(), 1u);
    ASSERT_EQ(r.roots[0].tag, "Frame");
    ASSERT_EQ(r.roots[0].attributes.size(), 2u);
    ASSERT_TRUE(r.roots[0].attributes.at("name").isString());
    ASSERT_EQ(r.roots[0].attributes.at("name").asString(), "test");
    ASSERT_TRUE(r.roots[0].attributes.at("padding").isInt());
    ASSERT_EQ(r.roots[0].attributes.at("padding").asInt(), 8);
}

static void testParseNested()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(
        <ScrollingFrame name="inv" visible=true>
            <Frame name="header" size=[1, 0, 0, 40]>
                <TextLabel text="Items" />
            </Frame>
            <TextButton name="slot" />
        </ScrollingFrame>
    )");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.roots.size(), 1u);

    auto &root = r.roots[0];
    ASSERT_EQ(root.tag, "ScrollingFrame");
    ASSERT_TRUE(root.attributes.at("visible").asBool());
    ASSERT_EQ(root.children.size(), 2u);

    auto &header = root.children[0];
    ASSERT_EQ(header.tag, "Frame");
    ASSERT_EQ(header.children.size(), 1u);
    ASSERT_EQ(header.children[0].tag, "TextLabel");

    auto &size = header.attributes.at("size");
    ASSERT_TRUE(size.isArray());
    ASSERT_EQ(size.asArray().size(), 4u);
    ASSERT_EQ(size.asArray()[0].asInt(), 1);
    ASSERT_EQ(size.asArray()[3].asInt(), 40);
}

static void testParseMultipleRoots()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<A /><B /><C />)");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.roots.size(), 3u);
    ASSERT_EQ(r.roots[0].tag, "A");
    ASSERT_EQ(r.roots[1].tag, "B");
    ASSERT_EQ(r.roots[2].tag, "C");
}

static void testParseMismatchedTag()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<Foo></Bar>)");
    ASSERT_TRUE(!r.ok());
}

static void testParseNestedArray()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<E m=[[1, 2], [3, 4]] />)");
    ASSERT_TRUE(r.ok());
    auto &m = r.roots[0].attributes.at("m");
    ASSERT_TRUE(m.isArray());
    ASSERT_EQ(m.asArray().size(), 2u);
    ASSERT_TRUE(m.asArray()[0].isArray());
    ASSERT_EQ(m.asArray()[0].asArray()[1].asInt(), 2);
}

static void testParseFloatAttribute()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<E opacity=0.75 />)");
    ASSERT_TRUE(r.ok());
    auto &val = r.roots[0].attributes.at("opacity");
    ASSERT_TRUE(val.isFloat());
    ASSERT_TRUE(val.asFloat() > 0.74 && val.asFloat() < 0.76);
}

static void testParseEmptyChildren()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<Frame></Frame>)");
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.roots[0].children.size(), 0u);
}

static void testAsNumber()
{
    TEST_SUITE("parser");
    auto r = parseSource(R"(<E a=10 b=2.5 />)");
    ASSERT_TRUE(r.ok());
    auto &a = r.roots[0].attributes.at("a");
    auto &b = r.roots[0].attributes.at("b");
    ASSERT_TRUE(a.asNumber() > 9.99 && a.asNumber() < 10.01);
    ASSERT_TRUE(b.asNumber() > 2.49 && b.asNumber() < 2.51);
}

int main()
{
    int beforeFail;

    std::printf("=== AML Tokenizer ===\n");
    beforeFail = g_failed; RUN_TEST(testTokenizeSimpleTag);
    beforeFail = g_failed; RUN_TEST(testTokenizeTypedValues);
    beforeFail = g_failed; RUN_TEST(testTokenizeArray);
    beforeFail = g_failed; RUN_TEST(testTokenizeComment);
    beforeFail = g_failed; RUN_TEST(testTokenizeUnterminatedString);
    beforeFail = g_failed; RUN_TEST(testTokenizeLineTracking);

    std::printf("\n=== AML Parser ===\n");
    beforeFail = g_failed; RUN_TEST(testParseSelfClosing);
    beforeFail = g_failed; RUN_TEST(testParseNested);
    beforeFail = g_failed; RUN_TEST(testParseMultipleRoots);
    beforeFail = g_failed; RUN_TEST(testParseMismatchedTag);
    beforeFail = g_failed; RUN_TEST(testParseNestedArray);
    beforeFail = g_failed; RUN_TEST(testParseFloatAttribute);
    beforeFail = g_failed; RUN_TEST(testParseEmptyChildren);
    beforeFail = g_failed; RUN_TEST(testAsNumber);

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
