include(FetchContent)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-3
)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.13.1
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy profiling" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "Tracy on-demand mode" FORCE)
    set(TRACY_NO_BROADCAST OFF CACHE BOOL "Enable Tracy broadcast" FORCE)
    set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "Disable system tracing" FORCE)
    set(TRACY_NO_SAMPLING ON CACHE BOOL "Disable call stack sampling" FORCE)
else()
    set(TRACY_ENABLE OFF CACHE BOOL "Enable Tracy profiling" FORCE)
endif()

FetchContent_MakeAvailable(glm spdlog freetype tracy tomlplusplus)

file(GLOB_RECURSE AMETHYST_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(amethyst ${AMETHYST_SOURCES})

target_include_directories(amethyst
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(amethyst PUBLIC glm::glm spdlog::spdlog freetype TracyClient tomlplusplus::tomlplusplus)

target_compile_options(amethyst PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
