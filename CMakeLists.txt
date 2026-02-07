cmake_minimum_required(VERSION 3.20)
project(Amethyst VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(AMETHYST_BUILD_TESTAPP "Build the Amethyst test application" ON)
option(AMETHYST_BUILD_TESTS "Build unit tests" OFF)
option(AMETHYST_RUN_TESTS "Run unit tests automatically after build" OFF)
option(AMETHYST_TEST_VALGRIND "Run tests under valgrind" OFF)

add_subdirectory(libamethyst)
add_subdirectory(backends)

if(AMETHYST_BUILD_TESTAPP)
    add_subdirectory(amethyst_testapp)
endif()
