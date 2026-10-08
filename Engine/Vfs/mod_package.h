#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// Which Thunderstore package a mod came from, as one line of text that travels between players:
// "Namespace-Name-1.2.3". The launcher installs a package as Mods/<Namespace-Name> with
// Thunderstore's manifest.json (version_number) inside, so a mod folder named that way names
// its package. Header-only and portable: the runtime, the dedicated server and the protocol
// share it.
namespace dingosdk::mods {

// At most this long: Thunderstore caps namespaces and names well below it.
inline constexpr std::size_t maximum_package_id = 128;

struct PackageId {
    std::string owner, name, version; // owner is the namespace (the team)

    // Namespace-Name: the package, and the Mods folder it installs as.
    std::string full_name() const { return owner + "-" + name; }
    // Namespace-Name-1.2.3: one version of it.
    std::string text() const { return full_name() + "-" + version; }
    bool operator==(const PackageId&) const = default;
};

// Thunderstore's rule for namespaces and names: letters, digits and '_'.
inline bool valid_package_name(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
    });
}

// Major.Minor.Patch, each part digits.
inline bool valid_package_version(std::string_view value) noexcept {
    if (value.empty() || value.size() > 32) return false;
    int parts = 1;
    bool digit = false;
    for (const char ch : value) {
        if (ch == '.') {
            if (!digit) return false;
            ++parts;
            digit = false;
        } else if (ch >= '0' && ch <= '9') {
            digit = true;
        } else {
            return false;
        }
    }
    return digit && parts == 3;
}

// The package a mod folder and its manifest's version_number name, when both are in
// Thunderstore's form: Mods/Team-Map with version 1.0.0 is Team-Map-1.0.0.
inline std::optional<PackageId> package_of(std::string_view folder, std::string_view version) {
    const auto dash = folder.find('-');
    if (dash == std::string_view::npos || !valid_package_version(version)) return std::nullopt;
    PackageId id{std::string(folder.substr(0, dash)), std::string(folder.substr(dash + 1)), std::string(version)};
    if (!valid_package_name(id.owner) || !valid_package_name(id.name) || id.text().size() > maximum_package_id)
        return std::nullopt;
    return id;
}

// Reads "Namespace-Name-1.2.3" back; nothing for anything else. Neither part may hold a '-',
// so the text splits one way only.
inline std::optional<PackageId> parse_package(std::string_view text) {
    if (text.size() > maximum_package_id) return std::nullopt;
    const auto last = text.rfind('-');
    if (last == std::string_view::npos) return std::nullopt;
    return package_of(text.substr(0, last), text.substr(last + 1));
}

} // namespace dingosdk::mods
