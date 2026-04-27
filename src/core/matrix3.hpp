// src/core/matrix3.hpp — Pass 1 stub header.
//
// Column-major 3x3 matrix following the bbcelite Lander convention:
//   col[0] = nose axis
//   col[1] = roof axis
//   col[2] = side axis
//
// All functions are constexpr and defined inline in the header (required by
// the C++ standard for constexpr to be usable in constant-expression
// contexts).  No .cpp counterpart is needed for this module; constexpr
// functions with bodies here ARE their definitions.
//
// The `at(row, col)` accessors interpret storage in column-major order:
//   element at (row r, column c) = col[c].{x,y,z}[r]
//   i.e. col[0].x = at(0,0), col[0].y = at(1,0), col[0].z = at(2,0)

#pragma once

#include "vec3.hpp"

// ---------------------------------------------------------------------------
// Mat3 struct — column-major
// ---------------------------------------------------------------------------

struct Mat3 {
    Vec3 col[3];  // col[0], col[1], col[2]
};

// ---------------------------------------------------------------------------
// Element accessors (row, col) — column-major storage
// ---------------------------------------------------------------------------

constexpr float& at(Mat3& m, int row, int col) noexcept {
    // col[c] is a Vec3; map row index to x/y/z
    switch (row) {
        case 0:  return m.col[col].x;
        case 1:  return m.col[col].y;
        default: return m.col[col].z;
    }
}

constexpr float at(const Mat3& m, int row, int col) noexcept {
    switch (row) {
        case 0:  return m.col[col].x;
        case 1:  return m.col[col].y;
        default: return m.col[col].z;
    }
}

// ---------------------------------------------------------------------------
// Factory / constructors
// ---------------------------------------------------------------------------

constexpr Mat3 identity() noexcept {
    return {
        Vec3{1.0f, 0.0f, 0.0f},   // col[0]
        Vec3{0.0f, 1.0f, 0.0f},   // col[1]
        Vec3{0.0f, 0.0f, 1.0f}    // col[2]
    };
}

// ---------------------------------------------------------------------------
// Determinant
// ---------------------------------------------------------------------------

constexpr float det(const Mat3& m) noexcept {
    // Cofactor expansion along first row
    float a00 = m.col[0].x, a10 = m.col[0].y, a20 = m.col[0].z;
    float a01 = m.col[1].x, a11 = m.col[1].y, a21 = m.col[1].z;
    float a02 = m.col[2].x, a12 = m.col[2].y, a22 = m.col[2].z;

    return a00 * (a11 * a22 - a12 * a21)
         - a01 * (a10 * a22 - a12 * a20)
         + a02 * (a10 * a21 - a11 * a20);
}

// ---------------------------------------------------------------------------
// Multiply — matrix * matrix
// ---------------------------------------------------------------------------

constexpr Mat3 multiply(const Mat3& a, const Mat3& b) noexcept {
    // result[j] = a * b.col[j]
    return {
        Vec3{
            at(a,0,0)*b.col[0].x + at(a,0,1)*b.col[0].y + at(a,0,2)*b.col[0].z,
            at(a,1,0)*b.col[0].x + at(a,1,1)*b.col[0].y + at(a,1,2)*b.col[0].z,
            at(a,2,0)*b.col[0].x + at(a,2,1)*b.col[0].y + at(a,2,2)*b.col[0].z
        },
        Vec3{
            at(a,0,0)*b.col[1].x + at(a,0,1)*b.col[1].y + at(a,0,2)*b.col[1].z,
            at(a,1,0)*b.col[1].x + at(a,1,1)*b.col[1].y + at(a,1,2)*b.col[1].z,
            at(a,2,0)*b.col[1].x + at(a,2,1)*b.col[1].y + at(a,2,2)*b.col[1].z
        },
        Vec3{
            at(a,0,0)*b.col[2].x + at(a,0,1)*b.col[2].y + at(a,0,2)*b.col[2].z,
            at(a,1,0)*b.col[2].x + at(a,1,1)*b.col[2].y + at(a,1,2)*b.col[2].z,
            at(a,2,0)*b.col[2].x + at(a,2,1)*b.col[2].y + at(a,2,2)*b.col[2].z
        }
    };
}

// ---------------------------------------------------------------------------
// Multiply — matrix * vector
// ---------------------------------------------------------------------------

constexpr Vec3 multiply(const Mat3& m, Vec3 v) noexcept {
    return {
        at(m,0,0)*v.x + at(m,0,1)*v.y + at(m,0,2)*v.z,
        at(m,1,0)*v.x + at(m,1,1)*v.y + at(m,1,2)*v.z,
        at(m,2,0)*v.x + at(m,2,1)*v.y + at(m,2,2)*v.z
    };
}

// ---------------------------------------------------------------------------
// Transpose
// ---------------------------------------------------------------------------

constexpr Mat3 transpose(const Mat3& m) noexcept {
    return {
        Vec3{ m.col[0].x, m.col[1].x, m.col[2].x },  // new col[0] = old row 0
        Vec3{ m.col[0].y, m.col[1].y, m.col[2].y },  // new col[1] = old row 1
        Vec3{ m.col[0].z, m.col[1].z, m.col[2].z }   // new col[2] = old row 2
    };
}
