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

FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG v3.2.1
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

FetchContent_MakeAvailable(glm spdlog freetype tracy tomlplusplus lunasvg)

file(GLOB_RECURSE AMETHYST_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(amethyst ${AMETHYST_SOURCES})

target_include_directories(amethyst
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(amethyst PUBLIC glm::glm spdlog::spdlog freetype TracyClient tomlplusplus::tomlplusplus lunasvg)

target_compile_definitions(amethyst PRIVATE TOML_EXCEPTIONS=0)

target_compile_options(amethyst PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

if(AMETHYST_BUILD_TESTS)
    if(AMETHYST_TEST_VALGRIND)
        find_program(VALGRIND_EXECUTABLE valgrind)
        if(NOT VALGRIND_EXECUTABLE)
            message(FATAL_ERROR "AMETHYST_TEST_VALGRIND is ON but valgrind was not found")
        endif()
        message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
    endif()

    file(GLOB AMETHYST_TEST_SOURCES CONFIGURE_DEPENDS "tests/*_test.cpp")

    foreach(test_source ${AMETHYST_TEST_SOURCES})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE amethyst)
        target_compile_options(${test_name} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        )

        if(AMETHYST_RUN_TESTS)
            if(AMETHYST_TEST_VALGRIND)
                add_custom_command(TARGET ${test_name} POST_BUILD
                    COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 $<TARGET_FILE:${test_name}>
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Running ${test_name} under valgrind..."
                )
            else()
                add_custom_command(TARGET ${test_name} POST_BUILD
                    COMMAND ${test_name}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Running ${test_name}..."
                )
            endif()
        endif()
    endforeach()
endif()
