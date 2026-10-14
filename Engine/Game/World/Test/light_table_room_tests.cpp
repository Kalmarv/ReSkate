// The light table guard (Extension/World/light_table_guard.cpp): when one more light of a kind
// may be made, and, with a Skate folder, that the game's code is what the guard was written for.
#include "Engine/Game/World/light_table_room.h"
#include "Engine/Game/Build/20260929/light_tables.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char* what) {
    if (!condition) { std::cerr << "FAILED: " << what << "\n"; ++failures; }
}
using namespace dingosdk::light_table_room;
constexpr std::uint64_t empty = no_free_id, waiting = (std::uint64_t{7} << 32) | 1234; // (a tag above the ID)

void the_room_rule() {
    check(capacity == 8192, "64 pages of 128");
    check(room_for_one(empty, 0) && room_for_one(empty, capacity - reserve - 1), "fresh IDs while short of the reserve");
    check(!room_for_one(empty, capacity - reserve), "no fresh ID into the reserve");
    check(room_for_one(waiting, capacity - reserve) && room_for_one(waiting, capacity - reserve / 2 - 1),
        "a recycled ID while one waits, past the reserve");
    check(!room_for_one(waiting, capacity - reserve / 2), "never into the last half of the reserve");
    check(!room_for_one(empty, capacity) && !room_for_one(waiting, capacity) && !room_for_one(waiting, 0xffffffff),
        "nothing once the table is full");
    check(room_for_one(std::uint64_t{0xffffffff} << 32, capacity - reserve), "ID 0 waiting is an ID (only the low half says none)");
    // Whatever the counter, when the guard says yes, reserve / 2 lights let through at that same
    // moment can each take a fresh ID (the recycled one gone to another) and all stay inside.
    bool inside = true;
    for (std::uint32_t issued = 0; issued <= capacity + 16; ++issued)
        for (const auto head : {empty, waiting})
            if (room_for_one(head, issued) && issued + reserve / 2 > capacity) inside = false;
    check(inside, "a burst of reserve / 2 after any yes stays inside the table");
}

// The image as the loader lays it out, enough to read code by RVA.
struct Image {
    std::vector<char> file;
    struct Section { std::uint32_t rva, size, raw, raw_size; };
    std::vector<Section> sections;
    std::optional<std::uint32_t> offset(std::uint32_t rva, std::uint32_t size) const {
        for (const auto& s : sections)
            if (rva >= s.rva && rva + size <= s.rva + std::min(s.size, s.raw_size)) return s.raw + (rva - s.rva);
        return std::nullopt;
    }
    bool same(std::uint32_t rva, const unsigned char* bytes, std::uint32_t size) const {
        const auto at = offset(rva, size);
        return at && *at + size <= file.size() && std::memcmp(file.data() + *at, bytes, size) == 0;
    }
};
template <class T> T read(const std::vector<char>& file, std::size_t at) {
    T value{};
    if (at + sizeof(T) <= file.size()) std::memcpy(&value, file.data() + at, sizeof(T));
    return value;
}
std::optional<Image> load(const std::filesystem::path& exe) {
    std::ifstream in(exe, std::ios::binary);
    if (!in) return std::nullopt;
    Image image;
    image.file.assign(std::istreambuf_iterator<char>(in), {});
    const auto pe = read<std::uint32_t>(image.file, 0x3c);
    if (read<std::uint32_t>(image.file, pe) != 0x4550) return std::nullopt;
    const auto count = read<std::uint16_t>(image.file, pe + 6);
    const auto table = pe + 24 + read<std::uint16_t>(image.file, pe + 20);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = table + i * 40;
        image.sections.push_back({read<std::uint32_t>(image.file, at + 12), read<std::uint32_t>(image.file, at + 8),
            read<std::uint32_t>(image.file, at + 20), read<std::uint32_t>(image.file, at + 16)});
    }
    return image;
}

void the_games_code(const char* game_root) {
    namespace lt = dingosdk::game::build::v20260929::light_tables;
    const auto image = load(std::filesystem::path(game_root) / "Skate.exe");
    check(image.has_value(), "Skate.exe read");
    if (!image) return;
    for (const auto& kind : lt::kinds)
        check(image->same(static_cast<std::uint32_t>(kind.factory.rva), kind.factory.bytes.data(), 32), kind.name);
    unsigned matched{};
    for (const auto& site : lt::sites) {
        if (image->same(site.rva, site.bytes, site.size)) { ++matched; continue; }
        std::cerr << "FAILED: the code at 0x" << std::hex << site.rva << std::dec << " is not what the guard was written for\n";
        ++failures;
    }
    check(matched == std::size(lt::sites), "every site of the light tables");
    std::cout << matched << " light table site(s) matched in " << game_root << "\n";
}
} // namespace

int main(int argc, char** argv) {
    the_room_rule();
    if (argc > 1 && *argv[1]) the_games_code(argv[1]);
    if (failures) return 1;
    std::cout << "Light table tests passed.\n";
    return 0;
}
