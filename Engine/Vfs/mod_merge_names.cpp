#include "mod_merge_internal.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <map>
#include <tuple>
#include <unordered_map>

namespace dingosdk::mods::detail {
namespace {

// The game finds an EBX or resource by this hash of its name alone (ASCII case folded), with no
// string compare; a mesh's material tables are shared by every mesh with the same low 32 bits.
constexpr std::uint64_t engine_hash(std::string_view name) noexcept {
    std::uint64_t value = 5381;
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        value = value * 33 ^ static_cast<unsigned char>(c);
    }
    return value;
}
constexpr std::uint32_t meshSetType = 0x49B156D4;

// The map a name belongs to ("dingolevel_reskate_<map>" in its path), or empty for the game's and
// other shared assets. Two maps never load together, so only names of one map, or of a map and
// what is shared, can be taken for one another.
std::string_view scope(std::string_view name) noexcept {
    const auto at = name.find("dingolevel_reskate_");
    if (at == std::string_view::npos) return {};
    return name.substr(at, name.find('/', at) - at);
}
bool together(std::string_view a, std::string_view b) noexcept {
    const auto x = scope(a), y = scope(b);
    return x.empty() || y.empty() || x == y;
}
constexpr std::size_t shown = 3;   // pairs named per mod and kind

// One name a mod's bundle lists: its engine hash, a second hash that tells names apart, and for a
// resource its id. The name itself stays with its bundle, looked at again only when it clashes.
struct Entry {
    std::uint64_t hash{}, second{}, id{};
    std::uint32_t bundle{}, text{};
    bool mesh{}, resource{};
};
struct BundleNames {
    std::vector<std::string> text;   // lower case
    std::vector<Entry> entries;
};

void read_bundle(BundleNames& out, std::uint32_t index, const fs::path& root, const fb::TocBundle& bundle,
                 const CasStore& store, const fs::path& baseRoot, const fs::path& gameRoot) {
    const auto listing = list_bundle(store, root, baseRoot, bundle, gameRoot);
    if (!listing) return;   // (the merge notes what it cannot read)
    const auto add = [&](const fb::BundleAsset& asset, bool resource) {
        auto& name = out.text.emplace_back(lower(asset.name));
        out.entries.push_back({engine_hash(name), name_hash(name), resource ? asset.resourceId : 0, index,
                               static_cast<std::uint32_t>(out.text.size() - 1), resource && asset.resourceType == meshSetType,
                               resource});
    };
    for (const auto& asset : listing->manifest.ebx) add(asset, false);
    for (const auto& asset : listing->manifest.resources) add(asset, true);
}

// The game's own names, as (engine hash, a second hash of the name) so a mod's copy of a game asset
// is told from a different name: every EBX and resource, and apart the mesh sets' low 32 bits.
struct GameNames {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> all;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> meshes;
};
constexpr char gameNamesMagic[8] = {'R', 'S', 'N', 'A', 'M', 'E', 'S', '1'};

std::string game_stamp(const fs::path& baseRoot) {
    std::error_code error;
    const auto layout = baseRoot / L"layout.toc";
    const auto size = fs::file_size(layout, error);
    const auto time = fs::last_write_time(layout, error).time_since_epoch().count();
    return std::to_string(size) + ' ' + std::to_string(time);
}

std::optional<GameNames> read_cache(const fs::path& file, const std::string& stamp) {
    std::ifstream in(file, std::ios::binary);
    char magic[8]{};
    std::uint32_t stampSize{};
    if (!in.read(magic, 8) || std::memcmp(magic, gameNamesMagic, 8) || !in.read(reinterpret_cast<char*>(&stampSize), 4) ||
        stampSize != stamp.size())
        return std::nullopt;
    std::string held(stampSize, '\0');
    if (!in.read(held.data(), stampSize) || held != stamp) return std::nullopt;
    GameNames names;
    for (auto* list : {&names.all, &names.meshes}) {
        std::uint64_t count{};
        if (!in.read(reinterpret_cast<char*>(&count), 8) || count > (64u << 20)) return std::nullopt;
        list->resize(count);
        if (!in.read(reinterpret_cast<char*>(list->data()), static_cast<std::streamsize>(count * sizeof((*list)[0]))))
            return std::nullopt;
    }
    return names;
}

void write_cache(const fs::path& file, const std::string& stamp, const GameNames& names) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    const auto stampSize = static_cast<std::uint32_t>(stamp.size());
    out.write(gameNamesMagic, 8);
    out.write(reinterpret_cast<const char*>(&stampSize), 4);
    out.write(stamp.data(), stampSize);
    for (const auto* list : {&names.all, &names.meshes}) {
        const std::uint64_t count = list->size();
        out.write(reinterpret_cast<const char*>(&count), 8);
        out.write(reinterpret_cast<const char*>(list->data()), static_cast<std::streamsize>(count * sizeof((*list)[0])));
    }
}

