// src/core/face_types.hpp -- Pass 6 face POD shared between core and render/entities.
//
// Defines core::ShipFace, the smallest possible per-face record:
//   - three vertex indices (uint8_t each; mesh has 9 vertices, fits comfortably)
//   - one base_colour (uint16_t; stored as-is from the spec table, e.g. 0x080)
//
// This header is intentionally POD-only with no behaviour and no constructor.
// Aggregate initialisation is the only documented construction path so the
// inline constexpr kShipFaces table in entities/ship.hpp can be a constant
// expression.
//
// Boundary: this header has NO dependencies beyond <cstdint>.  It MUST NOT
// pull in raylib, world/, entities/, render/, <random>, or <chrono>.  The
// AC-F80 static_assert in tests/test_faces.cpp checks RAYLIB_VERSION is not
// defined after including this header.

#pragma once

#include <cstdint>

namespace core {

struct ShipFace {
    std::uint8_t  v0;
    std::uint8_t  v1;
    std::uint8_t  v2;
    std::uint16_t base_colour;
};

}  // namespace core
