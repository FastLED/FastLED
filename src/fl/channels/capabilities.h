/// @file capabilities.h
/// @brief Declarative driver capabilities + channel-request types for the
///        Channel Manager's predicate / dependency-resolution system (#3186).
///
/// Drivers publish:
///   - DriverCapabilities  — small constant header (driver-wide facts)
///   - fl::span<const PinGroup>  — per-hardware-resource enumeration
///
/// The Channel Manager (and unit tests) consume these via the free
/// fl::canMatch / fl::canMatchBulk functions (declared in can_match.h),
/// which return a HandleResult describing whether a single-pin or bulk
/// ChannelRequest can be served by the supplied capability data.
///
/// This header defines pure POD data types only. No driver instance is
/// required to use any of these.

#pragma once

#include "fl/stl/bitset.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/optional.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string_view.h"

namespace fl {

// =====================================================================
// Protocol — what bus shape the channel uses.
// =====================================================================

enum class Protocol : fl::u8 {
    Clockless = 0,  ///< Single data line + async timing (WS2812, SK6812, ...)
    Spi       = 1,  ///< Data + clock (APA102, SK9822, ...)
};


// =====================================================================
// FreqRange / NanosRange — inclusive [min, max] ranges with explicit
// "unspecified" sentinels.
//
// All-zero is intentionally NOT the unspecified state — that would
// silently match every legitimate value. Use any() / exactly() /
// atLeast() / atMost() to construct.
// =====================================================================

struct FreqRange {
    fl::u32 min_hz;
    fl::u32 max_hz;

    static constexpr FreqRange any() FL_NOEXCEPT {
        return {0u, 0xFFFFFFFFu};
    }
    static constexpr FreqRange exactly(fl::u32 hz) FL_NOEXCEPT {
        return {hz, hz};
    }
    static constexpr FreqRange atLeast(fl::u32 hz) FL_NOEXCEPT {
        return {hz, 0xFFFFFFFFu};
    }
    static constexpr FreqRange atMost(fl::u32 hz) FL_NOEXCEPT {
        return {0u, hz};
    }

    constexpr bool isUnspecified() const FL_NOEXCEPT {
        return min_hz == 0u && max_hz == 0xFFFFFFFFu;
    }
    constexpr bool contains(fl::u32 hz) const FL_NOEXCEPT {
        return hz >= min_hz && hz <= max_hz;
    }
};

struct NanosRange {
    fl::u32 min_ns;
    fl::u32 max_ns;

    static constexpr NanosRange any() FL_NOEXCEPT {
        return {0u, 0xFFFFFFFFu};
    }
    static constexpr NanosRange exactly(fl::u32 ns) FL_NOEXCEPT {
        return {ns, ns};
    }
    static constexpr NanosRange atLeast(fl::u32 ns) FL_NOEXCEPT {
        return {ns, 0xFFFFFFFFu};
    }
    static constexpr NanosRange atMost(fl::u32 ns) FL_NOEXCEPT {
        return {0u, ns};
    }

    constexpr bool isUnspecified() const FL_NOEXCEPT {
        return min_ns == 0u && max_ns == 0xFFFFFFFFu;
    }
    constexpr bool contains(fl::u32 ns) const FL_NOEXCEPT {
        return ns >= min_ns && ns <= max_ns;
    }
};


// =====================================================================
// PinSet — discriminated pin allowlist (bitset-backed).
//
// Four conceptual shapes, three encodings:
//   None          — slot not applicable (e.g. clock on a Clockless group)
//   AnyOutputPin  — any GPIO output (RMT5-style flexibility)
//   Allowlist size = 1 — fixed pin (LPUART per-instance TX, SPI IO_MUX)
//   Allowlist size N>1 — flex within an allowlist (FlexIO shifters)
// =====================================================================

/// Maximum pin number representable in a PinSet's bitset. 128 covers
/// every MCU FastLED targets (ESP32-P4 = 55 GPIO, Teensy 4.1 ~ 55, etc.).
/// Use `static constexpr` (not `inline constexpr` — that's C++17).
static constexpr fl::u32 kPinSetCapacity = 128u;

using PinBitset = fl::bitset_fixed<kPinSetCapacity>;

enum class PinSetKind : fl::u8 {
    None         = 0,
    AnyOutputPin = 1,
    Allowlist    = 2,
};

struct PinSet {
    PinSetKind kind;
    PinBitset pins;  ///< only meaningful when kind == Allowlist

    constexpr PinSet() FL_NOEXCEPT : kind(PinSetKind::None), pins() {}
    constexpr PinSet(PinSetKind k) FL_NOEXCEPT : kind(k), pins() {}
    constexpr PinSet(PinSetKind k, const PinBitset& b) FL_NOEXCEPT : kind(k), pins(b) {}

    /// Factory: slot is not applicable.
    static constexpr PinSet none() FL_NOEXCEPT { return PinSet(PinSetKind::None); }

    /// Factory: any GPIO output pin in [0, kPinSetCapacity).
    static constexpr PinSet anyOutput() FL_NOEXCEPT { return PinSet(PinSetKind::AnyOutputPin); }

