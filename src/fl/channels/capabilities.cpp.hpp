/// @file capabilities.cpp.hpp
/// @brief Out-of-line definitions for capabilities.h types (#3186).
///
/// Keeps `src/fl/channels/capabilities.h` declaration-only for the
/// non-templated, non-trivial functions. Constexpr factories and
/// templates stay in the header where the language requires them.

// IWYU pragma: private

#include "fl/channels/capabilities.h"

namespace fl {

// ---------- PinSet ---------------------------------------------------

PinSet PinSet::fixed(fl::i16 pin) FL_NOEXCEPT {
    PinSet ps(PinSetKind::Allowlist);
    if (pin >= 0 && static_cast<fl::u32>(pin) < kPinSetCapacity) {
        ps.pins.set(static_cast<fl::u32>(pin));
    }
    return ps;
}

bool PinSet::accepts(fl::i16 pin) const FL_NOEXCEPT {
    if (kind == PinSetKind::None) {
        return false;
    }
    if (pin < 0 || static_cast<fl::u32>(pin) >= kPinSetCapacity) {
        return false;
    }
    if (kind == PinSetKind::AnyOutputPin) {
        return true;
    }
    return pins.test(static_cast<fl::u32>(pin));
}

bool PinSet::acceptsAll(const PinBitset& request) const FL_NOEXCEPT {
    if (kind == PinSetKind::None) {
        return request.none();
    }
    if (kind == PinSetKind::AnyOutputPin) {
        return true;
    }
    // Allowlist: every requested bit must be in `pins`.
    for (fl::u32 i = 0; i < kPinSetCapacity; ++i) {
        if (request.test(i) && !pins.test(i)) {
            return false;
        }
    }
    return true;
}


// ---------- DriverCapabilities ---------------------------------------

DriverCapabilities::DriverCapabilities() FL_NOEXCEPT
    : name()
    , priority(0)
    , supports_clockless(false)
    , supports_spi(false)
    , max_total_channels(0)
    , clockless_frequency(FreqRange::any())
    , spi_frequency(FreqRange::any())
    , clockless_timing(ClocklessTimingCapability::unspecified())
    , flags() {
    flags.synchronized_start = 0;
    flags.requires_dma_memory = 0;
    flags.open_drain_capable = 0;
    flags.mixed_protocols_ok = 0;
    flags.reserved = 0;
}


// ---------- ChannelRequest -------------------------------------------

ChannelRequest ChannelRequest::singlePin(
        Protocol p, fl::i16 data, fl::i16 clock,
        const ChipsetClocklessTiming& t,
        fl::optional<fl::u32> freq) FL_NOEXCEPT {
    ChannelRequest r;
    r.protocol = p;
    if (data >= 0 && static_cast<fl::u32>(data) < kPinSetCapacity) {
        r.data_pins.set(static_cast<fl::u32>(data));
    }
    r.clock_pin = clock;
    r.timing = t;
    r.frequency_hz = freq;
    return r;
}

}  // namespace fl
