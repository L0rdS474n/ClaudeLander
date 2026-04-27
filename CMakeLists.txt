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

# CLAUDE_LANDER_BUILD_GAME (default ON):
#   When ON  -- the full game executable is built; raylib is fetched and the
#               WIN32 DLL imports are linked.  Required for any actual run.
#   When OFF -- the game target and raylib fetch are skipped entirely.  This
#               lets a developer build only the core/ tests on a Linux box
#               without installing raylib's X11 dev dependencies.  CI workflows
#               always use the default ON.
# Local test-only invocation:
#   cmake -S . -B build-tests -G Ninja -DCLAUDE_LANDER_BUILD_GAME=OFF
option(CLAUDE_LANDER_BUILD_GAME  "Build the raylib-backed game executable" ON)

# ----------------------------------------------------------------------------
# Dependencies via FetchContent -- no system packages required.
# ----------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

if(CLAUDE_LANDER_BUILD_GAME)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

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
# core/ math layer -- pure C++, no raylib dependency.
# matrix3.hpp is fully constexpr and header-only; no .cpp counterpart needed.
# ----------------------------------------------------------------------------
add_library(claude_lander_core OBJECT
    src/core/vec3.cpp
    src/core/prng.cpp
    src/core/lookup_tables.cpp
)
target_include_directories(claude_lander_core PUBLIC src)
target_link_libraries(claude_lander_core PRIVATE claude_lander_warnings)

# ----------------------------------------------------------------------------
# world/ procedural terrain layer -- depends on core/, no raylib (ADR-0004).
# Declared unconditionally (not gated on BUILD_GAME) so the test executable
# can link against it on a Linux box without raylib's X11/OpenGL deps.
#
# Note: src/world/shadow.cpp defines render::project_shadow.  The header is
# in src/render/shadow.hpp but the body needs terrain::altitude, and src/
# render/ is forbidden from including world/ headers by the
# `render_no_raylib_or_world_includes` tripwire.  Hosting the .cpp here is
# the agreed-upon compromise; see the banner in src/render/shadow.hpp.
# ----------------------------------------------------------------------------
add_library(claude_lander_world OBJECT
    src/world/terrain.cpp
    src/world/camera_follow.cpp
    src/world/shadow.cpp
    src/world/object_map.cpp
)
target_include_directories(claude_lander_world PUBLIC src)
target_link_libraries(claude_lander_world PRIVATE
    claude_lander_core
    claude_lander_warnings
)

# ----------------------------------------------------------------------------
# render/ projection layer -- depends on core/ ONLY.  No raylib, no world/,
# no entities/ (ADR-0006, planner D-Yflip).  Declared unconditionally so the
# test executable can link against it without raylib's X11/OpenGL deps.
# Boundary is enforced by the `render_no_raylib_or_world_includes` CTest
# tripwire below.
# ----------------------------------------------------------------------------
add_library(claude_lander_render OBJECT
    src/render/projection.cpp
    src/render/faces.cpp
    src/render/bin_sorter.cpp
    src/render/hud.cpp
)
target_include_directories(claude_lander_render PUBLIC src)
target_link_libraries(claude_lander_render PRIVATE
    claude_lander_core
    claude_lander_warnings
)

# ----------------------------------------------------------------------------
# physics/ kinematics layer -- pure stateless functions on Vec3/Mat3.
# Depends on core/ only.  No raylib, no world/, no entities/, no <random>,
# no <chrono>.  Boundary enforced by physics_no_forbidden_includes CTest
# tripwire below.
# (Pass 4 — src/physics/kinematics.cpp does not exist yet; red-state intentional)
# ----------------------------------------------------------------------------
add_library(claude_lander_physics OBJECT
    src/physics/kinematics.cpp
    src/physics/collision.cpp
)
target_include_directories(claude_lander_physics PUBLIC src)
target_link_libraries(claude_lander_physics PRIVATE
    claude_lander_core
    claude_lander_warnings
)

# ----------------------------------------------------------------------------
# entities/ ship layer -- Ship struct + step().
# Depends on core/ and physics/.  No raylib, no world/, no render/.
# Boundary enforced by entities_no_forbidden_includes CTest tripwire below.
# (Pass 4 — src/entities/ship.cpp does not exist yet; red-state intentional)
# ----------------------------------------------------------------------------
add_library(claude_lander_entities OBJECT
    src/entities/ship.cpp
    src/entities/rock.cpp
    src/entities/particle.cpp
    src/entities/ground_object.cpp
)
target_include_directories(claude_lander_entities PUBLIC src)
target_link_libraries(claude_lander_entities PRIVATE
    claude_lander_core
    claude_lander_physics
    claude_lander_warnings
)

# ----------------------------------------------------------------------------
# input/ mouse-polar layer -- pure stateless math functions on Vec2/Mat3.
# Depends on core/ only.  No raylib, no world/, no entities/, no render/,
# no physics/, no <random>, no <chrono>.  Boundary enforced by the
# input_no_forbidden_includes CTest tripwire below.
# (Pass 5 -- src/input/mouse_polar.cpp does not exist yet; red-state intentional)
# ----------------------------------------------------------------------------
add_library(claude_lander_input OBJECT
    src/input/mouse_polar.cpp
)
target_include_directories(claude_lander_input PUBLIC src)
target_link_libraries(claude_lander_input PRIVATE
    claude_lander_core
    claude_lander_warnings
)

