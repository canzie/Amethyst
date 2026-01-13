cmake_minimum_required(VERSION 3.20)
project(Amethyst VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AMETHYST_BUILD_TESTAPP "Build the Amethyst test application" ON)

add_subdirectory(libamethyst)
add_subdirectory(backends)

    add_subdirectory(amethyst_testapp)