    /// Factory: allowlist of pins, constructed from a static array.
    /// Pin numbers >= kPinSetCapacity are ignored.
    template <fl::size N>
    static PinSet allowlist(const fl::i16 (&arr)[N]) FL_NOEXCEPT {
        PinSet ps(PinSetKind::Allowlist);
        for (fl::size i = 0; i < N; ++i) {
            if (arr[i] >= 0 && static_cast<fl::u32>(arr[i]) < kPinSetCapacity) {
                ps.pins.set(static_cast<fl::u32>(arr[i]));
            }
        }
        return ps;
    }

    /// Factory: allowlist with a single pin (the "fixed pin" idiom).
    static PinSet fixed(fl::i16 pin) FL_NOEXCEPT {
        PinSet ps(PinSetKind::Allowlist);
        if (pin >= 0 && static_cast<fl::u32>(pin) < kPinSetCapacity) {
            ps.pins.set(static_cast<fl::u32>(pin));
        }
        return ps;
    }

    /// True iff this set accepts the given single pin.
    bool accepts(fl::i16 pin) const FL_NOEXCEPT {
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

    /// True iff every set bit in `request` is also accepted here.
    /// For AnyOutputPin: trivially true. For Allowlist: bitset
    /// inclusion check.
    bool acceptsAll(const PinBitset& request) const FL_NOEXCEPT {
        if (kind == PinSetKind::None) {
            return request.none();
        }
        if (kind == PinSetKind::AnyOutputPin) {
            return true;
        }
        // Allowlist: every requested bit must be in `pins`.
        // i.e. request & ~pins must be empty. We implement that
        // without operator overloads to stay portable across bitset
        // impls: iterate via count() over the difference.
        for (fl::u32 i = 0; i < kPinSetCapacity; ++i) {
            if (request.test(i) && !pins.test(i)) {
                return false;
            }
        }
        return true;
    }
};


// =====================================================================
// ClocklessTimingCapability — driver's clock-tree model.
//
// Captures the physics: tick width = base_clock_hz / divider, phases
// are held for an integer number of ticks. Compatibility with a
// chipset's tolerance windows is determined by whether an integer N
// exists such that N * tick_ns falls inside each window.
//
// DMA-fed drivers (PARLIO, I2S) also fit: their "tick" is the divided
// peripheral clock, their "phase" is consecutive same-state bits in a
// shaped data pattern.
// =====================================================================

struct ClocklessTimingCapability {
    fl::u32 base_clock_hz;     ///< e.g. 80'000'000 for RMT5 (APB_CLK)
    fl::u16 min_divider;        ///< smallest divider -> finest tick width
    fl::u16 max_divider;        ///< largest divider  -> coarsest tick width
    fl::u16 max_phase_ticks;    ///< counter-width cap per phase

    static constexpr ClocklessTimingCapability unspecified() FL_NOEXCEPT {
        return {0u, 0u, 0u, 0u};
    }
    constexpr bool isUnspecified() const FL_NOEXCEPT {
        return base_clock_hz == 0u;
    }
};


// =====================================================================
// PinGroup — one independent hardware resource.
//
// Multiple groups per driver describe distinct slots (RMT channels,
// FlexIO instances, LPUART instances). Each can independently match
// some subset of requests.
// =====================================================================

struct PinGroup {
    fl::u16    instance_id;             ///< opaque ID; used in diagnostics
    Protocol   protocol;                 ///< group is Clockless OR Spi

    PinSet data_pins;                    ///< data-line allowlist (or AnyOutput)
    PinSet clock_pins;                   ///< clock-line allowlist; None for Clockless

    fl::u8 max_concurrent;               ///< how many data_pins can be active concurrently

    /// Frequency range for this group's clock.
    ///   Clockless: bit rate.
    ///   SPI: SCLK frequency.
    /// FreqRange::any() = "no per-group constraint" — fall back to the
    /// driver-level *_frequency.
    /// Best-effort note: drivers backed by PLL dividers can only hit a
    /// discrete set of rates within the range. The resolver does NOT
    /// enforce divisor-exactness — it accepts any request inside the
    /// range; drivers internally round to the nearest divisor at-or-
    /// below the requested rate.
    FreqRange frequency;

    /// Groups sharing a non-zero shared_clock_domain_id must all run at
    /// the same frequency (PARLIO timer, FlexIO instance timer, etc.).
    /// 0 = independent clock (default).
    fl::u8 shared_clock_domain_id;

    /// Groups sharing a non-zero shared_budget_id draw concurrent slots
    /// from one pool. Pool size lives on the FIRST group with this ID
    /// (its max_concurrent IS the pool size); subsequent groups with
    /// the same ID re-use it. Models FlexIO "4 shifters split between
    /// SPI and clockless groups on the same instance".
    /// 0 = private cap, max_concurrent is mine alone (default).
    fl::u8 shared_budget_id;

    /// True iff a bulk request's data_pins bitset must be contiguous
    /// (set bits form an unbroken run). RP2040 PIO side-set, STM32
    /// LTDC parallel, FlexIO shifter cascade. Single-pin requests
    /// trivially satisfy this. Default false.
    bool pins_must_be_contiguous;