# ----------------------------------------------------------------------------
# Game executable. As implementation milestones land, modules under src/
# (core/, world/, render/, ...) will be added as object libraries and linked
# in here.
# ----------------------------------------------------------------------------
if(CLAUDE_LANDER_BUILD_GAME)
    add_executable(ClaudeLander src/main.cpp)
    target_include_directories(ClaudeLander PRIVATE src)
    target_link_libraries(ClaudeLander PRIVATE
        raylib
        claude_lander_core
        claude_lander_world
        claude_lander_render
        claude_lander_physics
        claude_lander_entities
        claude_lander_input
        claude_lander_warnings
    )

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
endif()

# ----------------------------------------------------------------------------
# Tests (Catch2 v3) -- opt-out via -DCLAUDE_LANDER_BUILD_TESTS=OFF.
# Cross-compiled builds skip tests automatically because they cannot be run.
# ----------------------------------------------------------------------------
if(CLAUDE_LANDER_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    include(Catch)

    add_executable(claude_lander_tests
        tests/test_smoke.cpp
        tests/test_vec3.cpp
        tests/test_matrix3.cpp
        tests/test_prng.cpp
        tests/test_lookup_tables.cpp
        tests/test_terrain.cpp
        tests/test_projection.cpp
        tests/test_kinematics.cpp
        tests/test_ship.cpp
        tests/test_mouse_polar.cpp
        tests/test_faces.cpp
        tests/test_camera_follow.cpp
        tests/test_bin_sorter.cpp
        tests/test_shadow.cpp
        tests/test_collision.cpp
        tests/test_rock.cpp
        tests/test_particle.cpp
        tests/test_object_map.cpp
        tests/test_hud.cpp
    )
    target_include_directories(claude_lander_tests PRIVATE src)
    target_link_libraries(claude_lander_tests PRIVATE
        Catch2::Catch2WithMain
        claude_lander_core
        claude_lander_world
        claude_lander_render
        claude_lander_physics
        claude_lander_entities
        claude_lander_input
        claude_lander_warnings
    )
    catch_discover_tests(claude_lander_tests)

    # ------------------------------------------------------------------------
    # AC-W40 boundary tripwire: src/world/ must not include any raylib header.
    # We grep all headers and translation units under src/world/ for the
    # substring "raylib" appearing on a #include line; the test fails if any
    # match is found.  Using -L (files-without-match) inverts the semantics so
    # a clean tree exits 0 on every file and grep returns zero overall only
    # when all files lack the match.  We use -E and a regex that matches
    # #include lines pulling in raylib.h, rlgl.h, raymath.h, etc.
    # ------------------------------------------------------------------------
    add_test(
        NAME world_no_raylib_includes
        COMMAND ${CMAKE_COMMAND} -E env
                bash -c "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath)' ${CMAKE_SOURCE_DIR}/src/world/ ; then exit 1 ; else exit 0 ; fi"
    )

    # ------------------------------------------------------------------------
    # AC-R80 boundary tripwire: src/render/ must not include any raylib,
    # world/, or entities/ header.  Grep fires on the first match and exits 1.
    # ------------------------------------------------------------------------
    add_test(
        NAME render_no_raylib_or_world_includes
        COMMAND ${CMAKE_COMMAND} -E env
                bash -c "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath|world/|entities/)' ${CMAKE_SOURCE_DIR}/src/render/ ; then exit 1 ; else exit 0 ; fi"
    )

    # ------------------------------------------------------------------------
    # AC-K80 boundary tripwire: src/physics/ must not include raylib, world/,
    # entities/, render/, <random>, or <chrono>.
    # ------------------------------------------------------------------------
    add_test(
        NAME physics_no_forbidden_includes
        COMMAND ${CMAKE_COMMAND} -E env
                bash -c "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath|world/|entities/|render/|random|chrono)' ${CMAKE_SOURCE_DIR}/src/physics/ ; then exit 1 ; else exit 0 ; fi"
    )

    # ------------------------------------------------------------------------
    # AC-S80 boundary tripwire: src/entities/ must not include raylib, world/,
    # or render/ headers.
    # ------------------------------------------------------------------------
    add_test(
        NAME entities_no_forbidden_includes
        COMMAND ${CMAKE_COMMAND} -E env
                bash -c "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath|world/|render/)' ${CMAKE_SOURCE_DIR}/src/entities/ ; then exit 1 ; else exit 0 ; fi"
    )

    # ------------------------------------------------------------------------
    # AC-I80 boundary tripwire: src/input/ must not include raylib, world/,
    # entities/, render/, physics/, <random>, or <chrono>.
    # ------------------------------------------------------------------------
    add_test(
        NAME input_no_forbidden_includes
        COMMAND ${CMAKE_COMMAND} -E env
                bash -c "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath|world/|entities/|render/|physics/|random|chrono)' ${CMAKE_SOURCE_DIR}/src/input/ ; then exit 1 ; else exit 0 ; fi"
    )
endif()
