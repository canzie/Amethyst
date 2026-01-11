file(GLOB_RECURSE TESTAPP_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(amethyst_testapp ${TESTAPP_SOURCES})

target_link_libraries(amethyst_testapp PRIVATE amethyst)

target_compile_options(amethyst_testapp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
