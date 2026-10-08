#pragma once
#include "Engine/Game/Multiplayer/session_model.h"
#include <string>

// Downloading the map a multiplayer host is on. A join stops when this PC lacks the host's
// map; when the host named the Thunderstore package the map comes from (Packet::map_package),
// the session offers it here. Nothing downloads until the player accepts. The package comes
// only from the ReSkate community on Thunderstore, by the name the host sent, never from a
// link a host supplies; it installs as Mods/<Namespace-Name> as the launcher's Mods page does
// it, is switched on in mods.json and applied live, and the session joins again
// (take_rejoin). When the game cannot apply mods live it says to restart instead.
namespace dingosdk::map_downloads {
using Stage = MapDownloadStage;
using View = MapDownloadModel;
// Looks `package` up and offers it, replacing any offer not yet accepted. `map` and `host` are
// for the prompt. False, and nothing offered, while an accepted download is still running or
// for a package name not in Thunderstore's form.
bool offer(std::string package, std::string map, std::string host);
// Starts the offered download. Returns what happened, for the menu.
std::string accept();
// Drops the offer, or stops a download or install (an apply, once started, runs to its end).
void dismiss();
View view();
// True once, a moment after an accepted package was installed and applied: the session joins
// the host again.
bool take_rejoin();
} // namespace dingosdk::map_downloads
