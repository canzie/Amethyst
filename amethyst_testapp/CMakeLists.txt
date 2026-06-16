find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

function(add_amethyst_demo target_name source_file)
    add_executable(${target_name} ${source_file} vk_context.cpp)
    target_link_libraries(${target_name} PRIVATE amethyst amethyst_vk13_glfw Vulkan::Vulkan glfw)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_definitions(${target_name} PRIVATE
        AMETHYST_SHADER_DIR="${CMAKE_SOURCE_DIR}/backends/shaders/spirv"
        AMETHYST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/libamethyst/assets"
    )
    target_compile_options(${target_name} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wno-reorder-init-list -Wno-missing-designated-field-initializers>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endfunction()

add_amethyst_demo(amethyst_testapp main.cpp)
add_amethyst_demo(amethyst_demo_menus demo_menus.cpp)
add_amethyst_demo(amethyst_demo_treeview demo_treeview.cpp)
add_amethyst_demo(amethyst_demo_collapsible_header demo_collapsible_header.cpp)
add_amethyst_demo(amethyst_demo_svg demo_svg.cpp)
add_amethyst_demo(amethyst_demo_color_picker demo_color_picker.cpp)
add_amethyst_demo(amethyst_demo_drag demo_drag.cpp)
