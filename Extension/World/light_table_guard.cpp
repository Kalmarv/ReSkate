#include "light_table_guard.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/light_tables.h"
#include "Engine/Game/World/light_table_room.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <utility>

namespace dingosdk::light_table_guard {
namespace {
namespace lt = addr::light_tables;
constexpr std::size_t kind_count = std::size(lt::kinds);
// factory(creator, creation info): the entity, or null for none.
using Factory = void* (*)(void*, void*);
std::array<Factory, kind_count> originals{};
std::uintptr_t image{};
std::atomic<bool> installed{};
std::array<std::atomic<std::uint64_t>, kind_count> refused{};
std::atomic<std::uint64_t> refused_total{};

// Whether the kind's table has room for one more, from its allocator as it is now. Yes when
// the renderer is not there yet or cannot be read: then it is not this guard's to say.
bool room(std::size_t kind) noexcept {
    std::uintptr_t renderer{};
    if (!memory::peek(image + lt::renderer, renderer) || !renderer) return true;
    const auto allocator = renderer + lt::kinds[kind].manager + lt::allocator;
    std::uint64_t free_head{};
    std::uint32_t issued{};
    if (!memory::peek(allocator + lt::free_head, free_head) || !memory::peek(allocator + lt::issued, issued)) return true;
    return light_table_room::room_for_one(free_head, issued);
}
// Said for the first few of each kind, then every thousandth: a level over the limit is over it
// for every light it still has to make.
__declspec(noinline) void note_refused(std::size_t kind) noexcept {
    refused_total.fetch_add(1, std::memory_order_relaxed);
    const auto count = refused[kind].fetch_add(1, std::memory_order_relaxed) + 1;
    if (count > 8 && count % 1000) return;
    try {
        logging::log(logging::Level::warning, logging::Channel::runtime,
            "Lights: the level has more {} lights than the game's table of them holds (8,192); this one is not made and "
            "is not drawn, where the game would have crashed ({} left out since the game started). A light left out stays "
            "out until its part of the level is loaded again.",
            lt::kinds[kind].name, count);
    } catch (...) {}
}
template <std::size_t Kind> void* guarded(void* creator, void* info) {
    if (!room(Kind)) {
        note_refused(Kind);
        return nullptr; // (as the game's own factory does for a light it should not make)
    }
    return originals[Kind](creator, info);
}
template <std::size_t... Kinds> std::array<void*, kind_count> make_replacements(std::index_sequence<Kinds...>) {
    return {reinterpret_cast<void*>(&guarded<Kinds>)...};
}

bool code_is_known(std::uintptr_t base) noexcept {
    for (const auto& kind : lt::kinds) {
        std::array<unsigned char, 32> head{};
        if (!memory::peek(base + kind.factory.rva, head) || head != kind.factory.bytes) return false;
    }
    for (const auto& site : lt::sites) {
        std::array<unsigned char, sizeof(site.bytes)> now{};
        if (site.size > now.size() || !memory::peek_bytes(base + site.rva, now.data(), site.size) ||
            std::memcmp(now.data(), site.bytes, site.size) != 0)
            return false;
    }
    return true;
}
void say(logging::Level level, const char* text) noexcept {
    try {
        logging::log(level, logging::Channel::runtime, "Light table guard: {}", text);
    } catch (...) {}
}
} // namespace

bool install(std::uintptr_t base) noexcept {
    if (installed.exchange(true)) return originals[0] != nullptr;
    if (!base) return false;
    if (!code_is_known(base)) {
        say(logging::Level::warning, "this build's light code differs; it is left as it is.");
        return false;
    }
    image = base;
    static_assert(kind_count == 6);
    const auto replacements = make_replacements(std::make_index_sequence<kind_count>{});
    // All six in one go, or none.
    const auto target = [base](std::size_t kind) { return reinterpret_cast<void*>(base + lt::kinds[kind].factory.rva); };
    std::size_t prepared{};
    auto status = HookOk;
    for (; prepared < kind_count; ++prepared) {
        status = hook_prepare(target(prepared), replacements[prepared], reinterpret_cast<void**>(&originals[prepared]));
        if (status != HookOk) break;
    }
    for (std::size_t kind = 0; kind < kind_count && status == HookOk; ++kind) status = hook_queue_enable(target(kind));
    if (status == HookOk) status = hook_apply_queued();
    if (status != HookOk) {
        for (std::size_t kind = 0; kind < prepared; ++kind) hook_remove(target(kind));
        originals = {};
        say(logging::Level::warning, "the game's light factories could not be hooked; they are left as they are.");
        return false;
    }
    return true;
}

std::uint64_t left_out() noexcept { return refused_total.load(std::memory_order_relaxed); }
} // namespace dingosdk::light_table_guard
