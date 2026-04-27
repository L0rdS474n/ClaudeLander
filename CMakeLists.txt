cmake_minimum_required(VERSION 3.20)
project(ClaudeLander
        VERSION 0.1.0
        DESCRIPTION "Clean-room C++/raylib reimplementation of Lander (Acorn Archimedes 1987)"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(CLAUDE_LANDER_BUILD_TESTS "Build the Catch2 test suite" ON)

# ----------------------------------------------------------------------------
# Dependencies via FetchContent — no system packages required.
# ----------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(raylib)

if(CLAUDE_LANDER_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()

# ----------------------------------------------------------------------------
# Compile options applied to all targets we build.
# ----------------------------------------------------------------------------
add_library(claude_lander_warnings INTERFACE)
if(MSVC)
    target_compile_options(claude_lander_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(claude_lander_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# ----------------------------------------------------------------------------
# Game executable. As implementation milestones land, modules under src/
# (core/, world/, render/, ...) will be added as object libraries and linked
# in here.
# ----------------------------------------------------------------------------
add_executable(ClaudeLander src/main.cpp)
target_include_directories(ClaudeLander PRIVATE src)
target_link_libraries(ClaudeLander PRIVATE raylib claude_lander_warnings)

# raylib + GLFW require a handful of Windows system DLLs to be linked
# explicitly when building with MinGW (and harmless under MSVC). These DLLs
# ship with every Windows install -- they do not need to be redistributed.
# See docs/adr/0001-mingw-toolchain-link-flags.md.
if(WIN32)
    target_link_libraries(ClaudeLander PRIVATE opengl32 gdi32 winmm)
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MINGW)
        target_link_options(ClaudeLander PRIVATE -mwindows)
    elseif(MSVC)
        set_target_properties(ClaudeLander PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# ----------------------------------------------------------------------------
# Tests (Catch2 v3) — opt-out via -DCLAUDE_LANDER_BUILD_TESTS=OFF.
# Cross-compiled builds skip tests automatically because they cannot be run.
# ----------------------------------------------------------------------------
if(CLAUDE_LANDER_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    include(Catch)

    add_executable(claude_lander_tests
        tests/test_smoke.cpp
    )
    target_include_directories(claude_lander_tests PRIVATE src)
    target_link_libraries(claude_lander_tests PRIVATE
        Catch2::Catch2WithMain
        claude_lander_warnings
    )
    catch_discover_tests(claude_lander_tests)
endif()