    constexpr PinGroup() FL_NOEXCEPT
        : instance_id(0)
        , protocol(Protocol::Clockless)
        , data_pins()
        , clock_pins()
        , max_concurrent(1)
        , frequency(FreqRange::any())
        , shared_clock_domain_id(0)
        , shared_budget_id(0)
        , pins_must_be_contiguous(false) {}
};


// =====================================================================
// DriverCapabilities — tiny constant-header descriptor.
//
// Note: there is intentionally NO supported_chipsets field. Chipset
// support is a derived consequence of timing — if a chipset's tolerance
// windows can be hit by the driver's clock tree, the chipset is
// supported. New chipsets work automatically without driver updates.
// =====================================================================

struct DriverCapabilities {
    fl::string_view name;       ///< diagnostics, e.g. "RMT5"
    fl::u8 priority;            ///< higher = preferred
    bool supports_clockless;
    bool supports_spi;
    fl::u8 max_total_channels;  ///< aggregate cap; 0 = unbounded

    /// Aggregate frequency ranges. Used when a group's `frequency` is
    /// `any()`. FreqRange::any() = no driver-level constraint either.
    FreqRange clockless_frequency;
    FreqRange spi_frequency;

    /// Clock-tree model — the ONLY chipset-support gate. Use the
    /// unspecified() sentinel when the driver isn't clockless-capable.
    ClocklessTimingCapability clockless_timing;

    /// Peripheral-flavor flags. Default zero = neutral / no claim.
    struct Flags {
        fl::u8 synchronized_start    : 1;  ///< all groups latch on same clock edge
        fl::u8 requires_dma_memory   : 1;  ///< DMA can't reach PSRAM
        fl::u8 open_drain_capable    : 1;
        fl::u8 mixed_protocols_ok    : 1;  ///< CL+SPI on same instance OK
        fl::u8 reserved              : 4;
    } flags;

    DriverCapabilities() FL_NOEXCEPT
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
};


// =====================================================================
// ChipsetClocklessTiming — chipset's per-phase tolerance windows.
//
// Each window is the inclusive [min, max] in nanoseconds that the
// driver's quantized output must land inside for the chipset to be
// driven correctly. e.g. WS2812 T0H is nominally 350 ns ± 150 ns,
// so t0h_window = {200, 500}.
// =====================================================================

struct ChipsetClocklessTiming {
    NanosRange t0h_window;
    NanosRange t0l_window;
    NanosRange t1h_window;
    NanosRange t1l_window;

    static constexpr ChipsetClocklessTiming unspecified() FL_NOEXCEPT {
        return {NanosRange::any(), NanosRange::any(),
                NanosRange::any(), NanosRange::any()};
    }
};


// =====================================================================
// ChannelRequest — passive input to canMatch.
//
// One type for both single-pin and ganged-multi-pin requests: data_pins
// is a bitset, so a single-pin request just has one bit set. Drivers
// that fan out to N independent groups vs gang into one group is the
// resolver's concern (not the request's).
//
// Requests are NOT a first-class API surface — sketches don't construct
// them. The Channel Manager builds them from addLeds<>() calls and
// hands them to fl::canMatch against each driver's published capability
// data.
// =====================================================================

struct ChannelRequest {
    Protocol protocol;
    PinBitset data_pins;                    ///< bit N set = pin N included
    fl::i16 clock_pin;                      ///< -1 for Clockless; single shared clock for Spi
    ChipsetClocklessTiming timing;          ///< unspecified/any for Spi requests
    fl::optional<fl::u32> frequency_hz;     ///< mostly SPI override

    ChannelRequest() FL_NOEXCEPT
        : protocol(Protocol::Clockless)
        , data_pins()
        , clock_pin(-1)
        , timing(ChipsetClocklessTiming::unspecified())
        , frequency_hz() {}

    /// Convenience factory for the common single-pin case.
    static ChannelRequest singlePin(
            Protocol p, fl::i16 data, fl::i16 clock,
            const ChipsetClocklessTiming& t = ChipsetClocklessTiming::unspecified(),
            fl::optional<fl::u32> freq = fl::optional<fl::u32>()) FL_NOEXCEPT {
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
};


// =====================================================================
// HandleResult — outcome of canMatch / canMatchBulk.
// =====================================================================

enum class HandleResult : fl::u8 {
    Yes          = 0,
    NoProtocol   = 1,   ///< driver doesn't support the requested Protocol
    NoPin        = 2,   ///< data pin not in any matching group's allowlist
    NoPinPair    = 3,   ///< clock pin not in the matching group's allowlist
    NoFrequency  = 4,   ///< requested freq outside driver/group range
    NoTiming     = 5,   ///< chipset tolerance windows can't be hit
    NoCapacity   = 6,   ///< group full / driver-total cap reached
};

/// Convenience: most call sites really want a bool.
constexpr bool isYes(HandleResult r) FL_NOEXCEPT {
    return r == HandleResult::Yes;
}

}  // namespace fl
