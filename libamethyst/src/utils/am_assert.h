/*
 * Amethyst assertion macros
 */

#ifndef AMETHYST__AM_ASSERT_H
#define AMETHYST__AM_ASSERT_H

#include <cstdio>
#include <cstdlib>
#include <format>

#ifdef NDEBUG
#define AM_ASSERT(condition, msg)     ((void)0)
#define AM_ASSERT_MSG(condition, ...) ((void)0)
#else
#define AM_ASSERT(condition, msg)                                                                        \
    do {                                                                                                 \
        if (!(condition)) {                                                                              \
            fprintf(stderr, "Assertion failed: %s | %s | %s:%d\n", #condition, msg, __FILE__, __LINE__); \
            std::abort();                                                                                \
        }                                                                                                \
    } while (0)

#define AM_ASSERT_MSG(condition, ...)                                                                                \
    do {                                                                                                             \
        if (!(condition)) {                                                                                          \
            auto _am_msg = std::format(__VA_ARGS__);                                                                 \
            fprintf(stderr, "Assertion failed: %s | %s | %s:%d\n", #condition, _am_msg.c_str(), __FILE__, __LINE__); \
            std::abort();                                                                                            \
        }                                                                                                            \
    } while (0)
#endif

#define AM_CHECK(condition) \
    (void)((condition) || (fprintf(stderr, "Check failed: %s | %s:%d\n", #condition, __FILE__, __LINE__), std::abort(), 0))

#define AM_UNREACHABLE()                                                           \
    do {                                                                           \
        fprintf(stderr, "Unreachable code reached | %s:%d\n", __FILE__, __LINE__); \
        std::abort();                                                              \
    } while (0)

#endif // AMETHYST__AM_ASSERT_H
