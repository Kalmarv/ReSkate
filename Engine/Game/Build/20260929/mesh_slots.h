#pragma once
#include "Engine/Game/Build/fingerprint.h"
#include <cstdint>

// A mesh's texture requests (Extension/World/mesh_slot_guard.cpp). For each section the function
// reads the entry of its owner's material table at the section's material index; nothing checks
// the index against the table's count.
namespace dingosdk::game::build::v20260929::mesh_slots {
// request_textures(?, ?, state, ?, draw, ...): the owner is [state]; the mesh set [draw + mesh].
inline constexpr Fingerprint request_textures{0x3f3b180, {
    0x4c,0x8b,0xdc,0x55,0x53,0x49,0x8d,0xab,0xf8,0xfe,0xff,0xff,0x48,0x81,0xec,0xf8,
    0x01,0x00,0x00,0x48,0x8b,0x05,0x26,0x92,0x28,0x03,0x48,0x33,0xc4,0x48,0x89,0x85}};
// The owner's material table and how many entries it has.
inline constexpr std::uint32_t table = 0x20, table_count = 0x28;
// The draw: its mesh set and the first LOD it draws (u16).
inline constexpr std::uint32_t mesh = 0x28, first_lod = 0x44;
// The mesh set: its LODs (pointers), how many (u16), its name.
inline constexpr std::uint32_t lods = 0x20, lod_count = 0xa4, name = 0x58;
// A LOD: its sections (an unaligned pointer), and three lists of section numbers (bytes), each a
// count and a pointer, 12 bytes apart.
inline constexpr std::uint32_t sections = 0x0c, list_count = 0x14, list_items = 0x18, list_stride = 12;
inline constexpr std::uint32_t section_size = 0x180, material_index = 0x1c;   // (u16)
}
