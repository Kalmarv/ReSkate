#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Thunderstore (thunderstore.io/c/reskate): the package listing the launcher's
// Mods browser shows, and the one package a multiplayer host's map comes from
// (Extension/Assets/map_downloads.h). No network here; gui_mods_browse.cpp
// fetches with launcher_update::http_get, the runtime with https::get.
// ReSkate packages have no dependencies.
//
// A ReSkate package is a mod folder zipped with Thunderstore's manifest.json,
// icon.png and README.md at the top, and installs as Mods/<Namespace-Name>.
namespace dingosdk::thunderstore {

inline constexpr char site[] = "https://thunderstore.io";
inline constexpr char default_community[] = "reskate";

// The community the browser lists: RESKATE_THUNDERSTORE_COMMUNITY when set
// (to try the browser against a community that has packages), else reskate.
std::string community();
// /c/<community>/api/v1/package-listing-index/: a gzipped JSON array of
// gzipped listing chunks. The plain listing below is the fallback.
std::wstring listing_index_url(std::string_view community);
std::wstring listing_url(std::string_view community);
// The community's page on the site.
std::wstring community_page(std::string_view community);

struct Version {
    std::string number;                     // Major.Minor.Patch
    std::string full_name;                  // Namespace-Name-1.2.3
    std::string description;
    std::string icon;                       // https URL of the 256x256 icon
    std::string download_url;
    std::string website_url;
    std::string date_created;
    std::uint64_t downloads{};
    std::uint64_t file_size{};
};

struct Package {
    std::string name;                       // letters, digits and '_'
    std::string owner;                      // the team, which is the package namespace
    std::string full_name;                  // Namespace-Name
    std::string package_url;
    std::string date_created, date_updated; // ISO 8601, so they sort as text
    std::vector<std::string> categories;
    int rating{};
    bool pinned{}, deprecated{}, nsfw{};
    std::uint64_t downloads{};              // every version together
    std::vector<Version> versions;          // newest first, never empty

    const Version& latest() const { return versions.front(); }
    // The name as the site shows it: underscores as spaces.
    std::string title() const;
    bool in_category(std::string_view category) const;
};

// One listing chunk (or the whole plain listing): a JSON array of packages.
// Rows that are malformed or have no active version are skipped.
std::vector<Package> parse_listing(std::string_view json);
// The listing index: a JSON array of HTTPS chunk URLs.
std::vector<std::wstring> parse_index(std::string_view json);
// Unpacks a gzip stream (the index and its chunks), refusing output over
// `limit` bytes. Input that is not gzip comes back unchanged.
std::string gunzip(std::string_view data, std::size_t limit);

// One package without the listing: /api/experimental/package/<Namespace>/<Name>/
// answers with the communities that list it (parse_standing), and .../<version>/
// with that version (parse_release). Both are small; the listing is megabytes.
std::wstring package_url(std::string_view owner, std::string_view name);
std::wstring version_url(std::string_view owner, std::string_view name, std::string_view version);
struct Standing {
    std::vector<std::string> communities;   // the community slugs that list it
    bool deprecated{}, nsfw{};              // nsfw: in any of them
    bool listed_in(std::string_view community) const;
};
// Throws when the answer is not a package.
Standing parse_standing(std::string_view json);
// The version package_url's version answer describes; nothing when it is inactive
// or malformed. It carries no file_size.
std::optional<Version> parse_release(std::string_view json);

// Below zero, zero or above zero as `a` is older, the same or newer than `b`.
// Components compare as numbers ("1.10.0" is newer than "1.9.0").
int compare_versions(std::string_view a, std::string_view b);

// The Mods folder a package installs into: Namespace-Name, so a manual
// install of a downloaded zip (its version suffix dropped) lands in the same place.
std::string folder_for(std::string_view full_name);

// Installed mod folders and the version their manifest.json gives.
using Installed = std::map<std::string, std::string, std::less<>>;

// True when `package`'s latest version is newer than the installed copy.
bool update_available(const Package& package, const Installed& installed);

} // namespace dingosdk::thunderstore
