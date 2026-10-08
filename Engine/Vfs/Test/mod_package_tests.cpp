#include "Engine/Vfs/mod_package.h"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace dingosdk::mods;
namespace {
void check(bool ok, const std::string &message) {
    if (ok) return;
    std::cerr << message << '\n';
    std::exit(1);
}
} // namespace

int main() {
    const auto id = package_of("Team-Big_Map", "1.2.10");
    check(id && id->owner == "Team" && id->name == "Big_Map" && id->version == "1.2.10", "folder and version");
    check(id->full_name() == "Team-Big_Map" && id->text() == "Team-Big_Map-1.2.10", "text forms");
    check(parse_package("Team-Big_Map-1.2.10") == id, "text reads back");

    // Folders the launcher's Thunderstore install would never name.
    check(!package_of("BigMap", "1.0.0"), "no namespace");
    check(!package_of("Team-Big-Map", "1.0.0"), "a dash in the name");
    check(!package_of("Team-Big Map", "1.0.0"), "a space in the name");
    check(!package_of("-Map", "1.0.0") && !package_of("Team-", "1.0.0"), "empty parts");

    // Thunderstore versions are Major.Minor.Patch.
    for (const auto *version : {"", "1", "1.0", "1.0.0.0", "1..0", ".1.0", "1.0.", "v1.0.0", "1.0.0-beta"})
        check(!package_of("Team-Map", version), std::string("version ") + version);

    check(!parse_package("Team-Map"), "no version");
    check(!parse_package("Team-Map-"), "empty version");
    check(!parse_package(""), "empty");
    check(!parse_package("Team-Map-1.0.0\n"), "control character");
    check(!parse_package(std::string(70, 'a') + "-" + std::string(70, 'b') + "-1.0.0"), "too long");
    return 0;
}
