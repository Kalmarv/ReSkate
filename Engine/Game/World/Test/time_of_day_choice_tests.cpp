#include "Engine/Game/World/world_layers.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace dingosdk;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        // Rows keyed as the level scan keys them ("<map>_<bundle leaf>" in lower case): BAM's seven time-of-day
        // layers, one lighting tier's seven, their parents, Grom's WeatherDay and a few names that only look alike.
        constexpr std::array<const char*, 7> times{
            "1_morning", "2_noon", "3_afternoon", "4_evening", "5_night", "6_weatherday", "7_weathernight"};
        WorldLayerCatalog catalog{{}, {0}, {}};   // one row per node: a row and its node share an index
        const auto row = [&](std::string key, WorldMap map, int parent) {
            const auto slot = static_cast<unsigned>(catalog.nodes.size());
            catalog.nodes.push_back({"Levels/" + key, map, parent, false});
            catalog.layers.push_back({key, key, "", map, slot, slot, "Lighting"});
            return slot;
        };
        row("bam_startpoint_sublevels", WorldMap::bam, -1);
        std::array<unsigned, 7> seven{}, high{};
        for (unsigned n = 0; n < 7; ++n) seven[n] = row(std::string("bam_tod_") + times[n], WorldMap::bam, 0);
        const auto tier = static_cast<int>(row("bam_lighting_static_high", WorldMap::bam, -1));
        for (unsigned n = 0; n < 7; ++n) high[n] = row(std::string("bam_high_tod_") + times[n], WorldMap::bam, tier);
        const auto grom = row("grom_tod_6_weatherday", WorldMap::grom, -1);
        const auto global = row("bam_tod_global", WorldMap::bam, -1);
        const auto eight = row("bam_tod_8_extra", WorldMap::bam, -1);
        const auto other_map = row("grom_tod_6_on_bam", WorldMap::bam, -1);
        install_world_layer_catalog(std::move(catalog));

        for (unsigned n = 0; n < 7; ++n) check(world_layer_time_of_day_choice(seven[n]) == n + 1, "bam_tod_<n>_<name> is time of day n");
        check(world_layer_time_of_day_choice(grom) == 6, "Every map's seven are chosen between, Grom's too");
        for (unsigned n = 0; n < 7; ++n) check(world_layer_time_of_day_choice(high[n]) == 0, "A lighting tier's own time of day is not one of the seven");
        check(world_layer_time_of_day_choice(0) == 0 && world_layer_time_of_day_choice(static_cast<std::size_t>(tier)) == 0,
              "Parents are not times of day");
        check(world_layer_time_of_day_choice(global) == 0 && world_layer_time_of_day_choice(eight) == 0,
              "A tod row without a number from 1 to 7 is not one of the seven");
        check(world_layer_time_of_day_choice(other_map) == 0, "A row is one of the seven of its own map only");
        check(world_layer_time_of_day_choice(world_layers().size()) == 0, "A row outside the catalog is not a time of day");
        std::puts("Time-of-day choice checks passed."); return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