GameNames read_game(const fs::path& baseRoot, const fs::path& gameRoot, const CasStore& store, std::size_t threads,
                    bool background) {
    const auto files = scan(baseRoot);
    std::vector<GameNames> found(files.tocs.size());
    in_parallel(files.tocs.size(), threads, background, [&](std::size_t index) {
        auto& names = found[index];
        const auto toc = fb::read_toc(read_file(baseRoot / fs::path(files.tocs[index])));
        for (const auto& bundle : toc.bundles) {
            const auto listing = list_bundle(store, baseRoot, baseRoot, bundle, gameRoot);
            if (!listing) continue;
            const auto add = [&](const fb::BundleAsset& asset) {
                const auto name = lower(asset.name);
                const auto second = name_hash(name);
                names.all.emplace_back(engine_hash(name), second);
                if (asset.kind == fb::AssetKind::resource && asset.resourceType == meshSetType)
                    names.meshes.emplace_back(engine_hash(name) & 0xFFFFFFFF, second);
            };
            for (const auto& asset : listing->manifest.ebx) add(asset);
            for (const auto& asset : listing->manifest.resources) add(asset);
        }
    });
    GameNames names;
    for (auto& part : found) {
        names.all.insert(names.all.end(), part.all.begin(), part.all.end());
        names.meshes.insert(names.meshes.end(), part.meshes.begin(), part.meshes.end());
    }
    for (auto* list : {&names.all, &names.meshes}) {
        std::ranges::sort(*list);
        list->erase(std::unique(list->begin(), list->end()), list->end());
    }
    return names;
}

// True when a game asset has this hash under another name.
bool game_clash(const std::vector<std::pair<std::uint64_t, std::uint64_t>>& game, std::uint64_t hash, std::uint64_t second) {
    auto at = std::ranges::lower_bound(game, std::pair{hash, std::uint64_t{}});
    for (; at != game.end() && at->first == hash; ++at)
        if (at->second != second) return true;
    return false;
}

struct Findings {
    std::size_t count{};
    std::vector<std::string> examples;
    void add(std::string example) {
        if (count++ < shown) examples.push_back(std::move(example));
    }
};

} // namespace

