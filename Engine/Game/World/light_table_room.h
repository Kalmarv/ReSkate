#pragma once
#include <cstdint>

// Whether one more light of a kind may be made: the game's table of each kind of light holds
// 8,192 and nothing checks it (Engine/Game/Build/<build>/light_tables.h). Read from the kind's
// ID allocator just before the entity is made; the ID itself is taken a little later, and other
// lights may take IDs in between, so the last IDs are left free:
//  - a fresh ID is given only while the counter is short of the last `reserve`;
//  - past that, only while a recycled ID waits on the free list, and never into the last
//    `reserve` / 2, so even that many lights made at the same moment stay inside the table.
// The lights the game makes without the guarded factories have the reserve to themselves.
namespace dingosdk::light_table_room {
inline constexpr std::uint32_t capacity = 64 * 128;
inline constexpr std::uint32_t reserve = 256;
inline constexpr std::uint32_t no_free_id = 0xffffffff;

// free_head: the free list's head as the allocator keeps it (its low 32 bits the ID, or
// no_free_id when the list is empty); issued: the fresh IDs given so far.
constexpr bool room_for_one(std::uint64_t free_head, std::uint32_t issued) noexcept {
    if (issued < capacity - reserve) return true;
    const bool recycled = static_cast<std::uint32_t>(free_head) != no_free_id;
    return recycled && issued < capacity - reserve / 2;
}
}
