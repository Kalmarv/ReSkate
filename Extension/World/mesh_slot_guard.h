#pragma once
#include <cstdint>

// A mesh whose section asks for a material its owner's material table does not have crashed the
// game as it requested the mesh's textures (Skate.exe+0x3f3b73e; Engine/Game/Build/<build>/mesh_slots.h).
// The game shares that table between meshes whose names hash alike, so a mesh can be given a
// table with fewer materials than it has. The guard points such a section at the first material
// before the textures are requested: it draws with the wrong material, where the game would have
// crashed.
namespace dingosdk::mesh_slot_guard {
// Checks this build's function and hooks it; once. False when the build differs (no guard).
bool install(std::uintptr_t base) noexcept;
}
