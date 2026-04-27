// src/game/render_frame.cpp -- Pass 14 raylib bridge implementation.
//
// Translates Drawable records into raylib calls.  This is the only
// game-layer .cpp that may include raylib (game_no_raylib tripwire
// excludes this file).

#include "game/render_frame.hpp"

#include <cstdio>

#include <raylib.h>

#include "render/hud.hpp"

namespace game {

namespace {

constexpr Color kSkyTop    {  16,  20,  60, 255 };
constexpr Color kSkyMid    {  90, 130, 180, 255 };
constexpr Color kGround    {  70,  50,  30, 255 };
constexpr Color kHudBg     {   0,   0,   0, 200 };
constexpr Color kHudText   { 240, 240, 240, 255 };
constexpr Color kFuelEmpty {  40,  40,  40, 255 };
constexpr Color kFuelFull  {  60, 200,  80, 255 };
constexpr Color kShadow    {   0,   0,   0, 180 };

// Map a 12-bit hex base colour from the bbcelite face table into raylib RGB.
// The original palette layout is not transcribed; we use a simple expansion
// that preserves the hue spread (red/green/blue nibble triples).
Color color_from_base(std::uint16_t base, float brightness) noexcept {
    const auto r = static_cast<unsigned char>(((base >> 8) & 0xF) * 17);
    const auto g = static_cast<unsigned char>(((base >> 4) & 0xF) * 17);
    const auto b = static_cast<unsigned char>(((base >> 0) & 0xF) * 17);
    const float k = brightness;
    return Color{
        static_cast<unsigned char>(static_cast<float>(r) * k),
        static_cast<unsigned char>(static_cast<float>(g) * k),
        static_cast<unsigned char>(static_cast<float>(b) * k),
        255
    };
}

void draw_triangle(Vec2 a, Vec2 b, Vec2 c, Color col, float sx, float sy) noexcept {
    const Vector2 va{ a.x * sx, a.y * sy };
    const Vector2 vb{ b.x * sx, b.y * sy };
    const Vector2 vc{ c.x * sx, c.y * sy };
    DrawTriangle(va, vb, vc, col);
}

}  // namespace

void render_frame(
    const GameState& state,
    const render::BinSorter<Drawable>& sorter,
    float scale_x,
    float scale_y,
    int hud_y_offset) noexcept {
    const int win_w = GetScreenWidth();
    const int win_h = GetScreenHeight();

    // Sky + ground backdrop.
    DrawRectangleGradientV(0, 0, win_w, win_h / 3,
                           kSkyTop, kSkyMid);
    DrawRectangle(0, win_h / 3, win_w, win_h - win_h / 3, kGround);

    // Walk the bin sorter back-to-front and emit draw calls.
    sorter.for_each_back_to_front([scale_x, scale_y](const Drawable& d) noexcept {
        const Color col = color_from_base(d.color, 0.85f);
        switch (d.kind) {
            case Drawable::Kind::Terrain:
            case Drawable::Kind::ShipFace:
            case Drawable::Kind::RockFace:
                draw_triangle(d.a, d.b, d.c, col, scale_x, scale_y);
                break;
            case Drawable::Kind::Shadow:
                draw_triangle(d.a, d.b, d.c, kShadow, scale_x, scale_y);
                break;
            case Drawable::Kind::Particle: {
                const Vector2 v{ d.p.x * scale_x, d.p.y * scale_y };
                DrawCircleV(v, 2.0f, col);
                break;
            }
        }
    });

    // HUD (upper logical 16px scaled to physical).
    const int hud_h = static_cast<int>(16.0f * scale_y);
    DrawRectangle(0, hud_y_offset, win_w, hud_h, kHudBg);

    const auto score_str = render::format_score(state.score);
    const auto ammo_str  = render::format_ammo(state.ammo);

    const int score_x_px = static_cast<int>(8.0f  * scale_x);
    const int score_y_px = static_cast<int>(2.0f  * scale_y) + hud_y_offset;
    const int fuel_x_px  = static_cast<int>(120.0f * scale_x);
    const int fuel_y_px  = static_cast<int>(4.0f  * scale_y) + hud_y_offset;
    const int fuel_w_px  = static_cast<int>(64.0f * scale_x);
    const int fuel_h_px  = static_cast<int>(8.0f  * scale_y);
    const int ammo_x_px  = static_cast<int>(220.0f * scale_x);
    const int ammo_y_px  = score_y_px;
    const int text_size  = static_cast<int>(12.0f * scale_y);

    DrawText(score_str.data(), score_x_px, score_y_px, text_size, kHudText);

    DrawRectangle(fuel_x_px, fuel_y_px, fuel_w_px, fuel_h_px, kFuelEmpty);
    const int fill_w = render::fuel_bar_width(state.fuel, fuel_w_px);
    DrawRectangle(fuel_x_px, fuel_y_px, fill_w, fuel_h_px, kFuelFull);

    DrawText(ammo_str.data(), ammo_x_px, ammo_y_px, text_size, kHudText);

    if (state.crashed) {
        const char* msg = "CRASHED -- Esc to quit";
        const int w = MeasureText(msg, text_size * 2);
        DrawText(msg, (win_w - w) / 2, win_h / 2 - text_size,
                 text_size * 2, RED);
    } else if (state.landed) {
        const char* msg = "LANDED";
        const int w = MeasureText(msg, text_size * 2);
        DrawText(msg, (win_w - w) / 2, win_h / 2 - text_size,
                 text_size * 2, GREEN);
    }
}

}  // namespace game
