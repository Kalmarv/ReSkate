// DIAGNOSTIC ONLY (not for upstream): observe the shader-set registration that crashes at Skate.exe+0x3f7ceed.
// register(ctx, owner) at 0x3f71ac0 registers an owner's shader records (vector of 0x70-byte records at
// owner+0x38/+0x40; owner+0x18/+0x20 a serialized list of record-index groups). 0x3f72280 turns each group into
// record pointers (records + index * 0x70) and 0x3f7ce90 reads *(record + 0x40) for each, unchecked.
// Logs, without changing behaviour: per owner, its record count and blob size; any record inside the vector whose
// +0x40 is null; and any set item that lies outside the current owner's vector or has a null +0x40.
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Core/Platform/memory.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <format>
#include <bit>
#include <intrin.h>

namespace dingosdk::shader_set_probe {
namespace {
constexpr std::uintptr_t register_rva = 0x3f71ac0, keyset_rva = 0x3f7ce90;
constexpr std::array<unsigned char, 16> register_head{0x48,0x89,0x54,0x24,0x10,0x53,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x48};
constexpr std::array<unsigned char, 16> keyset_head{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57,0x41,0x55,0x41,0x56};
using Register = void (*)(std::uintptr_t, std::uintptr_t);
using Keyset = void (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
Register register_original{};
Keyset keyset_original{};
std::uintptr_t image{};
thread_local std::uintptr_t current_owner{};
std::atomic<std::uint64_t> owners{}, sets{}, outside{}, nulls{};

template <class... Args> void say(std::format_string<Args...> f, Args&&... a) noexcept {
    try { logging::log(logging::Level::warning, logging::Channel::runtime, f, std::forward<Args>(a)...); } catch (...) {}
}
std::uintptr_t q(std::uintptr_t a) noexcept { std::uintptr_t v{}; return memory::peek(a, v) ? v : 0xdeadull; }
std::string hex(std::uintptr_t a, std::size_t n) {
    std::string s;
    for (std::size_t o = 0; o < n; o += 8) s += std::format("{:016x} ", q(a + o));
    return s;
}

void register_hook(std::uintptr_t ctx, std::uintptr_t owner) {
    const auto begin = q(owner + 0x38), end = q(owner + 0x40);
    const auto count = end >= begin ? (end - begin) / 0x70 : 0;
    const auto n = owners.fetch_add(1) + 1;
    std::uint32_t blob_size{};
    (void)memory::read(owner + 0x20, blob_size);
    std::uint64_t bad{};
    for (std::uintptr_t r = begin; r < end && count < 100000; r += 0x70)
        if (!q(r + 0x40)) {
            if (++bad <= 4) say("SSP owner {:#x}: record {} of {} has +0x40 null: {}", owner, (r - begin) / 0x70, count, hex(r, 0x70));
        }
    if (n <= 64 || bad || std::has_single_bit(n)) say("SSP register #{} ctx {:#x} owner {:#x} vt {:#x} records {} [{:#x},{:#x}) blob {:#x}+{} null-in-range {} caller {:#x} owner bytes {}",
        n, ctx, owner, q(owner) - image, count, begin, end, q(owner + 0x18), blob_size, bad,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - image, hex(owner, 0x60));
    current_owner = owner;
    register_original(ctx, owner);
    current_owner = 0;
}

void keyset_hook(std::uintptr_t table, std::uintptr_t entry, std::uintptr_t vec) {
    sets.fetch_add(1);
    std::uint32_t n{};
    (void)memory::read(entry, n);
    const auto owner = current_owner;
    const auto begin = owner ? q(owner + 0x38) : 0, end = owner ? q(owner + 0x40) : 0;
    for (std::uint32_t i = 0; i < n && i < 4096; ++i) {
        const auto item = q(entry + 0x10 + 8ull * i);
        const bool inside = owner && item >= begin && item < end && (item - begin) % 0x70 == 0;
        const auto src = q(item + 0x40);
        if (owner && !inside) {
            const auto k = outside.fetch_add(1) + 1;
            if (k <= 16) say("SSP set item {}/{} {:#x} OUTSIDE owner {:#x} records [{:#x},{:#x}) ({} records); index {} ; +0x40 {:#x}; bytes {}",
                i, n, item, owner, begin, end, (end - begin) / 0x70,
                static_cast<std::int64_t>(item - begin) / 0x70, src, hex(item, 0x70));
        }
        if (!src) {
            const auto k = nulls.fetch_add(1) + 1;
            if (k <= 16) say("SSP set item {}/{} {:#x} has +0x40 null (inside={}, owner {:#x}, index {}, records {}); caller {:#x}; bytes {}; owner bytes {}; set {}",
                i, n, item, inside, owner, owner ? static_cast<std::int64_t>(item - begin) / 0x70 : -1,
                owner ? (end - begin) / 0x70 : 0, reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - image, hex(item, 0x70),
                owner ? hex(owner, 0x60) : std::string{}, hex(entry, 0x10 + 8ull * n));
        }
    }
    keyset_original(table, entry, vec);
}

bool hook(std::uintptr_t rva, const std::array<unsigned char, 16>& head, void* replacement, void** original, const char* what) {
    std::array<unsigned char, 16> got{};
    if (!memory::peek(image + rva, got) || got != head) { say("SSP: {} differs; not hooked", what); return false; }
    const auto t = reinterpret_cast<void*>(image + rva);
    if (hook_prepare(t, replacement, original) != HookOk || hook_enable(t) != HookOk) { say("SSP: {} hook failed", what); return false; }
    return true;
}
} // namespace

bool install(std::uintptr_t base) noexcept {
    image = base;
    const bool a = hook(register_rva, register_head, reinterpret_cast<void*>(&register_hook), reinterpret_cast<void**>(&register_original), "register");
    const bool b = hook(keyset_rva, keyset_head, reinterpret_cast<void*>(&keyset_hook), reinterpret_cast<void**>(&keyset_original), "keyset");
    say("SSP: shader set probe installed (register {}, keyset {})", a, b);
    return a && b;
}
} // namespace dingosdk::shader_set_probe
