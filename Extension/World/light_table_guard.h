#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The game keeps each kind of light (PbrSpotLight, PbrSphereLight, ...) in a table of 8,192 and
// crashes when a level makes more (Engine/Game/Build/<build>/light_tables.h). Once a kind's table is
// nearly full, the guard has the game make no more lights of that kind: they are not drawn, where
// the game would have crashed.
namespace dingosdk::light_table_guard {
// Checks this build's functions and hooks the six light factories; once. False when the build
// differs (no guard).
bool install(std::uintptr_t base) noexcept;

inline constexpr std::size_t kinds = 6; // OldPoint, OldSpot, PbrSphere, PbrSpot, PbrTube, PbrRectangular
inline constexpr std::uint32_t capacity = 8192;
// One kind's table: IDs in use (lights alive, or gone but not yet given back) and IDs free.
struct Occupancy { std::uint32_t live{}, free{}; };
// Every kind's table now. Empty before the game makes its light renderer, when this build's light
// code differs, or when a table changed too fast to be read.
std::optional<std::array<Occupancy, kinds>> occupancy() noexcept;
}
