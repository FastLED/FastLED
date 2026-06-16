/// @file can_match.cpp.hpp
/// @brief Implementation of fl::canMatch (#3186).

// IWYU pragma: private

#include "fl/channels/can_match.h"
#include "fl/channels/capabilities.h"

namespace fl {

namespace can_match_detail {

// "Strongest" rejection rank used when accumulating per-group failures
// across an entire driver's groups. We surface the most informative
// reason: pin/pair > frequency > timing > capacity > protocol.
// Yes always wins.
inline fl::u8 strength(HandleResult r) FL_NOEXCEPT {
    switch (r) {
        case HandleResult::Yes:         return 100u;
        case HandleResult::NoPinPair:   return 50u;
        case HandleResult::NoPin:       return 45u;
        case HandleResult::NoFrequency: return 40u;
        case HandleResult::NoTiming:    return 35u;
        case HandleResult::NoCapacity:  return 30u;
        case HandleResult::NoProtocol:  return 10u;
    }
    return 0u;
}

// Resolve which frequency range applies to a request landing in `g`,
// falling back to the driver-level *_frequency when the group doesn't
// constrain it.
inline FreqRange effectiveFrequencyRange(
        const DriverCapabilities& caps, const PinGroup& g) FL_NOEXCEPT {
    if (!g.frequency.isUnspecified()) {
        return g.frequency;
    }
    return (g.protocol == Protocol::Clockless)
        ? caps.clockless_frequency
        : caps.spi_frequency;
}

// "Can this driver hit a phase of integer N ticks where the phase
// duration N / effective_clock_hz (seconds) is inside `window`
// (nanoseconds)?" Equivalently: is there an integer N >= 1 such that
//   window.min_ns <= N * 1e9 / effective_clock_hz <= window.max_ns
// Solving for N integer-safely (no truncated tick_ns intermediate):
//   n_lo = ceil(window.min_ns * effective_clock_hz / 1e9)
//   n_hi = floor(window.max_ns * effective_clock_hz / 1e9)
// Match iff n_lo <= n_hi AND n_lo <= max_ticks (when max_ticks != 0).
//
// The u64 numerator avoids the tick_ns truncation that previously
// mis-ranked tight windows near boundaries (CodeRabbit #3197).
inline bool canHitPhase(
        fl::u32 effective_clock_hz, fl::u16 max_ticks,
        const NanosRange& window) FL_NOEXCEPT {
    if (window.isUnspecified()) {
        // No chipset constraint on this phase.
        return true;
    }
    if (effective_clock_hz == 0u) {
        return false;
    }
    const fl::u64 kBillion = 1000000000ULL;
    const fl::u64 lo_num = static_cast<fl::u64>(window.min_ns) * effective_clock_hz;
    const fl::u64 hi_num = static_cast<fl::u64>(window.max_ns) * effective_clock_hz;
    // ceil(lo_num / kBillion) — divide-and-round-up without overflow.
    fl::u64 n_lo_64 = (lo_num + kBillion - 1ULL) / kBillion;
    fl::u64 n_hi_64 = hi_num / kBillion;
    if (n_lo_64 < 1ULL) {
        n_lo_64 = 1ULL;
    }
    if (n_lo_64 > n_hi_64) {
        return false;
    }
    if (max_ticks != 0u && n_lo_64 > static_cast<fl::u64>(max_ticks)) {
        return false;
    }
    return true;
}

// True iff the driver can hit all four chipset tolerance windows at
// SOME divider value within [min_divider, max_divider].
inline bool isClocklessTimingCompatible(
        const ClocklessTimingCapability& cap,
        const ChipsetClocklessTiming& chip) FL_NOEXCEPT {
    if (cap.isUnspecified()) {
        // Driver doesn't constrain timing — treat as "any chipset fits"
        // (e.g. DMA-fed pattern shifters where any tick width is OK).
        return true;
    }
    if (chip.t0h_window.isUnspecified() && chip.t0l_window.isUnspecified()
        && chip.t1h_window.isUnspecified() && chip.t1l_window.isUnspecified()) {
        return true;
    }
    fl::u16 d_lo = cap.min_divider == 0u ? 1u : cap.min_divider;
    fl::u16 d_hi = cap.max_divider == 0u ? d_lo : cap.max_divider;
    if (d_hi < d_lo) {
        return false;
    }
    for (fl::u32 d = d_lo; d <= d_hi; ++d) {
        if (d == 0u) {
            continue;
        }
        fl::u32 effective_clock = cap.base_clock_hz / static_cast<fl::u32>(d);
        if (effective_clock == 0u) {
            continue;
        }
        if (canHitPhase(effective_clock, cap.max_phase_ticks, chip.t0h_window) &&
            canHitPhase(effective_clock, cap.max_phase_ticks, chip.t0l_window) &&
            canHitPhase(effective_clock, cap.max_phase_ticks, chip.t1h_window) &&
            canHitPhase(effective_clock, cap.max_phase_ticks, chip.t1l_window)) {
            return true;
        }
    }
    return false;
}

// True iff the set bits in `b` form one contiguous run (no gaps).
// Empty bitset and single-bit bitsets are trivially contiguous.
inline bool isContiguousBitset(const PinBitset& b) FL_NOEXCEPT {
    fl::u32 lo = 0u;
    fl::u32 hi = 0u;
    bool found_lo = false;
    for (fl::u32 i = 0; i < kPinSetCapacity; ++i) {
        if (b.test(i)) {
            if (!found_lo) {
                lo = i;
                found_lo = true;
            }
            hi = i;
        }
    }
    if (!found_lo) {
        return true;
    }
    return b.count() == (hi - lo + 1u);
}

// Driver-level protocol pre-filter — fast-out before walking groups.
inline bool driverProtocolGate(
        const DriverCapabilities& caps, Protocol p) FL_NOEXCEPT {
    return (p == Protocol::Clockless) ? caps.supports_clockless
                                      : caps.supports_spi;
}

// Single-group check that handles both single-pin and multi-pin requests
// uniformly (single-pin is a degenerate bitset of one bit).
inline HandleResult checkOneGroup(
        const DriverCapabilities& caps,
        const PinGroup& g,
        const ChannelRequest& r) FL_NOEXCEPT {
    if (g.protocol != r.protocol) {
        return HandleResult::NoProtocol;
    }
    if (!g.data_pins.acceptsAll(r.data_pins)) {
        return HandleResult::NoPin;
    }
    if (g.pins_must_be_contiguous && !isContiguousBitset(r.data_pins)) {
        return HandleResult::NoPin;
    }
    if (r.protocol == Protocol::Spi) {
        if (!g.clock_pins.accepts(r.clock_pin)) {
            return HandleResult::NoPinPair;
        }
    }
    // Capacity: bulk requests consume one slot per set bit on this
    // group (the "ganged into one group" model). For "fan out across
    // independent groups" the resolver handles distribution at a higher
    // layer — this free function answers "does THIS group accept the
    // entire bitset?". Single-pin requests trivially fit (count=1
    // <= max_concurrent for any sane group).
    fl::u32 requested_count = r.data_pins.count();
    if (g.max_concurrent != 0u && requested_count > static_cast<fl::u32>(g.max_concurrent)) {
        return HandleResult::NoCapacity;
    }
    if (r.frequency_hz.has_value()) {
        FreqRange fr = effectiveFrequencyRange(caps, g);
        if (!fr.isUnspecified() && !fr.contains(*r.frequency_hz)) {
            return HandleResult::NoFrequency;
        }
    }
    if (r.protocol == Protocol::Clockless) {
        if (!isClocklessTimingCompatible(caps.clockless_timing, r.timing)) {
            return HandleResult::NoTiming;
        }
    }
    return HandleResult::Yes;
}

}  // namespace can_match_detail


HandleResult canMatch(
    const DriverCapabilities& caps,
    fl::span<const PinGroup>  groups,
    const ChannelRequest&     request) FL_NOEXCEPT {
    if (!can_match_detail::driverProtocolGate(caps, request.protocol)) {
        return HandleResult::NoProtocol;
    }
    if (groups.empty()) {
        // No groups published; nothing this driver can match concretely.
        return HandleResult::NoPin;
    }
    if (request.data_pins.none()) {
        // No pins requested — nothing to match against.
        return HandleResult::NoPin;
    }
    // Driver-wide aggregate cap: a bulk request with N pins can't be
    // served by a driver that publishes max_total_channels < N, even if
    // an individual group's max_concurrent would otherwise fit it
    // (CodeRabbit #3197).
    const fl::u32 requested_count = request.data_pins.count();
    if (caps.max_total_channels != 0u &&
        requested_count > static_cast<fl::u32>(caps.max_total_channels)) {
        return HandleResult::NoCapacity;
    }

    HandleResult best = HandleResult::NoProtocol;
    for (fl::size i = 0; i < groups.size(); ++i) {
        HandleResult r = can_match_detail::checkOneGroup(caps, groups[i], request);
        if (r == HandleResult::Yes) {
            return r;
        }
        if (can_match_detail::strength(r) > can_match_detail::strength(best)) {
            best = r;
        }
    }
    return best;
}

}  // namespace fl
