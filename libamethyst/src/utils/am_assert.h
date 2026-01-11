/*
 * Amethyst assertion macros
 */

#ifndef AMETHYST__AM_ASSERT_H
#define AMETHYST__AM_ASSERT_H

#include <spdlog/spdlog.h>
#include <cstdlib>

#ifdef NDEBUG
    #define AM_ASSERT(condition, msg) ((void)0)
    #define AM_ASSERT_MSG(condition, fmt, ...) ((void)0)
#else
    #define AM_ASSERT(condition, msg) \
        do { \
            if (!(condition)) { \
                spdlog::critical("Assertion failed: {} | {} | {}:{}", \
                    #condition, msg, __FILE__, __LINE__); \
                std::abort(); \
            } \
        } while (0)

    #define AM_ASSERT_MSG(condition, fmt, ...) \
        do { \
            if (!(condition)) { \
                spdlog::critical("Assertion failed: {} | " fmt " | {}:{}", \
                    #condition, ##__VA_ARGS__, __FILE__, __LINE__); \
                std::abort(); \
            } \
        } while (0)
#endif

// Simple assert using && trick for message
#define AM_CHECK(condition) \
    (void)((condition) || (spdlog::critical("Check failed: {} | {}:{}", #condition, __FILE__, __LINE__), std::abort(), 0))

// Unreachable code marker
#define AM_UNREACHABLE() \
    do { \
        spdlog::critical("Unreachable code reached | {}:{}", __FILE__, __LINE__); \
        std::abort(); \
    } while (0)

#endif // AMETHYST__AM_ASSERT_H
