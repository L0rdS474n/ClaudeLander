// ClaudeLander -- Pass 0 bootstrap smoke-test.
//
// Opens a window, draws a gradient sky over a flat horizon line, prints a
// version banner, and exits cleanly on Esc. The only thing this proves is
// that the CMake + raylib + FetchContent toolchain is wired up. Real game
// logic arrives in Pass 1+ (see docs/ARCHITECTURE.md once written).

#include <raylib.h>

namespace {

// Logical render size matches the original Acorn Archimedes Mode 13.
// The window is rendered at a multiple of this to keep the aspect ratio.
constexpr int kLogicalWidth  = 320;
constexpr int kLogicalHeight = 256;
constexpr int kWindowScale   = 3;
constexpr int kWindowWidth   = kLogicalWidth  * kWindowScale;   // 960
constexpr int kWindowHeight = kLogicalHeight * kWindowScale;    // 768

constexpr int kTargetFps = 60;

}  // namespace

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kWindowWidth, kWindowHeight, "ClaudeLander -- Pass 0 bootstrap");
    SetTargetFPS(kTargetFps);

    while (!WindowShouldClose()) {
        BeginDrawing();

        // Sky gradient (deep blue at the top, lighter near the horizon).
        DrawRectangleGradientV(
            0, 0,
            kWindowWidth, kWindowHeight / 2,
            Color{ 16,  20,  60, 255 },
            Color{ 90, 130, 180, 255 });

        // Ground placeholder (uniform brown).
        DrawRectangle(
            0, kWindowHeight / 2,
            kWindowWidth, kWindowHeight / 2,
            Color{ 70, 50, 30, 255 });

        // Horizon line.
        DrawLine(
            0, kWindowHeight / 2,
            kWindowWidth, kWindowHeight / 2,
            Color{ 200, 200, 200, 255 });

        DrawText("ClaudeLander", 24, 24, 32, RAYWHITE);
        DrawText("Pass 0 -- bootstrap smoke test", 24, 64, 18, LIGHTGRAY);
        DrawText("Esc to quit", 24, kWindowHeight - 32, 16, LIGHTGRAY);
        DrawFPS(kWindowWidth - 96, 8);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
