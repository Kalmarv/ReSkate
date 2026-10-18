#include "mesh_slot_guard.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/mesh_slots.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>

namespace dingosdk::mesh_slot_guard {
namespace {
namespace slots = addr::mesh_slots;
using RequestTextures = void (*)(std::uint64_t, std::uint64_t, std::uintptr_t, std::uint64_t, std::uintptr_t,
                                 std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
RequestTextures original{};
std::atomic<bool> installed{};
std::atomic<std::uint64_t> pointed{};

template <class T> T at(std::uintptr_t address) noexcept {
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

// Every section the function will read whose material index is past the owner's table is pointed
// at the first material. How many; -1 when something could not be read (then nothing is changed
// past that point and the game goes on as it would have).
int point_into_table(std::uintptr_t state, std::uintptr_t draw, std::uintptr_t& mesh, std::uint32_t& count,
                     std::uint16_t& index) noexcept {
    int changed = 0;
    __try {
        const auto owner = at<std::uintptr_t>(state);
        count = at<std::uint32_t>(owner + slots::table_count);
        mesh = at<std::uintptr_t>(draw + slots::mesh);
        if (!count) return 0;
        const auto lods = at<std::uint16_t>(mesh + slots::lod_count);
        for (std::uint32_t lod = at<std::uint16_t>(draw + slots::first_lod); lod < lods; ++lod) {
            const auto record = at<std::uintptr_t>(mesh + slots::lods + 8 * lod);
            const auto sections = at<std::uintptr_t>(record + slots::sections);
            for (std::uint32_t list = 0; list < 3; ++list) {
                const auto items = at<const std::uint8_t*>(record + slots::list_items + slots::list_stride * list);
                const auto items_count = at<std::uint32_t>(record + slots::list_count + slots::list_stride * list);
                for (std::uint32_t item = 0; item < items_count; ++item) {
                    auto* const material = reinterpret_cast<std::uint16_t*>(
                        sections + static_cast<std::uintptr_t>(items[item]) * slots::section_size + slots::material_index);
                    if (*material < count) continue;
                    index = *material;
                    *material = 0;
                    ++changed;
                }
            }
        }
        return changed;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

// Said for the first few, then each power of two.
__declspec(noinline) void note(std::uintptr_t mesh, std::uint32_t count, std::uint16_t index, std::uint64_t total) noexcept {
    if (total > 8 && !std::has_single_bit(total)) return;
    try {
        std::array<char, 256> name{};
        std::uintptr_t text{};
        if (!memory::peek(mesh + slots::name, text) || memory::peek_cstring(text, name.data(), name.size()) < 0)
            name = {'?'};
        logging::log(logging::Level::warning, logging::Channel::runtime,
            "Meshes: {} asks for material {} of a material table with {}, as when another mesh's name hashes alike; it "
            "draws with the first material, where the game would have crashed ({} since the game started).",
            name.data(), index, count, total);
    } catch (...) {}
}

void guarded(std::uint64_t a, std::uint64_t b, std::uintptr_t state, std::uint64_t d, std::uintptr_t draw,
             std::uint64_t f, std::uint64_t g, std::uint64_t h, std::uint64_t i) {
    std::uintptr_t mesh{};
    std::uint32_t count{};
    std::uint16_t index{};
    if (const auto changed = point_into_table(state, draw, mesh, count, index); changed > 0)
        note(mesh, count, index, pointed.fetch_add(static_cast<std::uint64_t>(changed), std::memory_order_relaxed) + changed);
    original(a, b, state, d, draw, f, g, h, i);
}
} // namespace

bool install(std::uintptr_t base) noexcept {
    if (installed.exchange(true)) return original != nullptr;
    if (!base) return false;
    std::array<unsigned char, 32> head{};
    if (!memory::peek(base + slots::request_textures.rva, head) || head != slots::request_textures.bytes) {
        logging::log(logging::Level::warning, logging::Channel::runtime,
            "Mesh slot guard: this build's mesh texture code differs; it is left as it is.");
        return false;
    }
    const auto target = reinterpret_cast<void*>(base + slots::request_textures.rva);
    if (hook_prepare(target, reinterpret_cast<void*>(&guarded), reinterpret_cast<void**>(&original)) != HookOk ||
        hook_enable(target) != HookOk) {
        original = nullptr;
        logging::log(logging::Level::warning, logging::Channel::runtime, "Mesh slot guard: the game's function could not be hooked.");
        return false;
    }
    return true;
}
} // namespace dingosdk::mesh_slot_guard
