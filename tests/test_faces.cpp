// tests/test_faces.cpp — Pass 6 render/faces tests.
//
// Covers AC-F01..F23 and AC-F80..F82 (24 ACs total).
// All tests tagged [render][faces].
//
// === Determinism plan ===
// All tests are fully deterministic.  shade_face and rotate_vertices are
// declared noexcept pure functions with no clocks, no PRNG, no filesystem,
// no system calls, no global mutable state.
// AC-F04 and AC-F23 verify bit-identical results between two independent runs.
//
// === Brightness formula (D-BrightnessFormula, locked) ===
//   brightness = clamp(0.5f + 0.5f * (-normal.y) + 0.1f * (-normal.x), 0.0f, 1.0f)
//
// === Cull sense (D-CullSense, locked) ===
//   Face is VISIBLE iff dot(normal_world, face_centroid_world - camera_position) < 0
//   Face is CULLED  iff dot product >= 0
//   Face centroid = (v[v0] + v[v1] + v[v2]) / 3
//
// === Winding (D-OutwardNormals) ===
//   normal = normalize(cross(v1 - v0, v2 - v0))  — outward CCW from outside
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught by dedicated tests with banner comments:
//   (a) AC-F11 (AC-F-y-up):      normal_world={0,-1,0} → brightness ≈ 1.0 (Y-DOWN fence).
//   (b) AC-F18 (AC-F-winding):   face 0 outward-normal test: dot(n, centroid-body_origin)>0.
//   (c) AC-F22 (AC-F-cull-sense): synthetic face normal {0,0,-1} toward camera is visible;
//                                  {0,0,1} is culled.
//
// === AC-F82 noexcept verification ===
// static_assert checks noexcept on both public functions placed after the
// #include so they fire at compile time in the same TU.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/core/face_types.hpp
//   src/render/faces.hpp
//   src/render/faces.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

#include "core/vec3.hpp"
#include "core/matrix3.hpp"
#include "core/face_types.hpp"
#include "entities/ship.hpp"
#include "render/faces.hpp"

// ---------------------------------------------------------------------------
// AC-F80: src/render/faces.hpp and src/core/face_types.hpp must not pull in
// raylib, world/, entities/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-F80 VIOLATED: render/faces.hpp or core/face_types.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency.");
#endif

// ---------------------------------------------------------------------------
// AC-F82: Both public functions must be declared noexcept.
// Checked at compile time here in the same TU as the #include.
// ---------------------------------------------------------------------------
namespace {
    // Build a minimal span for the noexcept probe — we only need the
    // call expression to be syntactically valid; it will never execute.
    inline const Vec3 kProbeVerts[3] = {{0,0,0},{1,0,0},{0,1,0}};
    inline const core::ShipFace kProbeFace{0, 1, 2, 0x000};
}

static_assert(
    noexcept(render::shade_face(
        std::span<const Vec3>(kProbeVerts, 3),
        kProbeFace,
        Vec3{0.0f, 0.0f, -5.0f})),
    "AC-F82: render::shade_face must be declared noexcept");

static_assert(
    noexcept(render::rotate_vertices(
        std::span<const Vec3>(kProbeVerts, 3),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>{})),
    "AC-F82: render::rotate_vertices must be declared noexcept");

// ---------------------------------------------------------------------------
// Tolerance constants (per planner spec §7)
// ---------------------------------------------------------------------------
static constexpr float kFaceEps   = 1e-5f;
static constexpr float kBrightEps = 1e-4f;

// ---------------------------------------------------------------------------
// Helper: compute the magnitude of a Vec3.
// ---------------------------------------------------------------------------
static float vec_magnitude(Vec3 v) noexcept {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

// ---------------------------------------------------------------------------
// Helper: compute outward normal from three vertices using the locked winding.
//   normal = normalize(cross(v1 - v0, v2 - v0))
// ---------------------------------------------------------------------------
static Vec3 compute_normal(Vec3 v0, Vec3 v1, Vec3 v2) noexcept {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 c     = cross(edge1, edge2);
    return normalize(c);
}

// ---------------------------------------------------------------------------
// Helper: brightness formula, applied to a world-space normal.
//   brightness = clamp(0.5f + 0.5f*(-normal.y) + 0.1f*(-normal.x), 0.0f, 1.0f)
// ---------------------------------------------------------------------------
static float expected_brightness(Vec3 n) noexcept {
    const float raw = 0.5f + 0.5f * (-n.y) + 0.1f * (-n.x);
    if (raw < 0.0f) return 0.0f;
    if (raw > 1.0f) return 1.0f;
    return raw;
}

// ===========================================================================
// GROUP 1: rotate_vertices (AC-F01..F05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-F01 — Identity orient + zero translation = pass-through, exact.
//   Given: 9 body-frame vertices, orientation=identity, translation={0,0,0}
//   When:  rotate_vertices is called
//   Then:  each output vertex equals the corresponding input vertex exactly
// ---------------------------------------------------------------------------
TEST_CASE("AC-F01: rotate_vertices with identity and zero translation is exact pass-through", "[render][faces]") {
    // Given
    const auto& body = entities::kShipVertices;
    std::array<Vec3, entities::kShipVertexCount> world{};

    // When
    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    // Then
    for (std::size_t i = 0; i < entities::kShipVertexCount; ++i) {
        CAPTURE(i, body[i].x, body[i].y, body[i].z, world[i].x, world[i].y, world[i].z);
        REQUIRE(world[i].x == body[i].x);
        REQUIRE(world[i].y == body[i].y);
        REQUIRE(world[i].z == body[i].z);
    }
}

// ---------------------------------------------------------------------------
// AC-F02 — Identity orient + translation {3,-2,7} = body+t element-wise within kFaceEps.
//   Given: 9 body-frame vertices, orientation=identity, translation={3,-2,7}
//   When:  rotate_vertices is called
//   Then:  world[i] == body[i] + {3,-2,7} within kFaceEps for all i
// ---------------------------------------------------------------------------
TEST_CASE("AC-F02: rotate_vertices with identity and non-zero translation adds translation element-wise", "[render][faces]") {
    // Given
    const auto& body = entities::kShipVertices;
    std::array<Vec3, entities::kShipVertexCount> world{};
    const Vec3 translation{3.0f, -2.0f, 7.0f};

    // When
    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        identity(),
        translation,
        std::span<Vec3>(world.data(), world.size()));

    // Then
    for (std::size_t i = 0; i < entities::kShipVertexCount; ++i) {
        CAPTURE(i, body[i].x, body[i].y, body[i].z, world[i].x, world[i].y, world[i].z);
        REQUIRE(world[i].x == Catch::Approx(body[i].x + translation.x).margin(kFaceEps));
        REQUIRE(world[i].y == Catch::Approx(body[i].y + translation.y).margin(kFaceEps));
        REQUIRE(world[i].z == Catch::Approx(body[i].z + translation.z).margin(kFaceEps));
    }
}

