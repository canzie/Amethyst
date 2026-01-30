file(GLOB_RECURSE TESTAPP_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(amethyst_testapp ${TESTAPP_SOURCES})

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(amethyst_testapp PRIVATE
    amethyst
    amethyst_vk13_glfw
    Vulkan::Vulkan
    glfw
)

target_include_directories(amethyst_testapp PRIVATE
    ${CMAKE_SOURCE_DIR}/backends
)

target_compile_definitions(amethyst_testapp PRIVATE
    AMETHYST_SHADER_DIR="${CMAKE_SOURCE_DIR}/backends/shaders/spirv"
    AMETHYST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/libamethyst/assets"
)

target_compile_options(amethyst_testapp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
