#pragma once

// The two shader lookup tables a level superbundle carries. A map mod adds a
// row per material it introduces, aliasing a shader program and texture set the
// base already ships, so two mods' tables differ from the base only by the rows
// each added and can be combined exactly.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dingosdk::frostbite::shader {

inline constexpr std::uint32_t programLookupType = 0xE2E6955B;
inline constexpr std::uint32_t textureLookupType = 0x2D254A89;

struct Table {
    std::span<const std::byte> resource;
    std::span<const std::byte> resourceMeta;
};

// A row an edit added that names a program the base does not have (one the mod
// added to its own copy of the programs).
struct MissingProgram {
    std::size_t edit{};
    std::uint64_t key{};
    std::uint32_t program{};
    std::uint32_t programs{};  // how many the base has
};

struct MergedTable {
    std::vector<std::byte> resource;
    std::vector<std::byte> resourceMeta;
    std::size_t added{};       // rows taken from the edits
    std::size_t conflicts{};   // rows two edits disagreed on; the first won
    std::vector<MissingProgram> missing; // rows left out
};

// `resourceMeta` is 16 bytes: program size, relocation size, lookup size. The
// merged table keeps the base's programs and relocations, so a row an edit added
// is kept only when every program it names is one of the base's.
[[nodiscard]] MergedTable merge_program_lookup(const Table& base, std::span<const Table> edits);

[[nodiscard]] MergedTable merge_texture_lookup(const Table& base, std::span<const Table> edits);

} // namespace dingosdk::frostbite::shader
