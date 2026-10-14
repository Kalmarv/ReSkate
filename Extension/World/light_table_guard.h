#pragma once
#include <cstdint>

// The game keeps each kind of light (PbrSpotLight, PbrSphereLight, ...) in a table of 8,192 and
// writes past its end, and then crashes, when a level makes more
// (Engine/Game/Build/<build>/light_tables.h): a big custom map, or two time-of-day layers of one
// loaded together for a moment. The guard has the game make no more lights of a kind once its
// table is close to full: those lights are not drawn, where the game would have crashed. A light
// left out stays out until its part of the level is loaded again.
namespace dingosdk::light_table_guard {
// Checks this build's code from each factory to its table and hooks the six factories; once.
// False when the build differs (no guard).
bool install(std::uintptr_t base) noexcept;
// How many lights were left out since the game started, all kinds together.
std::uint64_t left_out() noexcept;
}