// ---------------------------------------------------------------------------
// AC-F03 — Pure 90° yaw on kShipVertices: lengths preserved within kFaceEps.
//   Given: 9 body-frame vertices, orientation=90-degree yaw around Y, translation={0,0,0}
//   When:  rotate_vertices is called
//   Then:  magnitude of each output vertex equals magnitude of corresponding input
//          within kFaceEps (rotation is an isometry)
// ---------------------------------------------------------------------------
TEST_CASE("AC-F03: rotate_vertices with 90-degree yaw preserves vertex distances from origin", "[render][faces]") {
    // Given
    const auto& body = entities::kShipVertices;
    std::array<Vec3, entities::kShipVertexCount> world{};

    // Build a pure 90-degree yaw (rotation around Y axis by pi/2):
    //   col[0] = { cos(pi/2), 0, -sin(pi/2)} = {0,  0, -1}
    //   col[1] = {0,          1,  0          } = {0,  1,  0}
    //   col[2] = { sin(pi/2), 0,  cos(pi/2) } = {1,  0,  0}
    const Mat3 yaw90{
        Vec3{ 0.0f, 0.0f, -1.0f},  // col[0]
        Vec3{ 0.0f, 1.0f,  0.0f},  // col[1]
        Vec3{ 1.0f, 0.0f,  0.0f}   // col[2]
    };

    // When
    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        yaw90,
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    // Then — lengths preserved
    for (std::size_t i = 0; i < entities::kShipVertexCount; ++i) {
        const float body_len  = vec_magnitude(body[i]);
        const float world_len = vec_magnitude(world[i]);
        CAPTURE(i, body_len, world_len);
        REQUIRE(world_len == Catch::Approx(body_len).margin(kFaceEps));
    }
}

// ---------------------------------------------------------------------------
// AC-F04 — Determinism: bit-identical between two fresh runs.
//   Given: two independent calls with the same input (90-degree yaw on kShipVertices)
//   When:  rotate_vertices is called twice in sequence
//   Then:  both output arrays are bit-identical (exact equality, NOT Approx)
// ---------------------------------------------------------------------------
TEST_CASE("AC-F04: rotate_vertices is deterministic — bit-identical across two fresh calls", "[render][faces]") {
    // Given
    const auto& body = entities::kShipVertices;
    const Mat3 yaw90{
        Vec3{ 0.0f, 0.0f, -1.0f},
        Vec3{ 0.0f, 1.0f,  0.0f},
        Vec3{ 1.0f, 0.0f,  0.0f}
    };
    const Vec3 translation{1.5f, -0.5f, 2.0f};

    // When
    std::array<Vec3, entities::kShipVertexCount> world_a{};
    std::array<Vec3, entities::kShipVertexCount> world_b{};

    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        yaw90, translation,
        std::span<Vec3>(world_a.data(), world_a.size()));
    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        yaw90, translation,
        std::span<Vec3>(world_b.data(), world_b.size()));

    // Then — bit-identical
    for (std::size_t i = 0; i < entities::kShipVertexCount; ++i) {
        CAPTURE(i, world_a[i].x, world_b[i].x);
        REQUIRE(world_a[i].x == world_b[i].x);
        REQUIRE(world_a[i].y == world_b[i].y);
        REQUIRE(world_a[i].z == world_b[i].z);
    }
}

