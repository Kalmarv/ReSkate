#include "Engine/Game/World/world_layers.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <stdexcept>

using namespace dingosdk;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        // BAM's time-of-day layers as the level scan finds them: the seven under
        // StartPoint_Sublevels, and the seven of one lighting tier; one Grom time of day.
        constexpr std::array<const char*, 7> times{
            "1_Morning", "2_Noon", "3_Afternoon", "4_Evening", "5_Night", "6_WeatherDay", "7_WeatherNight"};
        WorldLayerCatalog catalog{{{"Levels/Game/BAM_LevelRoot/Lighting_Static", WorldMap::bam, -1, true},
                                   {"Levels/Game/BAM_LevelRoot/Lighting_Static_High", WorldMap::bam, 0, false}}, {0}, {}};
        for (const auto* time : times)
            catalog.nodes.push_back({std::string("Levels/Game/BAM_LevelRoot/High_TOD_") + time, WorldMap::bam, 1, false});
        const auto start = static_cast<int>(catalog.nodes.size());
        catalog.nodes.push_back({"Levels/Game/BAM_LevelRoot/StartPoint_Sublevels", WorldMap::bam, -1, false});
        for (const auto* time : times)
            catalog.nodes.push_back({std::string("Levels/game/BAM_LevelRoot/TOD_") + time, WorldMap::bam, start, false});
        const auto grom = static_cast<int>(catalog.nodes.size());
        catalog.nodes.push_back({"levels/game/DingoLevel_Isle_Of_Grom/StartPoint_Sublevels", WorldMap::grom, -1, false});
        const auto grom_weatherday = catalog.nodes.size();
        catalog.nodes.push_back({"levels/game/DingoLevel_Isle_Of_Grom/TOD_6_WeatherDay", WorldMap::grom, grom, false});
        catalog.nodes.push_back({"Levels/Game/BAM_LevelRoot/TOD_Global", WorldMap::bam, -1, false});
        catalog.nodes.push_back({"Levels/Game/BAM_LevelRoot/Period_TODO_1", WorldMap::bam, -1, false});
        catalog.nodes.push_back({"Levels/Game/BAM_LevelRoot/MethodTOD_1_Test", WorldMap::bam, -1, false});
        install_world_layer_catalog(std::move(catalog));
        const auto high = [](unsigned n) { return std::size_t{1} + n; };
        const auto tod = [start](unsigned n) { return static_cast<std::size_t>(start) + n; };
        const auto none = world_layer_nodes().size();

        for (unsigned n = 1; n <= 7; ++n)
            check(world_layer_time_of_day(tod(n)) && world_layer_time_of_day(high(n)), "Every TOD_<n> and tier TOD_<n> layer is a time of day");
        check(!world_layer_time_of_day(0) && !world_layer_time_of_day(1) && !world_layer_time_of_day(static_cast<std::size_t>(start)),
            "Lighting and StartPoint parents are not times of day");
        check(!world_layer_time_of_day(none - 3) && !world_layer_time_of_day(none - 2) && !world_layer_time_of_day(none - 1),
            "TOD without a number, TODO and a TOD inside a word are not times of day");
        check(!world_layer_time_of_day(none), "A slot outside the catalog is not a time of day");

        // WeatherDay -> Evening, as `tod evening` leaves the choices: Evening on, the other six off.
        std::vector<std::optional<bool>> desired(none);
        for (unsigned n = 1; n <= 7; ++n) desired[tod(n)] = n == 4;
        std::set<std::size_t> live{tod(6)};
        std::vector<std::size_t> asked;
        const auto off = [&](std::size_t at) { asked.push_back(at); return !live.contains(at); };
        check(world_layer_old_time_of_day(tod(4), desired, off) == tod(6), "Evening waits for WeatherDay, a higher slot, to unload");
        live = {tod(2)};
        check(world_layer_old_time_of_day(tod(4), desired, off) == tod(2), "Evening waits for Noon, a lower slot, to unload");
        live = {tod(2), tod(6)};
        check(world_layer_old_time_of_day(tod(4), desired, off) == tod(2), "With two old times of day the lower slot is named first");
        live = {tod(6)};
        check(world_layer_old_time_of_day(tod(4), desired, off) == tod(6), "and then the other one");
        live.clear();
        check(world_layer_old_time_of_day(tod(4), desired, off) == none, "Evening goes ahead once the old time of day is off");

        // Only siblings that are to be off hold a time of day back.
        live = {tod(6)};
        desired[tod(6)].reset();
        check(world_layer_old_time_of_day(tod(4), desired, off) == none, "A sibling left to the level does not hold the new time of day");
        desired[tod(6)] = true;
        check(world_layer_old_time_of_day(tod(4), desired, off) == none, "A sibling also forced on does not hold the new time of day");
        desired[tod(6)] = false;

        // The group is the time of day's siblings: not another tier, not another map.
        desired[high(6)] = false; desired[grom_weatherday] = false;
        live = {high(6), grom_weatherday};
        asked.clear();
        check(world_layer_old_time_of_day(tod(4), desired, off) == none, "Another tier's or another map's time of day does not hold this one");
        check(std::none_of(asked.begin(), asked.end(), [&](std::size_t at) { return at == high(6) || at == grom_weatherday; }),
            "Layers outside the group are not even looked at");
        desired[high(4)] = true;
        check(world_layer_old_time_of_day(high(4), desired, off) == high(6), "A tier's times of day form a group of their own");

        // A layer that is not a time of day never waits.
        desired[static_cast<std::size_t>(start)] = true;
        live = {tod(2)};
        check(world_layer_old_time_of_day(static_cast<std::size_t>(start), desired, off) == none, "StartPoint_Sublevels does not wait for its children");
        check(world_layer_old_time_of_day(none, desired, off) == none, "A slot outside the catalog does not wait");
        std::puts("World layer time-of-day checks passed."); return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
