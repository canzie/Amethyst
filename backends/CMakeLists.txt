find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_library(amethyst_vk13_glfw STATIC
    amethyst__vk13_glfw.cpp
)

target_include_directories(amethyst_vk13_glfw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(amethyst_vk13_glfw PUBLIC
    amethyst
    Vulkan::Vulkan
    glfw
)

target_compile_options(amethyst_vk13_glfw PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
