include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

if(AMETHYST_TEST_VALGRIND)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(NOT VALGRIND_EXECUTABLE)
        message(FATAL_ERROR "AMETHYST_TEST_VALGRIND is ON but valgrind was not found")
    endif()
endif()

# Shared helpers (fixtures, fake contexts) live here and are compiled into every suite.
file(GLOB AMETHYST_TEST_COMMON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp")

# One executable per module: every subdirectory with *_test.cpp files becomes
# amethyst_<dir>_tests, so a suite is many small TESTs in a handful of files.
file(GLOB AMETHYST_TEST_DIRS LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir ${AMETHYST_TEST_DIRS})
    if(NOT IS_DIRECTORY ${dir})
        continue()
    endif()
    get_filename_component(suite ${dir} NAME)
    if(suite STREQUAL "common")
        continue()
    endif()
    file(GLOB suite_sources CONFIGURE_DEPENDS "${dir}/*_test.cpp")
    if(NOT suite_sources)
        continue()
    endif()

    set(target amethyst_${suite}_tests)
    add_executable(${target} ${suite_sources} ${AMETHYST_TEST_COMMON})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE amethyst GTest::gtest_main)
    target_compile_definitions(${target} PRIVATE AMETHYST_TEST_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
        $<$<CXX_COMPILER_ID:Clang>:-Wno-reorder-init-list -Wno-missing-designated-field-initializers>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    )
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)

    if(AMETHYST_RUN_TESTS)
        if(AMETHYST_TEST_VALGRIND)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 $<TARGET_FILE:${target}>
                COMMENT "Running ${target} under valgrind...")
        else()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${target}
                COMMENT "Running ${target}...")
        endif()
    endif()
endforeach()
