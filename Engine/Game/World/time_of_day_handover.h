#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dingosdk {
// Times of day take turns: a new one is asked to load only once every sibling time of day that
// is to be off has gone. Its reference reports nothing requested and nothing loaded within a
// frame of the unload, but the engine frees that time of day's entities, and with them their
// places in the light tables, only later (up to half a second, in a city with thousands of
// lights); a time of day that loads in between has its lights refused. So a time of day seen on
// has gone only once it has been seen off and its lights have left the light tables. A
// reference that disappears has not gone either. Each wait ends at a time limit, so a switch can
// never leave the world without a time of day.
//
// Mark is what the light tables held when the time of day was asked to go (mark), or else when it
// was first seen off; settle's lights_left compares the tables with it.
//
// Kept per world, across the apply loop's session resets: those only rebuild the plan.
template <class Mark> class TimeOfDayHandover {
public:
    static constexpr std::uint64_t missing_limit_ms = 2000, lights_limit_ms = 1500;
    enum class Wait { gone, off, lights, missing, timed_out };

    // The world the apply loop runs in. Another world forgets what was seen in the last one.
    void enter(std::uintptr_t world, std::size_t slots) {
        if (world == world_ && seen_.size() == slots) return;
        world_ = world;
        seen_.assign(slots, std::nullopt);
    }
    // One time of day's reference as this poll sees it; present is false when its reference is
    // not captured or not readable in this world. now_mark: what the tables hold now, asked for
    // only when it is first seen off without a mark.
    template <class NowMark>
    void observe(std::size_t slot, bool present, bool requested, bool loaded, std::uint64_t now, NowMark&& now_mark) {
        if (slot >= seen_.size()) return;
        auto& seen = seen_[slot];
        if (present && (requested || loaded)) {
            // On again after it was off: outgoing from scratch. Still on: keep the mark.
            if (!seen || seen->off_at) seen = Seen{};
            seen->missing_since = 0;
            return;
        }
        if (!seen) return;
        if (!present) {
            if (!seen->missing_since) seen->missing_since = now;
            return;
        }
        seen->missing_since = 0;
        if (seen->off_at) return;
        seen->off_at = now;
        if (!seen->mark) seen->mark = now_mark();
    }
    // It is being asked to go; what the tables hold now.
    void mark(std::size_t slot, Mark mark) {
        if (slot < seen_.size() && seen_[slot]) seen_[slot]->mark = std::move(mark);
    }
    // Whether a time of day that is to be off has gone. lights_left(const std::optional<Mark>&):
    // whether lights it made are still in the tables. Gone and timed out both forget it.
    template <class LightsLeft>
    Wait settle(std::size_t slot, std::uint64_t now, LightsLeft&& lights_left) {
        if (slot >= seen_.size() || !seen_[slot]) return Wait::gone;
        const auto& seen = *seen_[slot];
        Wait wait;
        if (seen.missing_since && !seen.off_at)
            wait = now - seen.missing_since >= missing_limit_ms ? Wait::timed_out : Wait::missing;
        else if (!seen.off_at)
            wait = Wait::off;
        else if (!lights_left(seen.mark))
            wait = Wait::gone;
        else
            wait = now - seen.off_at >= lights_limit_ms ? Wait::timed_out : Wait::lights;
        if (wait == Wait::lights) seen_[slot]->waited_for_lights = true;
        if (wait == Wait::gone || wait == Wait::timed_out) seen_[slot].reset();
        return wait;
    }

    // When it was first seen off; 0 when not, or not outgoing.
    std::uint64_t off_at(std::size_t slot) const {
        return slot < seen_.size() && seen_[slot] ? seen_[slot]->off_at : 0;
    }
    // Whether settle has had it wait for its lights.
    bool waited_for_lights(std::size_t slot) const {
        return slot < seen_.size() && seen_[slot] && seen_[slot]->waited_for_lights;
    }

private:
    struct Seen {
        std::uint64_t off_at{};        // first seen present with nothing requested or loaded
        std::uint64_t missing_since{}; // its reference not seen since
        std::optional<Mark> mark;
        bool waited_for_lights{};
    };
    std::uintptr_t world_{};
    std::vector<std::optional<Seen>> seen_;
};
}