std::vector<std::string> check_names(const std::vector<const Mod*>& mods, const std::map<const Mod*, RelativeFiles>& modFiles,
                                     const fs::path& dataRoot, const fs::path& output, const fs::path& cache,
                                     std::size_t threads, bool background) {
    const auto baseRoot = dataRoot / L"Data";
    const auto layout = vfs::read_layout(baseRoot / L"layout.toc");
    const CasStore store(baseRoot, output, layout.root);

    // Bundle by bundle on several threads (one map can have thousands), each mod's TOCs read first.
    std::vector<fb::TocDocument> tocs;
    std::vector<std::pair<std::size_t, const fb::TocBundle*>> bundles;   // (mod, bundle)
    for (std::size_t index = 0; index < mods.size(); ++index)
        if (const auto files = modFiles.find(mods[index]); files != modFiles.end())
            for (const auto& relative : files->second.tocs) try {
                tocs.push_back(fb::read_toc(read_file(mods[index]->directory / fs::path(relative))));
            } catch (const std::exception&) {
                tocs.emplace_back();   // (the merge leaves a mod out whose TOC cannot be read)
            }
    for (std::size_t index = 0, toc = 0; index < mods.size(); ++index)
        if (const auto files = modFiles.find(mods[index]); files != modFiles.end())
            for (std::size_t count = files->second.tocs.size(); count; --count, ++toc)
                for (const auto& bundle : tocs[toc].bundles) bundles.emplace_back(index, &bundle);
    std::vector<BundleNames> found(bundles.size());
    in_parallel(bundles.size(), threads, background, [&](std::size_t index) {
        read_bundle(found[index], static_cast<std::uint32_t>(index), mods[bundles[index].first]->directory,
                    *bundles[index].second, store, baseRoot, dataRoot);
    });
    const auto text = [&](const Entry& entry) -> const std::string& { return found[entry.bundle].text[entry.text]; };
    const auto mod_of = [&](const Entry& entry) -> const std::string& { return mods[bundles[entry.bundle].first]->name; };

    const auto stamp = game_stamp(baseRoot);
    auto game = read_cache(cache, stamp);
    if (!game) {
        game = read_game(baseRoot, dataRoot, store, threads, background);
        std::error_code error;
        try { write_cache(cache, stamp, *game); } catch (const std::exception&) { fs::remove(cache, error); }
    }

    // Each mod's distinct names (one name is often in many of its bundles), and its resource names
    // listed under two ids; a mod by itself on each thread.
    std::map<std::string, Findings> names, meshes, resources;
    std::vector<std::vector<Entry>> own(mods.size()), twice(mods.size());
    in_parallel(mods.size(), threads, background, [&](std::size_t mod) {
        std::vector<Entry> all;
        for (std::size_t index = 0; index < bundles.size(); ++index)
            if (bundles[index].first == mod) all.insert(all.end(), found[index].entries.begin(), found[index].entries.end());
        // (An EBX and a resource often share a name: a mesh's asset and its mesh set.)
        std::ranges::sort(all, {}, [](const Entry& entry) { return std::tuple{entry.second, entry.resource, entry.id}; });
        for (std::size_t at = 0; at < all.size();) {
            auto end = at + 1;
            while (end < all.size() && all[end].second == all[at].second && all[end].resource == all[at].resource) ++end;
            // (Sorted by id within the name: a resource listed under two ids has its first and last differ.)
            if (all[at].resource && all[at].id != all[end - 1].id) twice[mod].push_back(all[at]);
            own[mod].push_back(all[at]);
            at = end;
        }
    });
    std::vector<Entry> distinct;
    for (std::size_t mod = 0; mod < mods.size(); ++mod) {
        for (const auto& entry : twice[mod]) resources[mods[mod]->name].add(text(entry) + " (in this mod)");
        distinct.insert(distinct.end(), own[mod].begin(), own[mod].end());
    }
    // One name with two resource ids across mods, and two names with one hash: compared in sorted runs.
    const auto runs = [&](std::vector<Entry>& list, auto key, auto pair) {
        std::ranges::sort(list, {}, [&](const Entry& entry) { return std::tuple{key(entry), entry.second}; });
        for (std::size_t at = 0; at < list.size();) {
            auto end = at + 1;
            while (end < list.size() && key(list[end]) == key(list[at])) ++end;
            for (auto later = at + 1; later < end; ++later)
                for (auto earlier = at; earlier < later; ++earlier)
                    if (pair(list[earlier], list[later])) break;
            at = end;
        }
    };
    std::vector<Entry> resourceList, meshList;
    for (const auto& entry : distinct) {
        if (entry.resource) resourceList.push_back(entry);
        if (entry.mesh) meshList.push_back(entry);
    }
    runs(resourceList, [](const Entry& entry) { return entry.second; }, [&](const Entry& earlier, const Entry& later) {
        if (earlier.id == later.id || mod_of(earlier) == mod_of(later)) return false;
        resources[mod_of(later)].add(text(later) + " (" + mod_of(earlier) + ')');
        return true;
    });
    const auto clashes = [&](std::map<std::string, Findings>& into, std::uint64_t mask) {
        return [&into, mask, &text, &mod_of](const Entry& earlier, const Entry& later) {
            if ((earlier.hash & mask) != (later.hash & mask) || earlier.second == later.second ||
                !together(text(earlier), text(later)))
                return false;
            into[mod_of(later)].add(text(later) + " / " + text(earlier) + " (" + mod_of(earlier) + ')');
            return true;
        };
    };
    runs(distinct, [](const Entry& entry) { return entry.hash; }, clashes(names, ~std::uint64_t{}));
    runs(meshList, [](const Entry& entry) { return entry.hash & 0xFFFFFFFF; }, clashes(meshes, 0xFFFFFFFF));
    // And a name the game has under its hash.
    for (const auto& entry : distinct) {
        if (game_clash(game->all, entry.hash, entry.second)) names[mod_of(entry)].add(text(entry) + " / a game asset");
        if (entry.mesh && game_clash(game->meshes, entry.hash & 0xFFFFFFFF, entry.second))
            meshes[mod_of(entry)].add(text(entry) + " / a game mesh");
    }

    std::vector<std::string> notes;
    const auto say = [&](const std::map<std::string, Findings>& found, const char* what) {
        for (const auto& [mod, findings] : found) {
            if (!findings.count) continue;
            std::string line = "Mod " + mod + ": " + std::to_string(findings.count) + what;
            for (std::size_t at = 0; at < findings.examples.size(); ++at) line += (at ? "; " : ": ") + findings.examples[at];
            notes.push_back(std::move(line));
        }
    };
    say(meshes, " mesh name(s) share the low 32 bits of their name hash with another mesh, so the game gives them one "
                "set of material tables (wrong materials, or a crash when one has more materials)");
    say(names, " asset name(s) share their name hash with another name, and the game finds assets by the hash alone");
    say(resources, " resource name(s) stand for two different resources (another resource id in another bundle), and the "
                   "game keeps one resource per name");
    return notes;
}

} // namespace dingosdk::mods::detail