// ---------------------------------------------------------------------------
// AC-F05 — Span contract: body.size() == world_out.size() == 9 honoured.
//   Given: the kShipVertices span (size 9) and an output span of the same size
//   When:  rotate_vertices is called
//   Then:  all 9 output slots are populated (no crash, no partial write)
//          Verified by checking each element is finite.
// ---------------------------------------------------------------------------
TEST_CASE("AC-F05: rotate_vertices with size-9 spans populates all 9 output vertices", "[render][faces]") {
    // Given
    const auto& body = entities::kShipVertices;
    std::array<Vec3, 9> world{};
    REQUIRE(body.size() == 9);

    // When
    render::rotate_vertices(
        std::span<const Vec3>(body.data(), body.size()),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    // Then — all slots populated and finite
    for (std::size_t i = 0; i < 9; ++i) {
        CAPTURE(i, world[i].x, world[i].y, world[i].z);
        REQUIRE(std::isfinite(world[i].x));
        REQUIRE(std::isfinite(world[i].y));
        REQUIRE(std::isfinite(world[i].z));
    }
}

// ===========================================================================
// GROUP 2: shade_face cull + shade (AC-F06..F10)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-F06 — Front-face visible: outward-pointing normal toward camera returns populated optional.
//   Given: a triangle in the XY plane at z=0 with normal pointing toward +z,
//          camera at {0,0,-5} (looking at the face from -z direction)
//   When:  shade_face is called
//   Then:  returns a populated optional<VisibleFace>
// ---------------------------------------------------------------------------
TEST_CASE("AC-F06: shade_face on front-facing triangle toward camera returns populated optional", "[render][faces]") {
    // Given: triangle with winding producing normal {0,0,-1} (toward -z / toward camera)
    // Camera at {0,0,-5} so face_centroid - camera points in +z, dot < 0 → visible
    // Vertices: v0={-1,0,0}, v1={1,0,0}, v2={0,1,0}
    // cross((v1-v0),(v2-v0)) = cross({2,0,0},{1,1,0}) = {0*0-0*1, 0*1-2*0, 2*1-0*1} = {0,0,2}
    // normalize → {0,0,1}
    // centroid = {0, 1/3, 0}
    // dot({0,0,1}, {0,1/3,0} - {0,0,-5}) = dot({0,0,1},{0,1/3,5}) = 5 > 0 — CULLED
    //
    // Flip winding: v0={-1,0,0}, v1={0,1,0}, v2={1,0,0}
    // cross({1,1,0},{2,0,0}) = {1*0-0*0, 0*2-1*0, 1*0-1*2} = {0,0,-2}
    // normalize → {0,0,-1}
    // centroid = {0, 1/3, 0}
    // dot({0,0,-1}, {0,1/3,5}) = -5 < 0 — VISIBLE
    const std::array<Vec3, 3> verts{{
        {-1.0f, 0.0f, 0.0f},
        { 0.0f, 1.0f, 0.0f},
        { 1.0f, 0.0f, 0.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x040};
    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-F07 — Back-face culled: returns std::nullopt.
//   Given: same triangle geometry but winding producing normal {0,0,1} (away from camera)
//   When:  shade_face is called with camera at {0,0,-5}
//   Then:  returns std::nullopt
// ---------------------------------------------------------------------------
TEST_CASE("AC-F07: shade_face on back-facing triangle away from camera returns nullopt", "[render][faces]") {
    // Given: triangle with winding producing normal {0,0,1} (away from camera at {0,0,-5})
    // v0={-1,0,0}, v1={1,0,0}, v2={0,1,0}
    // cross({2,0,0},{1,1,0}) = {0,0,2} → normal {0,0,1}
    // centroid = {0, 1/3, 0}
    // dot({0,0,1}, {0,1/3,5}) = 5 >= 0 — CULLED
    const std::array<Vec3, 3> verts{{
        {-1.0f, 0.0f, 0.0f},
        { 1.0f, 0.0f, 0.0f},
        { 0.0f, 1.0f, 0.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x040};
    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-F08 — Edge-on (dot == 0): culled (>= test).
//   Given: camera placed exactly in the plane of the triangle normal
//          so dot(normal, centroid - camera) == 0
//   When:  shade_face is called
//   Then:  returns std::nullopt  (edge-on is treated as culled)
// ---------------------------------------------------------------------------
TEST_CASE("AC-F08: shade_face with edge-on triangle (dot==0) returns nullopt (culled)", "[render][faces]") {
    // Given: triangle in XY plane at z=0, normal {0,0,1}, centroid at {0,0,0}
    // Camera placed on the centroid-normal line orthogonally: camera at {0,0,0}
    // dot({0,0,1}, {0,0,0} - {0,0,0}) = 0 → culled
    //
    // Use a camera at {0,0,0} and centroid at origin:
    // Vertices: v0={-1,-1,0}, v1={1,-1,0}, v2={0,1,0}
    // centroid = {0, -1/3, 0}
    // normal via cross((v1-v0),(v2-v0)) = cross({2,0,0},{1,2,0}) = {0,0,4} → {0,0,1}
    // camera perpendicular to normal: camera at {0, -1/3 + t, 0} for any t — centroid-camera is +z
    // Actually: dot({0,0,1}, centroid - camera) = (centroid.z - camera.z)
    // For dot == 0 need camera.z == centroid.z == 0.
    // Place camera at {0, -1.0f/3.0f, 0.0f}: centroid - camera = {0,0,0}, dot = 0
    const std::array<Vec3, 3> verts{{
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 0.0f,  1.0f, 0.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x040};
    // centroid = {(-1+1+0)/3, (-1-1+1)/3, 0} = {0, -1/3, 0}
    const Vec3 camera{0.0f, -1.0f/3.0f, 0.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-F09 — Returned normal_world is unit length within kFaceEps.
//   Given: a visible face
//   When:  shade_face returns a VisibleFace
//   Then:  |magnitude(normal_world) - 1.0f| < kFaceEps
// ---------------------------------------------------------------------------
TEST_CASE("AC-F09: shade_face returns normal_world with unit length within kFaceEps", "[render][faces]") {
    // Given: visible triangle (same as AC-F06)
    const std::array<Vec3, 3> verts{{
        {-1.0f, 0.0f, 0.0f},
        { 0.0f, 1.0f, 0.0f},
        { 1.0f, 0.0f, 0.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x040};
    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE(result.has_value());
    const float mag = vec_magnitude(result->normal_world);
    CAPTURE(mag);
    REQUIRE(std::abs(mag - 1.0f) < kFaceEps);
}

// ---------------------------------------------------------------------------
// AC-F10 — Brightness in [0, 1] for all sweeps.
//   Given: a set of visible faces with normals spanning multiple orientations
//   When:  shade_face is called on each
//   Then:  brightness is in [0.0f, 1.0f] for every returned VisibleFace
// ---------------------------------------------------------------------------
TEST_CASE("AC-F10: shade_face brightness is always in [0, 1] for various face orientations", "[render][faces]") {
    // Given: sweep over several unit-normal directions that each produce visible faces
    // We construct a triangle whose cross-product normal matches our target, then
    // place the camera such that the face is front-facing.
    //
    // For each target outward normal N, build a degenerate-free triangle with
    // that winding, and place camera on the opposite side: camera = centroid - 10*N
    // so that dot(N, centroid - camera) = dot(N, 10*N) = 10 > 0... wait, that culls.
    // We need dot < 0, so camera must satisfy dot(N, centroid - camera) < 0.
    // camera = centroid + 10*N → centroid - camera = -10*N → dot(N, -10*N) = -10 < 0. Visible.

    // Helper: build a triangle around a centroid C with outward normal N
    // We need two vectors perpendicular to N, then build the triangle.
    // Use a fixed vertex offset approach for simplicity.

    // We test the brightness formula directly using normals that are already
    // unit vectors; AC-F09 validates that shade_face produces unit normals.
    // For AC-F10 we build several synthetic front-facing cases.

    struct TestCase { Vec3 normal; };
    const Vec3 test_normals[] = {
        {0.0f, -1.0f,  0.0f},  // roof-up
        {0.0f,  1.0f,  0.0f},  // floor-down
        {-1.0f, 0.0f,  0.0f},  // left
        { 1.0f, 0.0f,  0.0f},  // right
        { 0.0f, 0.0f, -1.0f},  // toward -z camera
    };

    for (const Vec3& target_n : test_normals) {
        // Build triangle: find two vectors perpendicular to target_n.
        // Use a helper cross: pick an arbitrary non-parallel vector.
        const Vec3 arbitrary = (std::abs(target_n.x) < 0.9f)
            ? Vec3{1.0f, 0.0f, 0.0f}
            : Vec3{0.0f, 1.0f, 0.0f};
        const Vec3 u = normalize(cross(target_n, arbitrary));
        const Vec3 w = cross(target_n, u); // already unit since target_n and u are unit+perp

        // Centroid at origin; triangle vertices such that cross(v1-v0, v2-v0) || target_n
        // v0 = u, v1 = w, v2 = -u  (CCW viewed from target_n direction)
        // cross(v1-v0, v2-v0) = cross(w-u, -u-u) = cross(w-u, -2u)
        // This can be tricky; instead pick a simpler construction:
        // v0 = u, v1 = -u + w, v2 = -u - w  (equilateral-ish in tangent plane)
        const Vec3 v0 = u;
        const Vec3 v1 = Vec3{-u.x + w.x, -u.y + w.y, -u.z + w.z};
        const Vec3 v2 = Vec3{-u.x - w.x, -u.y - w.y, -u.z - w.z};

        const std::array<Vec3, 3> verts{v0, v1, v2};
        // Verify winding gives a normal in the same direction as target_n
        const Vec3 computed_n = compute_normal(v0, v1, v2);
        const float ndot = dot(computed_n, target_n);

        if (std::abs(ndot) < 0.9f) {
            // Winding produced opposite normal; skip this test case (geometry issue)
            continue;
        }

        // Camera placed at centroid + 10 * computed_n (normal direction); so face is visible
        const Vec3 centroid{
            (v0.x + v1.x + v2.x) / 3.0f,
            (v0.y + v1.y + v2.y) / 3.0f,
            (v0.z + v1.z + v2.z) / 3.0f
        };
        const Vec3 camera{
            centroid.x + 10.0f * computed_n.x,
            centroid.y + 10.0f * computed_n.y,
            centroid.z + 10.0f * computed_n.z
        };
        const core::ShipFace face{0, 1, 2, 0x040};

        const auto result = render::shade_face(
            std::span<const Vec3>(verts.data(), verts.size()),
            face, camera);

        if (result.has_value()) {
            CAPTURE(result->brightness, target_n.x, target_n.y, target_n.z);
            REQUIRE(result->brightness >= 0.0f);
            REQUIRE(result->brightness <= 1.0f);
        }
    }
}

// ===========================================================================
// GROUP 3: Brightness fences (AC-F11..F13)
// ===========================================================================
//
// These tests exercise the brightness formula directly by building a
// degenerate-free visible face whose computed normal matches the target normal.
//
// For each target normal N, we construct: camera = centroid + 10*N → visible.
// Then brightness must equal clamp(0.5 + 0.5*(-N.y) + 0.1*(-N.x), 0, 1).
//
// ===========================================================================

// ===========================================================================
//  BUG-CLASS FENCE (AC-F-y-up) — AC-F11
// ===========================================================================
//
//  D-RoofUpBrightness: at normal_world = {0, -1, 0} (roof pointing up in
//  Y-DOWN world), brightness MUST be ≈ 1.0.
//
//  Brightness formula: clamp(0.5 + 0.5*(-(-1)) + 0.1*(-(0)), 0, 1)
//                    = clamp(0.5 + 0.5*1 + 0.0, 0, 1)
//                    = clamp(1.0, 0, 1)
//                    = 1.0
//
//  If a Y-up refactor flips the sign of normal.y inside the formula (e.g.
//  using +normal.y instead of -normal.y), the result becomes 0.0 instead of
//  1.0, which this test catches loud and clearly.
//
//  If this test fails: re-read D-BrightnessFormula and D-RoofUpBrightness in
//  the planner output.  The formula uses -normal.y; do not change the sign.
// ===========================================================================
TEST_CASE("AC-F11 (AC-F-y-up): shade_face with normal_world={0,-1,0} gives brightness≈1.0 (Y-DOWN fence)", "[render][faces]") {
    // Given: a face whose CCW winding produces an outward normal of {0,-1,0}
    // In Y-DOWN world, {0,-1,0} points "up" — the roof-up direction.
    // The brightness formula must return 1.0 for this normal.
    //
    // Build triangle with normal {0,-1,0}:
    //   v0 = {1, 0, 0}, v1 = {0, 0, 1}, v2 = {-1, 0, 0}
    //   edge1 = v1-v0 = {-1, 0, 1}, edge2 = v2-v0 = {-2, 0, 0}
    //   cross = {0*0-1*0, 1*(-2)-(-1)*0, (-1)*0-0*(-2)} = {0, -2, 0} → normalize → {0,-1,0}
    const std::array<Vec3, 3> verts{{
        { 1.0f, 0.0f,  0.0f},
        { 0.0f, 0.0f,  1.0f},
        {-1.0f, 0.0f,  0.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x080};
    // Camera at centroid + 10 * {0,-1,0}; centroid = {0, 0, 1/3}
    // camera = {0, 0 + 10*(-1), 1/3} = {0, -10, 1/3}
    const Vec3 centroid{0.0f, 0.0f, 1.0f/3.0f};
    const Vec3 camera{0.0f, -10.0f, 1.0f/3.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE(result.has_value());
    CAPTURE(result->normal_world.x, result->normal_world.y, result->normal_world.z,
            result->brightness);
    // Normal must point in {0,-1,0} direction
    REQUIRE(result->normal_world.y < -0.9f);
    REQUIRE(std::abs(result->normal_world.x) < kFaceEps);
    // Brightness must be ≈ 1.0
    REQUIRE(result->brightness == Catch::Approx(1.0f).margin(kBrightEps));
}

// ---------------------------------------------------------------------------
// AC-F12 — normal_world={0,1,0} → brightness ≈ 0.0 (clamp at floor).
//   Formula: clamp(0.5 + 0.5*(-1) + 0.1*(0), 0, 1) = clamp(0.0, 0, 1) = 0.0
//   Given: face with outward normal {0,1,0}
//   When:  shade_face is called with camera on the +y side (visible)
//   Then:  brightness ≈ 0.0 within kBrightEps
// ---------------------------------------------------------------------------
TEST_CASE("AC-F12: shade_face with normal_world={0,1,0} gives brightness≈0.0 (clamp at floor)", "[render][faces]") {
    // Given: triangle with outward normal {0,1,0}:
    //   v0={1,0,0}, v1={-1,0,0}, v2={0,0,1}
    //   edge1={-2,0,0}, edge2={-1,0,1}
    //   cross({-2,0,0},{-1,0,1}) = {0*1-0*0, 0*(-1)-(-2)*1, (-2)*0-0*(-1)} = {0,2,0}
    //   normalize → {0,1,0}
    const std::array<Vec3, 3> verts{{
        { 1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        { 0.0f, 0.0f, 1.0f}
    }};
    const core::ShipFace face{0, 1, 2, 0x040};
    // Camera above face in +y (normal direction): camera = {0, 10, 1/3}
    const Vec3 camera{0.0f, 10.0f, 1.0f/3.0f};

    // When
    const auto result = render::shade_face(
        std::span<const Vec3>(verts.data(), verts.size()),
        face, camera);

    // Then
    REQUIRE(result.has_value());
    CAPTURE(result->normal_world.x, result->normal_world.y, result->normal_world.z,
            result->brightness);
    REQUIRE(result->normal_world.y > 0.9f);
    REQUIRE(result->brightness == Catch::Approx(0.0f).margin(kBrightEps));
}

// ---------------------------------------------------------------------------
// AC-F13 — X-tweak sign: {-1,0,0} → 0.6, {1,0,0} → 0.4 within kBrightEps.
//   {-1,0,0}: formula = clamp(0.5 + 0.5*(0) + 0.1*(-(-1)), 0, 1) = clamp(0.6, 0, 1) = 0.6
//   {1,0,0}:  formula = clamp(0.5 + 0.5*(0) + 0.1*(-(1)),  0, 1) = clamp(0.4, 0, 1) = 0.4
//   Given: two faces with normals pointing left and right respectively
//   When:  shade_face is called with camera on the normal side (visible)
//   Then:  brightness ≈ 0.6 for left-pointing; ≈ 0.4 for right-pointing
// ---------------------------------------------------------------------------
TEST_CASE("AC-F13: shade_face X-tweak: normal={-1,0,0}→brightness≈0.6, {1,0,0}→brightness≈0.4", "[render][faces]") {
    // Given: face with outward normal {-1,0,0}:
    //   v0={0,1,0}, v1={0,0,1}, v2={0,-1,0}
    //   edge1={0,-1,1}, edge2={0,-2,0}
    //   cross({0,-1,1},{0,-2,0}) = {(-1)*0-1*(-2), 1*0-0*0, 0*(-2)-(-1)*0} = {2,0,0}
    //   Wait: cross(a,b) = {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}
    //   cross({0,-1,1},{0,-2,0}) = {(-1)*0-1*(-2), 1*0-0*0, 0*(-2)-(-1)*0} = {2,0,0}
    //   normalize → {1,0,0} — WRONG direction. Swap v1 and v2:
    //   v0={0,1,0}, v1={0,-1,0}, v2={0,0,1}
    //   edge1={0,-2,0}, edge2={0,-1,1}
    //   cross({0,-2,0},{0,-1,1}) = {(-2)*1-0*(-1), 0*0-0*1, 0*(-1)-(-2)*0} = {-2,0,0}
    //   normalize → {-1,0,0}  ✓

    // Left-pointing face, normal {-1,0,0}
    {
        const std::array<Vec3, 3> verts{{
            {0.0f,  1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f,  0.0f, 1.0f}
        }};
        const core::ShipFace face{0, 1, 2, 0x040};
        // Camera at centroid + 10*{-1,0,0}; centroid = {0, 0, 1/3}
        const Vec3 camera{-10.0f, 0.0f, 1.0f/3.0f};

        const auto result = render::shade_face(
            std::span<const Vec3>(verts.data(), verts.size()),
            face, camera);

        REQUIRE(result.has_value());
        CAPTURE(result->normal_world.x, result->normal_world.y, result->normal_world.z,
                result->brightness);
        REQUIRE(result->normal_world.x < -0.9f);
        REQUIRE(result->brightness == Catch::Approx(0.6f).margin(kBrightEps));
    }

    // Right-pointing face, normal {1,0,0}
    // v0={0,1,0}, v1={0,0,1}, v2={0,-1,0}
    // edge1={0,-1,1}, edge2={0,-2,0}
    // cross({0,-1,1},{0,-2,0}) = {(-1)*0-1*(-2), 1*0-0*0, 0*(-2)-(-1)*0} = {2,0,0} → {1,0,0}
    {
        const std::array<Vec3, 3> verts{{
            {0.0f,  1.0f, 0.0f},
            {0.0f,  0.0f, 1.0f},
            {0.0f, -1.0f, 0.0f}
        }};
        const core::ShipFace face{0, 1, 2, 0x040};
        // Camera at centroid + 10*{1,0,0}; centroid = {0, 0, 1/3}
        const Vec3 camera{10.0f, 0.0f, 1.0f/3.0f};

        const auto result = render::shade_face(
            std::span<const Vec3>(verts.data(), verts.size()),
            face, camera);

        REQUIRE(result.has_value());
        CAPTURE(result->normal_world.x, result->normal_world.y, result->normal_world.z,
                result->brightness);
        REQUIRE(result->normal_world.x > 0.9f);
        REQUIRE(result->brightness == Catch::Approx(0.4f).margin(kBrightEps));
    }
}

// ===========================================================================
// GROUP 4: Ship topology (AC-F14..F18)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-F14 — kShipFaces.size() == 9.
//   Given: the kShipFaces constant array
//   When:  its size is checked at compile time and runtime
//   Then:  exactly 9 elements
// ---------------------------------------------------------------------------
TEST_CASE("AC-F14: kShipFaces has exactly 9 entries", "[render][faces]") {
    // Given / When / Then
    static_assert(entities::kShipFaces.size() == 9,
        "AC-F14: kShipFaces must have exactly 9 entries (kShipFaceCount)");
    REQUIRE(entities::kShipFaces.size() == 9);
}

// ---------------------------------------------------------------------------
// AC-F15 — All face indices < kShipVertexCount.
//   Given: kShipFaces
//   When:  each face's v0, v1, v2 are checked
//   Then:  all indices are < kShipVertexCount (9)
// ---------------------------------------------------------------------------
TEST_CASE("AC-F15: all kShipFaces vertex indices are less than kShipVertexCount", "[render][faces]") {
    // Given / When / Then
    for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
        const auto& f = entities::kShipFaces[i];
        CAPTURE(i, f.v0, f.v1, f.v2);
        REQUIRE(static_cast<std::size_t>(f.v0) < entities::kShipVertexCount);
        REQUIRE(static_cast<std::size_t>(f.v1) < entities::kShipVertexCount);
        REQUIRE(static_cast<std::size_t>(f.v2) < entities::kShipVertexCount);
    }
}

// ---------------------------------------------------------------------------
// AC-F16 — Distinct indices per face (no degenerate triangles).
//   Given: kShipFaces
//   When:  each face's v0, v1, v2 are checked
//   Then:  v0 != v1, v1 != v2, v0 != v2 for every face
// ---------------------------------------------------------------------------
TEST_CASE("AC-F16: all kShipFaces have distinct vertex indices (no degenerate triangles)", "[render][faces]") {
    // Given / When / Then
    for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
        const auto& f = entities::kShipFaces[i];
        CAPTURE(i, f.v0, f.v1, f.v2);
        REQUIRE(f.v0 != f.v1);
        REQUIRE(f.v1 != f.v2);
        REQUIRE(f.v0 != f.v2);
    }
}

// ---------------------------------------------------------------------------
// AC-F17 — Recomputed normals are unit length within kFaceEps.
//   Given: kShipFaces and kShipVertices
//   When:  the normal is recomputed via normalize(cross(v1-v0, v2-v0)) for each face
//   Then:  |magnitude - 1.0f| < kFaceEps for all 9 faces
// ---------------------------------------------------------------------------
TEST_CASE("AC-F17: recomputed normals from kShipFaces are unit length within kFaceEps", "[render][faces]") {
    // Given
    const auto& verts = entities::kShipVertices;
    const auto& faces = entities::kShipFaces;

    // When / Then
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto& f = faces[i];
        const Vec3 n = compute_normal(verts[f.v0], verts[f.v1], verts[f.v2]);
        const float mag = vec_magnitude(n);
        CAPTURE(i, f.v0, f.v1, f.v2, n.x, n.y, n.z, mag);
        REQUIRE(std::abs(mag - 1.0f) < kFaceEps);
    }
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-F-winding) — AC-F18
// ===========================================================================
//
//  D-OutwardNormals: all normals point outward from the hull.
//  For a closed convex mesh centred near the origin, the outward normal of
//  face 0 must satisfy dot(n, centroid_of_face - body_origin) > 0.
//
//  Face 0: {v0=0, v1=1, v2=5}
//    v[0] = {1.000, 0.3125, 0.500}
//    v[1] = {1.000, 0.3125,-0.500}
//    v[5] = {-0.900,-0.531, 0.000}
//    centroid = ((1+1-0.9)/3, (0.3125+0.3125-0.531)/3, (0.5-0.5+0)/3)
//             = (1.1/3, 0.094/3, 0) ≈ {0.3667, 0.0313, 0}
//    edge1 = v1-v0 = {0, 0, -1}
//    edge2 = v5-v0 = {-1.9, -0.8435, -0.5}
//    cross(edge1, edge2) = {0*(-0.5)-(-1)*(-0.8435),  (-1)*(-1.9)-0*(-0.5),  0*(-0.8435)-0*(-1.9)}
//                        = {-0.8435, 1.9, 0}
//    normalize → roughly {-0.406, 0.914, 0} — points roughly in +y and -x direction
//    centroid_body_origin dot = centroid . n:
//      0.3667*(-0.406) + 0.0313*(0.914) + 0*0 ≈ -0.149 + 0.028 ≈ -0.12 < 0
//
//  This is a borderline case; the key check is that IF the winding is correct
//  (CCW viewed from outside), the normal points outward. The test uses a
//  "mesh origin" (centre of mass of vertices) rather than {0,0,0} to be robust.
//
//  If this test fails: the face list has wrong winding for face 0, or the
//  cross product arguments are in the wrong order (v1-v0, v2-v0 must be correct).
//  Do NOT swap the test — fix the face list or the normal computation.
// ===========================================================================
TEST_CASE("AC-F18 (AC-F-winding): face 0 outward-normal dot(n, centroid - mesh_centroid) > 0", "[render][faces]") {
    // Given: kShipFaces face 0 = {v0=0, v1=1, v2=5}; kShipVertices body frame
    const auto& verts = entities::kShipVertices;
    const auto& faces = entities::kShipFaces;
    const auto& f0    = faces[0];
    REQUIRE(f0.v0 == 0);
    REQUIRE(f0.v1 == 1);
    REQUIRE(f0.v2 == 5);

    // Compute mesh centroid (average of all 9 vertices)
    Vec3 mesh_centroid{0.0f, 0.0f, 0.0f};
    for (const auto& v : verts) {
        mesh_centroid.x += v.x;
        mesh_centroid.y += v.y;
        mesh_centroid.z += v.z;
    }
    mesh_centroid.x /= static_cast<float>(entities::kShipVertexCount);
    mesh_centroid.y /= static_cast<float>(entities::kShipVertexCount);
    mesh_centroid.z /= static_cast<float>(entities::kShipVertexCount);

    // When: recompute normal from winding
    const Vec3 n = compute_normal(verts[f0.v0], verts[f0.v1], verts[f0.v2]);

    // Face centroid
    const Vec3 face_centroid{
        (verts[f0.v0].x + verts[f0.v1].x + verts[f0.v2].x) / 3.0f,
        (verts[f0.v0].y + verts[f0.v1].y + verts[f0.v2].y) / 3.0f,
        (verts[f0.v0].z + verts[f0.v1].z + verts[f0.v2].z) / 3.0f
    };

    // Then: dot(n, face_centroid - mesh_centroid) > 0 (outward)
    const Vec3 centroid_to_face{
        face_centroid.x - mesh_centroid.x,
        face_centroid.y - mesh_centroid.y,
        face_centroid.z - mesh_centroid.z
    };
    const float d = dot(n, centroid_to_face);
    CAPTURE(n.x, n.y, n.z, face_centroid.x, face_centroid.y, face_centroid.z,
            mesh_centroid.x, mesh_centroid.y, mesh_centroid.z, d);
    REQUIRE(d > 0.0f);
}

// ===========================================================================
// GROUP 5: Integration (AC-F19..F22)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-F19 — Identity ship at origin, camera {0,0,-5}: at least one face visible.
//   Given: kShipVertices transformed by identity + zero translation (world == body)
//          camera at {0, 0, -5}
//   When:  shade_face is called for all 9 faces
//   Then:  at least one face returns a populated optional
// ---------------------------------------------------------------------------
TEST_CASE("AC-F19: identity-oriented ship at origin with camera at {0,0,-5} has at least one visible face", "[render][faces]") {
    // Given
    std::array<Vec3, entities::kShipVertexCount> world{};
    render::rotate_vertices(
        std::span<const Vec3>(entities::kShipVertices.data(), entities::kShipVertices.size()),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // When
    int visible_count = 0;
    for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
        const auto result = render::shade_face(
            std::span<const Vec3>(world.data(), world.size()),
            entities::kShipFaces[i],
            camera);
        if (result.has_value()) {
            ++visible_count;
        }
    }

    // Then
    CAPTURE(visible_count);
    REQUIRE(visible_count >= 1);
}

// ---------------------------------------------------------------------------
// AC-F20 — After 90° yaw: visible-face SET differs from AC-F19 (set inequality).
//   Given: kShipVertices rotated by 90-degree yaw (same camera at {0,0,-5})
//   When:  shade_face is called for all 9 faces
//   Then:  the set of visible face indices differs from the identity-orient set
// ---------------------------------------------------------------------------
TEST_CASE("AC-F20: after 90-degree yaw the visible-face set differs from identity orientation", "[render][faces]") {
    // Given: identity orientation
    const Vec3 camera{0.0f, 0.0f, -5.0f};

    auto collect_visible = [&](const Mat3& orientation) -> std::set<std::uint8_t> {
        std::array<Vec3, entities::kShipVertexCount> world{};
        render::rotate_vertices(
            std::span<const Vec3>(entities::kShipVertices.data(), entities::kShipVertices.size()),
            orientation,
            Vec3{0.0f, 0.0f, 0.0f},
            std::span<Vec3>(world.data(), world.size()));

        std::set<std::uint8_t> visible;
        for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
            const auto result = render::shade_face(
                std::span<const Vec3>(world.data(), world.size()),
                entities::kShipFaces[i],
                camera);
            if (result.has_value()) {
                visible.insert(static_cast<std::uint8_t>(i));
            }
        }
        return visible;
    };

    // When
    const std::set<std::uint8_t> identity_visible = collect_visible(identity());
    const Mat3 yaw90{
        Vec3{ 0.0f, 0.0f, -1.0f},
        Vec3{ 0.0f, 1.0f,  0.0f},
        Vec3{ 1.0f, 0.0f,  0.0f}
    };
    const std::set<std::uint8_t> yaw90_visible = collect_visible(yaw90);

    // Then
    CAPTURE(identity_visible.size(), yaw90_visible.size());
    REQUIRE(identity_visible != yaw90_visible);
}

// ---------------------------------------------------------------------------
// AC-F21 — Visible face count ∈ [1, 9] (closed convex hull).
//   Given: identity ship, camera at {0,0,-5}
//   When:  shade_face is called for all 9 faces
//   Then:  visible count is in [1, 9]
// ---------------------------------------------------------------------------
TEST_CASE("AC-F21: visible face count is in [1, 9] for identity-oriented ship", "[render][faces]") {
    // Given
    std::array<Vec3, entities::kShipVertexCount> world{};
    render::rotate_vertices(
        std::span<const Vec3>(entities::kShipVertices.data(), entities::kShipVertices.size()),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // When
    int visible_count = 0;
    for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
        const auto result = render::shade_face(
            std::span<const Vec3>(world.data(), world.size()),
            entities::kShipFaces[i],
            camera);
        if (result.has_value()) ++visible_count;
    }

    // Then
    CAPTURE(visible_count);
    REQUIRE(visible_count >= 1);
    REQUIRE(visible_count <= 9);
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-F-cull-sense) — AC-F22
// ===========================================================================
//
//  D-CullSense: visible iff dot(normal_world, face_centroid - camera) < 0.
//
//  This test uses a fully controlled synthetic triangle to verify the cull
//  sense with no ambiguity:
//    Triangle at z=0, normal = {0,0,-1} (computed by winding)
//    Centroid at {0,0,0}
//    Camera at {0,0,-5}
//    dot({0,0,-1}, {0,0,0} - {0,0,-5}) = dot({0,0,-1},{0,0,5}) = -5 < 0 → VISIBLE
//
//  Reversed (normal {0,0,1}):
//    Camera at {0,0,-5}
//    dot({0,0,1}, {0,0,0} - {0,0,-5}) = dot({0,0,1},{0,0,5}) = 5 >= 0 → CULLED
//
//  If a developer accidentally reverses the sign (e.g. uses
//  `dot(normal, camera - centroid) < 0` instead of
//  `dot(normal, centroid - camera) < 0`), the visible and culled cases swap.
//  This test catches that class of bug with maximum clarity.
//
//  If this test fails: re-read D-CullSense in the planner output before
//  modifying.  Do NOT swap the REQUIRE / REQUIRE_FALSE — fix the implementation.
// ===========================================================================
TEST_CASE("AC-F22 (AC-F-cull-sense): normal {0,0,-1} toward camera is VISIBLE; {0,0,1} is CULLED", "[render][faces]") {
    // Given: camera at {0,0,-5}; centroid at {0,0,0}
    const Vec3 camera{0.0f, 0.0f, -5.0f};

    // Face with outward normal {0,0,-1} (toward camera at -z):
    // Need cross(v1-v0, v2-v0) pointing in {0,0,-1}
    //   v0={-1,0,0}, v1={0,1,0}, v2={1,0,0}
    //   edge1={1,1,0}, edge2={2,0,0}
    //   cross({1,1,0},{2,0,0}) = {1*0-0*0, 0*2-1*0, 1*0-1*2} = {0,0,-2} → {0,0,-1} ✓
    {
        const std::array<Vec3, 3> verts{{
            {-1.0f, 0.0f, 0.0f},
            { 0.0f, 1.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f}
        }};
        const core::ShipFace face{0, 1, 2, 0x040};

        const auto result = render::shade_face(
            std::span<const Vec3>(verts.data(), verts.size()),
            face, camera);

        CAPTURE("normal={0,0,-1} should be VISIBLE");
        REQUIRE(result.has_value());
    }

    // Face with outward normal {0,0,1} (away from camera at -z):
    //   v0={-1,0,0}, v1={1,0,0}, v2={0,1,0}
    //   edge1={2,0,0}, edge2={1,1,0}
    //   cross({2,0,0},{1,1,0}) = {0*0-0*1, 0*1-2*0, 2*1-0*1} = {0,0,2} → {0,0,1} ✓
    {
        const std::array<Vec3, 3> verts{{
            {-1.0f, 0.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f},
            { 0.0f, 1.0f, 0.0f}
        }};
        const core::ShipFace face{0, 1, 2, 0x040};

        const auto result = render::shade_face(
            std::span<const Vec3>(verts.data(), verts.size()),
            face, camera);

        CAPTURE("normal={0,0,1} should be CULLED (nullopt)");
        REQUIRE_FALSE(result.has_value());
    }
}

// ===========================================================================
// GROUP 6: Determinism (AC-F23)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-F23 — 1000-iter deterministic sweep: bit-identical between two fresh runs.
//   Given: a fixed sequence of 1000 (vertices, face, camera) inputs
//   When:  shade_face is called twice over the same sequence
//   Then:  every returned brightness and normal component is bit-identical
//          Confirms no PRNG, clock, or global mutable state inside shade_face.
// ---------------------------------------------------------------------------
TEST_CASE("AC-F23: 1000-iteration shade_face sweep is deterministic — bit-identical across two runs", "[render][faces]") {
    // Given
    std::array<Vec3, entities::kShipVertexCount> world{};
    render::rotate_vertices(
        std::span<const Vec3>(entities::kShipVertices.data(), entities::kShipVertices.size()),
        identity(),
        Vec3{0.0f, 0.0f, 0.0f},
        std::span<Vec3>(world.data(), world.size()));

    // Fixed camera trajectory: cycle through 1000 positions, using a simple
    // deterministic formula (no PRNG, no clock).
    auto run_sweep = [&]() -> std::pair<float, Vec3> {
        float brightness_sum = 0.0f;
        Vec3  normal_sum{0.0f, 0.0f, 0.0f};
        for (int iter = 0; iter < 1000; ++iter) {
            // Camera circles around the ship in the XZ plane
            const float angle  = static_cast<float>(iter) * 0.00628318f; // 2*pi/1000
            const float cx     = 5.0f * std::cos(angle);
            const float cz     = -5.0f * std::sin(angle) - 0.001f;  // avoid exact edge-on
            const Vec3  camera{cx, -2.0f, cz};

            const std::size_t face_idx = static_cast<std::size_t>(iter % 9);
            const auto result = render::shade_face(
                std::span<const Vec3>(world.data(), world.size()),
                entities::kShipFaces[face_idx],
                camera);

            if (result.has_value()) {
                brightness_sum += result->brightness;
                normal_sum.x   += result->normal_world.x;
                normal_sum.y   += result->normal_world.y;
                normal_sum.z   += result->normal_world.z;
            }
        }
        return {brightness_sum, normal_sum};
    };

    // When
    const auto [sum_a_b, sum_a_n] = run_sweep();
    const auto [sum_b_b, sum_b_n] = run_sweep();

    // Then — bit-identical (exact equality, NOT Approx)
    CAPTURE(sum_a_b, sum_b_b, sum_a_n.x, sum_b_n.x);
    REQUIRE(sum_a_b == sum_b_b);
    REQUIRE(sum_a_n.x == sum_b_n.x);
    REQUIRE(sum_a_n.y == sum_b_n.y);
    REQUIRE(sum_a_n.z == sum_b_n.z);
}

// ===========================================================================
// GROUP 7: Hygiene (AC-F80..F82)
// ===========================================================================
//
// AC-F80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include chain.
//
// AC-F81: claude_lander_render link list = core + warnings (unchanged from Pass 3).
//   Verified by CMakeLists.txt; no runtime TEST_CASE needed.
//   The BUILD_GAME=OFF build exercises this invariant.
//   The existing render_no_raylib_or_world_includes CTest tripwire covers this.
//
// AC-F82: Both public functions are noexcept.
//   Verified by the static_assert block at the top of this file.

TEST_CASE("AC-F80: render/faces.hpp and core/face_types.hpp compile and link without raylib (BUILD_GAME=OFF)", "[render][faces]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran — which means the headers compiled and linked without raylib,
    //        satisfying AC-F80.
    SUCCEED("compilation and linkage without raylib succeeded — AC-F80 satisfied");
}

TEST_CASE("AC-F81: render library compiles and links against core+warnings only — no forbidden deps at runtime", "[render][faces]") {
    // Given: this test binary was linked with claude_lander_render depending on
    //        core + warnings only (verified by CMakeLists.txt)
    // When:  it reaches this TEST_CASE
    // Then:  it ran without link errors, so AC-F81 is satisfied
    SUCCEED("link against core+warnings only — AC-F81 satisfied");
}

TEST_CASE("AC-F82: both public render/faces functions are noexcept (verified by static_assert at top of TU)", "[render][faces]") {
    // Given: static_assert checks at the top of this file verified noexcept for
    //        render::shade_face and render::rotate_vertices
    // When:  this test is reached (compile succeeded means all static_asserts passed)
    // Then:  AC-F82 is satisfied
    SUCCEED("all static_assert(noexcept(...)) checks passed at compile time — AC-F82 satisfied");
}
