// ClaudeLander -- Pass 14 game executable entry point.
//
// Drives the game loop:
//   1. Initialise raylib window (320x256 logical, scaled to physical).
//   2. Each frame:
//      a. Poll input (mouse + buttons -> game::FrameInput).
//      b. game::tick(state, input) advances the simulation.
//      c. game::build_drawables(state, camera, sorter) populates draw queue.
//      d. game::render_frame(state, sorter, ...) emits raylib calls.
//   3. Esc quits cleanly.

#include <cstdio>

#include <raylib.h>

#include "game/game_state.hpp"
#include "game/game_loop.hpp"
#include "game/render_frame.hpp"
#include "render/bin_sorter.hpp"
#include "world/camera_follow.hpp"

namespace {

constexpr int kLogicalWidth  = 320;
constexpr int kLogicalHeight = 256;
constexpr int kWindowScale   = 3;
constexpr int kWindowWidth   = kLogicalWidth  * kWindowScale;
constexpr int kWindowHeight  = kLogicalHeight * kWindowScale;
constexpr int kTargetFps     = 50;

// Mouse offset is measured from screen centre.
Vec2 mouse_offset_from_centre() noexcept {
    const Vector2 m = GetMousePosition();
    return Vec2{
        m.x - static_cast<float>(kWindowWidth)  * 0.5f,
        m.y - static_cast<float>(kWindowHeight) * 0.5f,
    };
}

}  // namespace

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kWindowWidth, kWindowHeight, "ClaudeLander");
    SetTargetFPS(kTargetFps);
    HideCursor();

    game::GameState state{};
    render::BinSorter<game::Drawable> sorter;

    const float scale_x = static_cast<float>(kWindowWidth)  / static_cast<float>(kLogicalWidth);
    const float scale_y = static_cast<float>(kWindowHeight) / static_cast<float>(kLogicalHeight);

    while (!WindowShouldClose()) {
        // 1. Read input.
        game::FrameInput input;
        input.mouse_offset = mouse_offset_from_centre();
        input.thrust_full  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        input.thrust_half  = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        input.fire         = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

        // 2. Advance simulation (pure of raylib).
        game::tick(state, input);

        // 3. Build draw queue.
        const Vec3 camera_pos = world::follow_camera_position(state.ship.position);
        game::build_drawables(state, camera_pos, sorter);

        // 4. Render.
        BeginDrawing();
        game::render_frame(state, sorter, scale_x, scale_y, /*hud_y_offset=*/0);
        DrawFPS(kWindowWidth - 96, kWindowHeight - 24);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
