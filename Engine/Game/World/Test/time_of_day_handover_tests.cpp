#include "Engine/Game/World/time_of_day_handover.h"
#include <iostream>

namespace {
int failures{};
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
// A mark here is how many lights were given back so far when the time of day was asked to go.
using Handover = dingosdk::TimeOfDayHandover<int>;
using Wait = Handover::Wait;
constexpr std::uintptr_t level = 0x1000;
int given_back{};
const auto now_mark = [] { return given_back; };
const auto lights_left = [](const std::optional<int>& mark) { return mark && given_back <= *mark; };
}
int main() {
    {
        Handover handover;
        handover.enter(level, 3);
        handover.observe(0, true, false, false, 100, now_mark);
        check(handover.settle(0, 100, lights_left) == Wait::gone, "a time of day never seen on has gone");
        check(handover.settle(1, 100, lights_left) == Wait::gone, "nor does one with no reference");
    }
    {
        // The level's own Noon: seen loaded, asked to go, reads off a frame later; its lights
        // are given back half a second after that.
        Handover handover;
        handover.enter(level, 3);
        given_back = 0;
        handover.observe(1, true, true, true, 1000, now_mark);
        handover.mark(1, given_back);
        check(handover.settle(1, 1000, lights_left) == Wait::off, "loaded: wait for the unload");
        handover.observe(1, true, false, true, 1010, now_mark);
        check(handover.settle(1, 1010, lights_left) == Wait::off, "still loaded: wait");
        handover.observe(1, true, false, false, 1020, now_mark);
        check(!handover.waited_for_lights(1), "not waited yet");
        check(handover.settle(1, 1030, lights_left) == Wait::lights, "off, but its lights are still in the tables");
        check(handover.waited_for_lights(1) && handover.off_at(1) == 1020, "waiting for the lights since it was seen off");
        handover.observe(1, true, false, false, 1300, now_mark);
        given_back = 7500;
        check(handover.settle(1, 1550, lights_left) == Wait::gone, "off and its lights given back: gone");
        check(handover.settle(1, 1560, lights_left) == Wait::gone, "gone is forgotten");
    }
    {
        // Its lights came back before the first poll that saw it off: the mark from when it was
        // asked to go still shows them coming back.
        Handover handover;
        handover.enter(level, 3);
        given_back = 0;
        handover.observe(2, true, true, true, 0, now_mark);
        handover.mark(2, given_back);
        given_back = 1810;
        handover.observe(2, true, false, false, 250, now_mark);
        check(handover.settle(2, 250, lights_left) == Wait::gone, "released before it was seen off");
    }
    {
        // Asked to go by someone else: marked when first seen off.
        Handover handover;
        handover.enter(level, 3);
        given_back = 5;
        handover.observe(2, true, true, true, 0, now_mark);
        handover.observe(2, true, false, false, 10, now_mark);
        check(handover.settle(2, 20, lights_left) == Wait::lights, "marked when first seen off");
        given_back = 6;
        check(handover.settle(2, 30, lights_left) == Wait::gone, "then given back");
    }
    {
        Handover handover;
        handover.enter(level, 3);
        given_back = 0;
        handover.observe(2, true, true, true, 0, now_mark);
        handover.mark(2, given_back);
        handover.observe(2, true, false, false, 10, now_mark);
        check(handover.settle(2, 10 + Handover::lights_limit_ms - 1, lights_left) == Wait::lights, "lights: wait up to the limit");
        check(handover.settle(2, 10 + Handover::lights_limit_ms, lights_left) == Wait::timed_out, "then give up");
        check(handover.settle(2, 20 + Handover::lights_limit_ms, lights_left) == Wait::gone, "and forget it");
    }
    {
        // A reference that vanishes is not off: wait for it to come back, up to a limit.
        Handover handover;
        handover.enter(level, 3);
        handover.observe(1, true, true, true, 0, now_mark);
        handover.observe(1, false, false, false, 100, now_mark);
        check(handover.settle(1, 100, lights_left) == Wait::missing, "missing reference: not gone");
        handover.observe(1, false, false, false, 500, now_mark);
        check(handover.settle(1, 100 + Handover::missing_limit_ms - 1, lights_left) == Wait::missing, "missing: up to the limit");
        handover.observe(1, true, true, true, 600, now_mark);
        check(handover.settle(1, 600, lights_left) == Wait::off, "back and still loaded");
        handover.observe(1, false, false, false, 700, now_mark);
        check(handover.settle(1, 700 + Handover::missing_limit_ms, lights_left) == Wait::timed_out, "missing past the limit: give up");
    }
    {
        // The apply loop resets its session (a frame without a map, a reselected anchor): that is
        // the same world, so what was seen stays. Another world starts over.
        Handover handover;
        handover.enter(level, 3);
        handover.observe(1, true, true, true, 0, now_mark);
        handover.enter(level, 3);
        check(handover.settle(1, 10, lights_left) == Wait::off, "kept across a session reset");
        handover.enter(level + 1, 3);
        check(handover.settle(1, 20, lights_left) == Wait::gone, "another world forgets");
    }
    {
        // Switched back on before it went: it waits for the unload again.
        Handover handover;
        handover.enter(level, 3);
        handover.observe(0, true, true, true, 0, now_mark);
        handover.observe(0, true, false, false, 10, now_mark);
        handover.observe(0, true, true, false, 20, now_mark);
        check(handover.settle(0, 30, lights_left) == Wait::off, "seen on again: waits for the unload again");
        given_back = 100;
        handover.observe(0, true, false, false, 40, now_mark);
        check(handover.settle(0, 50, lights_left) == Wait::lights, "and is marked afresh when seen off again");
    }
    if (failures) return 1;
    std::cout << "time of day handover: ok\n";
    return 0;
}
