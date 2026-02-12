#ifndef AMETHYST__TEST_MACROS_H
#define AMETHYST__TEST_MACROS_H

#include <cstdio>

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

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
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

#endif // AMETHYST__TEST_MACROS_H
