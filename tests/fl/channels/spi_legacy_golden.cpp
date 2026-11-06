/// @file tests/fl/channels/spi_legacy_golden.cpp
/// @brief Golden-byte lock-down for legacy `FastLED.addLeds<ESPIChipsets ...>`
///        under FASTLED_SPI_USES_CHANNEL_API (#4585 T1).
///
/// Purpose: the legacy addLeds<ESPIChipsets CHIPSET, DATA_PIN, CLOCK_PIN, ...>
/// overloads, when FASTLED_SPI_USES_CHANNEL_API is enabled (src/FastLED.h,
/// the `#elif FASTLED_SPI_USES_CHANNEL_API` block), build an
/// `fl::SpiChipsetConfig` + `fl::ChannelConfig` exactly the way
/// `fl::Channel::create(cfg)` is built directly, and route the frame through
/// the same `fl::Channel::encodeXXX()` free functions
/// (src/fl/channels/channel.cpp.hpp). This file pins that public-API
/// surface down for a later refactor (T2, which replaces the fl::Channel
/// back end with a slim controller) so the bytes a mock driver receives stay
/// byte-identical before and after.
///
/// Each case drives ONLY the public addLeds<>() + FastLED.show() API and
/// inspects the bytes a capturing mock IChannelDriver receives. Because the
/// legacy addLeds<>() overloads each keep a function-local `static
/// fl::ChannelPtr sChannel` (src/FastLED.h ~1167), every test case below
/// uses a distinct (DATA_PIN, CLOCK_PIN) pair so each addLeds<> call site is
/// instantiated exactly once, and the same instance is reused (and
/// re-registered on the shared, growing draw list) across the whole file.
/// Where a literal byte array is not safely hand-derivable (e.g. any case
/// that depends on the internal brightness/dither scaling pipeline), the
/// case instead builds the reference frame the same way `fl::Channel::create
/// (cfg)` (src/FastLED.h's own Channel-API addLeds<>() implementation) does,
/// and diffs against that -- marked `// differential: literal not
/// hand-derived`. That is not a weaker check: FastLED.h's Channel-API
/// addLeds<>() overloads construct the identical SpiEncoder/SpiChipsetConfig
/// /ChannelConfig and call the identical `fl::Channel::encodeXXX()`, so a
/// diff here still exercises (and pins) the real production code path.

// NOTE: tests are compiled with a shared PCH (tests/test_pch.h) that already
// parsed FastLED.h with the host default FASTLED_SPI_USES_CHANNEL_API=0, so a
// local `#define FASTLED_SPI_USES_CHANNEL_API 1` here is ignored and the
// legacy SPIOutput controllers get instantiated instead. Each case therefore
// calls `FastLED.addLedsSpiChannel<...>()` -- the always-declared bodies that
// the public `addLeds<ESPIChipsets ...>()` overloads forward to verbatim when
// FASTLED_SPI_USES_CHANNEL_API=1 (src/FastLED.h).

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/dither_frame.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/stl/malloc.h"
#include "crgb.h"
#include "test.h"

using namespace fl;

// MY9221 wire capture (#4636): explicit FastPin specializations for the two
// pins only the MY9221 case below uses (90 = DI, 91 = DCKI). The stub
// FastPin is a no-op, so these record the DI level sampled on every DCKI
// edge (MY9221 is dual-edge clocked: one toggle shifts one bit).
namespace my9221_wire {
inline bool& dataLevel() { static bool v = false; return v; }
inline fl::vector<fl::u8>& sampledBits() { static fl::vector<fl::u8> v; return v; }
}  // namespace my9221_wire

namespace fl {
template <> class FastPin<90> {
  public:
    typedef volatile RwReg* port_ptr_t;
    typedef RwReg port_t;
    static void setOutput() {}
    static void setInput() {}
    static void hi() { my9221_wire::dataLevel() = true; }
    static void lo() { my9221_wire::dataLevel() = false; }
    static void toggle() { my9221_wire::dataLevel() = !my9221_wire::dataLevel(); }
    static void strobe() { toggle(); toggle(); }
};
template <> class FastPin<91> {
  public:
    typedef volatile RwReg* port_ptr_t;
    typedef RwReg port_t;
    static void setOutput() {}
    static void setInput() {}
    static void hi() {}
    static void lo() {}
    static void toggle() {
        my9221_wire::sampledBits().push_back(my9221_wire::dataLevel() ? 1 : 0);
    }
    static void strobe() { toggle(); toggle(); }
};
}  // namespace fl

namespace { // anonymous namespace for spi_legacy_golden timing observation

// Virtual GPIO/time observations only. This model does not serialize bits and
// its nanoseconds are schedule units, not host wall time or target measurements.
struct My9221TimeEvent {
    enum Kind { Data, Clock, Wait } kind;
    fl::u64 ns;
    bool data;
    bool clock;
    bool interrupts_enabled;
};

struct My9221TimeState {
    fl::u64 ns = 0;
    bool data = false;
    bool clock = false;
    bool interrupts_enabled = true;
    int guard_entries = 0;
    fl::vector<My9221TimeEvent> events;

    void pin(bool clock_pin, bool level) {
        ++ns; // Explicit one-nanosecond model cost per GPIO operation.
        bool& previous = clock_pin ? clock : data;
        if (previous == level) return;
        previous = level;
        events.push_back({clock_pin ? My9221TimeEvent::Clock : My9221TimeEvent::Data,
                          ns, data, clock, interrupts_enabled});
    }

    void wait(fl::u64 duration) {
        ns += duration;
        events.push_back({My9221TimeEvent::Wait, ns, data, clock, interrupts_enabled});
    }
};

My9221TimeState& my9221TimeState() {
    static My9221TimeState state;
    return state;
}

void checkMy9221Timing(const My9221TimeState& state,
                        const fl::vector<My9221TimeEvent>& edges,
                        fl::size last_clock_index, fl::u64 stop_bound_ns) {
    fl::vector<My9221TimeEvent> pulses;
    for (fl::size i = last_clock_index + 1; i < state.events.size(); ++i) {
        const My9221TimeEvent& event = state.events[i];
        FL_CHECK_FALSE(event.clock); // DCKI remains parked throughout latch.
        // Final payload HIGH may require an initial DI parking fall. It is
        // not a fifth latch pulse; still verify the final data hold interval.
        if (event.kind == My9221TimeEvent::Data && pulses.empty() && !event.data) {
            FL_CHECK_GE(event.ns - edges.back().ns, static_cast<fl::u64>(10));
            continue;
        }
        if (event.kind == My9221TimeEvent::Data) pulses.push_back(event);
    }
    FL_REQUIRE_EQ(pulses.size(), static_cast<fl::size>(8));
    for (fl::size pulse = 0; pulse < 4; ++pulse) {
        const My9221TimeEvent& high = pulses[pulse * 2];
        const My9221TimeEvent& low = pulses[pulse * 2 + 1];
        FL_CHECK_TRUE(high.data);
        FL_CHECK_FALSE(low.data);
        const fl::u64 latch_high_ns = low.ns - high.ns;
        // Independent literal manufacturer bounds, not production constants.
        // Intended initial RED: consecutive production hi/lo gives1 model ns.
        FL_REQUIRE_GT(latch_high_ns, static_cast<fl::u64>(70));
        const fl::u64 low_end = pulse < 3 ? pulses[pulse * 2 + 2].ns : state.ns;
        FL_CHECK_GT(low_end - low.ns, static_cast<fl::u64>(230));
    }
    FL_CHECK_GT(pulses[0].ns - edges.back().ns, static_cast<fl::u64>(220000));
    FL_CHECK_GT(state.ns - pulses.back().ns, stop_bound_ns);
    for (fl::size i = 0; i < edges.size(); ++i) {
        if (i > 0) {
            const fl::u64 phase = edges[i].ns - edges[i - 1].ns;
            FL_CHECK_GE(phase, static_cast<fl::u64>(50));
            FL_CHECK_LE(phase, static_cast<fl::u64>(7200));
        }
        fl::u64 previous_data_change = 0;
        fl::u64 next_data_change = state.ns;
        for (const My9221TimeEvent& event : state.events) {
            if (event.kind != My9221TimeEvent::Data) continue;
            if (event.ns <= edges[i].ns) previous_data_change = event.ns;
            else { next_data_change = event.ns; break; }
        }
        FL_CHECK_GE(edges[i].ns - previous_data_change, static_cast<fl::u64>(10));
        FL_CHECK_GE(next_data_change - edges[i].ns, static_cast<fl::u64>(10));
    }
}

struct My9221AllocationControl {
    bool reject = false;
    int rejected = 0;
};
My9221AllocationControl& my9221AllocationControl() {
    static My9221AllocationControl control;
    return control;
}
void* my9221ControlledAllocate(fl::size bytes) {
    if (my9221AllocationControl().reject && bytes == 130) {
        ++my9221AllocationControl().rejected;
        return nullptr;
    }
    return fl::malloc(bytes);
}
void my9221ControlledFree(void* ptr) { fl::free(ptr); }

} // anonymous namespace for spi_legacy_golden timing observation

namespace fl {

// Pins92/93 are unused by this host target. Real SAM block code also names
// those numbers, but that platform implementation is not selected here.
template <> class FastPin<92> {
  public:
    typedef volatile RwReg* port_ptr_t;
    typedef RwReg port_t;
    static void setOutput() {}
    static void setInput() {}
    static void hi() { my9221TimeState().pin(false, true); }
    static void lo() { my9221TimeState().pin(false, false); }
    static void toggle() { my9221TimeState().pin(false, !my9221TimeState().data); }
    static void strobe() { toggle(); toggle(); }
};

template <> class FastPin<93> {
  public:
    typedef volatile RwReg* port_ptr_t;
    typedef RwReg port_t;
    static void setOutput() {}
    static void setInput() {}
    static void hi() { my9221TimeState().pin(true, true); }
    static void lo() { my9221TimeState().pin(true, false); }
    static void toggle() { my9221TimeState().pin(true, !my9221TimeState().clock); }
    static void strobe() { toggle(); toggle(); }
};

namespace detail {

// Declare the observation seam without requiring production to implement it.
// Unchanged MY9221Controller still compiles and executes, then fails the actual
// event-width assertion. A future default policy must bind real waits separately.
template <int DATA_PIN, fl::u8 CLOCK_PIN> struct My9221Timing;
template <> struct My9221Timing<92, 93> {
    // Model only: real targets must supply a prior-state-preserving guard.
    struct Guard {
        bool previous;
        Guard() : previous(my9221TimeState().interrupts_enabled) {
            ++my9221TimeState().guard_entries;
            my9221TimeState().interrupts_enabled = false;
        }
        ~Guard() { my9221TimeState().interrupts_enabled = previous; }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    };
    template <fl::u32 NS> static void waitNanoseconds() {
        my9221TimeState().wait(NS);
    }
    static void waitMicroseconds(fl::u32 us) {
        my9221TimeState().wait(static_cast<fl::u64>(us) * 1000);
    }
    static fl::u64 nowNanoseconds() { return my9221TimeState().ns; }
};

} // namespace detail
} // namespace fl

FL_TEST_FILE(FL_FILEPATH) {

namespace spi_legacy_golden {

/// Capturing mock driver: registered at a very high priority so it always
/// wins draw-list dispatch. Snapshots each enqueued frame's encoded bytes
/// (copied at enqueue time, since poll()/show() may free/reuse the source).
class SpiLegacyGoldenMock : public IChannelDriver {
  public:
    int enqueueCount = 0;
    int showCount = 0;
    fl::vector<fl::vector<u8>> capturedData;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override {
        return data && data->isSpi();
    }

    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override {
        if (!data) {
            return;
        }
        ++enqueueCount;
        const auto& src = data->getData();
        fl::vector<u8> copy;
        for (fl::size i = 0; i < src.size(); ++i) {
            copy.push_back(src[i]);
        }
        capturedData.push_back(copy);
        data->setInUse(false);
    }

    void show() FL_NO_EXCEPT override { ++showCount; }

    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("SPI_GOLDEN_MOCK");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(false, true);  // SPI only
    }

    void reset() {
        enqueueCount = 0;
        showCount = 0;
        capturedData.clear();
    }
};

inline SpiLegacyGoldenMock& mockDriverInstance() {
    static SpiLegacyGoldenMock driver;
    return driver;
}

/// Registers the shared mock singleton exactly once for the whole file, at
/// a priority high enough to beat any other driver registered by other test
/// files sharing this process's ChannelManager.
inline void ensureMockRegistered() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    ChannelManager::instance().addDriver(100000,
        fl::make_shared_no_tracking(mockDriverInstance()));
    // On host, Bus::AUTO resolves to Bus::BIT_BANG (fl/channels/bus.h), so
    // the legacy addLeds<> path pins a "BIT_BANG" affinity and
    // selectDriverForChannel() would return the real BIT_BANG driver ahead
    // of priority dispatch. Make the mock exclusive: BIT_BANG (including any
    // later idempotent re-registration) is disabled, findDriverByName()
    // skips it, and dispatch falls through to the mock for every channel.
    ChannelManager::instance().setExclusiveDriverByName("SPI_GOLDEN_MOCK");
}

/// LED buffers are function-static: controllers registered by earlier cases
/// persist and are re-shown by later cases, so their buffers must outlive
/// the case that created them.
///
/// Test fixture: clears capture state and normalizes FastLED global state
/// before each case. Does NOT remove previously-registered addLeds<>()
/// controllers -- they are function-local statics that persist for the
/// life of the process (see file header) -- so every FastLED.show() call
/// re-enqueues every controller/channel added by every earlier case in this
/// file. mockDriverInstance().capturedData.back() is therefore always the
/// most-recently-added channel's frame: the legacy path always appends its
/// static ChannelPtr's channel after all previously-existing ones.
struct SpiLegacyGoldenFixture {
    SpiLegacyGoldenFixture() {
        ensureMockRegistered();
        mockDriverInstance().reset();
        FastLED.setBrightness(255);
        FastLED.setDither(DISABLE_DITHER);
    }
};

/// Builds the "reference" frame the same way FastLED.h's Channel-API
/// addLeds<>() does: SpiEncoder -> SpiChipsetConfig -> ChannelConfig ->
/// FastLED.add(). Returns the captured bytes for that channel (always the
/// last entry captured by the immediately-following FastLED.show()).
template <typename... Unused>
struct Unused_ {};

}  // namespace spi_legacy_golden

using namespace spi_legacy_golden;

// ===========================================================================
// APA102
// ===========================================================================

FL_TEST_CASE("APA102 default overload (RGB order, default speed) golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<APA102, 40, 41>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // Differential: build the same ChannelConfig directly, the way
    // FastLED.h's Channel-API addLeds<>() does for the default overload
    // (RGB order, default speed).
    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(42, 43, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy.size(), reference.size());
    FL_CHECK_EQ(legacy, reference);

    // Structural invariants, hand-derived from
    // src/fl/chipsets/encoders/apa102.h: 4-byte zero start frame, then
    // [0xE0|bri5][B][G][R] per LED, then (n/32)+1 dwords of 0xFF.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);
    FL_CHECK_EQ(legacy[4] & 0xE0, 0xE0);
    FL_CHECK_EQ(legacy[8] & 0xE0, 0xE0);
    FL_CHECK_EQ(legacy[12], 0xFF);
    FL_CHECK_EQ(legacy[13], 0xFF);
    FL_CHECK_EQ(legacy[14], 0xFF);
    FL_CHECK_EQ(legacy[15], 0xFF);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("APA102 explicit BGR order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<APA102, 44, 45, BGR>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(46, 47, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), BGR);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

FL_TEST_CASE("APA102 brightness 64 golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<APA102, 48, 49>(leds, 2);
    FastLED.setBrightness(64);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived (global brightness scaling
    // pipeline is not safely hand-derivable byte-for-byte).
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(50, 51, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    // FASTLED_USE_GLOBAL_BRIGHTNESS defaults to 0 (fastled_config.h), so
    // writeAPA102() takes the full-brightness path (apa102.h encodeAPA102,
    // hard-coded global_brightness=31): the 5-bit header field is pinned at
    // 0xE0|0x1F regardless of FastLED.setBrightness(); the RGB scaling
    // happens upstream in the pixel controller instead.
    FL_CHECK_EQ(legacy[4] & 0x1F, 0x1F);
    FL_CHECK_EQ(legacy[8] & 0x1F, 0x1F);

    FastLED.setBrightness(255);
    FastLED.remove(refChannel);
}

// ===========================================================================
// SK9822
// ===========================================================================

FL_TEST_CASE("SK9822 default overload golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<SK9822, 52, 53>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    SpiChipsetConfig spiCfg(54, 55, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    // Same 4+4n+end framing as APA102 (encoders/apa102.h / sk9822.h share
    // the [0xE0|bri5][B][G][R] wire layout).
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("SK9822 brightness 64 golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<SK9822, 56, 57>(leds, 2);
    FastLED.setBrightness(64);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    SpiChipsetConfig spiCfg(58, 59, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.setBrightness(255);
    FastLED.remove(refChannel);
}

// ===========================================================================
// HD107 (turbo APA102)
// ===========================================================================

FL_TEST_CASE("HD107 explicit RGB order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<HD107, 60, 61, RGB>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived. HD107 shares Channel::encodeAPA102
    // (channel.cpp.hpp) with APA102 -- only the SpiEncoder's default clock_hz
    // differs (40 MHz vs 6 MHz), which does not affect the emitted bytes.
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD107);
    SpiChipsetConfig spiCfg(62, 63, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

// ===========================================================================
// HD108
// ===========================================================================

FL_TEST_CASE("HD108 default overload defaults to GRB order") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<HD108, 64, 65>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // FastLED.h's default-order Channel-API overload special-cases HD108 to
    // GRB (src/FastLED.h: "HD108 defaults to GRB; all other SPI chipsets
    // default to RGB"). Build the differential reference with GRB explicitly
    // to pin that behaviour.
    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD108);
    SpiChipsetConfig spiCfg(66, 67, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), GRB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/pixel_iterator.h
    // PixelIterator::writeHD108(): 8-byte zero start frame, 2-byte gain
    // header (0xFF 0xFF per encoder_utils.h hd108BrightnessHeader) + 3x
    // 16-bit big-endian channels per LED, then (n/2 + 4) bytes of 0xFF.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(8 + 2 * 8 + (2 / 2 + 4)));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[7], 0x00);
    FL_CHECK_EQ(legacy[8], 0xFF);
    FL_CHECK_EQ(legacy[9], 0xFF);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("HD108 explicit RGB order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<HD108, 68, 69, RGB>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD108);
    SpiChipsetConfig spiCfg(70, 71, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(8 + 2 * 8 + (2 / 2 + 4)));

    FastLED.remove(refChannel);
}

// ===========================================================================
// WS2801
// ===========================================================================

FL_TEST_CASE("WS2801 golden bytes: 3 bytes/LED, no frame overhead") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<WS2801, 72, 73>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::WS2801);
    SpiChipsetConfig spiCfg(74, 75, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    // Structural invariant, hand-derived from
    // src/fl/chipsets/encoders/ws2801.h: exactly 3 RGB-order bytes per LED,
    // no start/end frame.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(3 * 2));

    FastLED.remove(refChannel);
}

// ===========================================================================
// LPD8806
// ===========================================================================

FL_TEST_CASE("LPD8806 golden bytes: GRB MSB-set encoding + latch") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[3] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80), CRGB(0x00, 0x00, 0x00)};
    FastLED.addLedsSpiChannel<LPD8806, 76, 77>(leds, 3);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD8806);
    SpiChipsetConfig spiCfg(78, 79, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 3), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/lpd8806.h:
    // 3 GRB bytes/LED (each with MSB 0x80 set), then
    // (num_leds*3 + 63) / 64 zero latch bytes.
    const fl::size ledBytes = 3 * 3;
    const fl::size latchBytes = (3 * 3 + 63) / 64;
    FL_REQUIRE_EQ(legacy.size(), ledBytes + latchBytes);
    for (fl::size i = 0; i < ledBytes; ++i) {
        FL_CHECK_EQ(legacy[i] & 0x80, 0x80);
    }
    for (fl::size i = ledBytes; i < legacy.size(); ++i) {
        FL_CHECK_EQ(legacy[i], 0x00);
    }

    FastLED.remove(refChannel);
}

// ===========================================================================
// P9813
// ===========================================================================

FL_TEST_CASE("P9813 golden bytes: flag-byte encoding + boundaries") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<P9813, 80, 81>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::P9813);
    SpiChipsetConfig spiCfg(82, 83, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/p9813.h:
    // 4-byte zero start boundary, [flag][B][G][R] per LED (flag = 0xC0 |
    // checksum), 4-byte zero end boundary.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);
    FL_CHECK_EQ(legacy[4] & 0xC0, 0xC0);
    FL_CHECK_EQ(legacy[8] & 0xC0, 0xC0);
    const fl::size endOffset = 4 + 4 * 2;
    FL_CHECK_EQ(legacy[endOffset + 0], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 1], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 2], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 3], 0x00);

    FastLED.remove(refChannel);
}

// ===========================================================================
// Full 5-param form: chipset, pins, order, and explicit data rate
// ===========================================================================

FL_TEST_CASE("APA102 full 5-param form (explicit DATA_RATE_MHZ) golden bytes") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLedsSpiChannel<APA102, 84, 85, RGB, DATA_RATE_MHZ(12)>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived. DATA_RATE_MHZ(12) only changes
    // the SpiEncoder's clock_hz, which is not part of the encoded byte
    // stream -- pinned here for completeness of the addLeds<> overload set.
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102, DATA_RATE_MHZ(12));
    SpiChipsetConfig spiCfg(86, 87, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

// ===========================================================================
// Bus pinning: host/stub has no BusTraits<Bus::SPI> specialization, so pin
// the host's actual DefaultBus<SpiChipsetConfig>::value (Bus::BIT_BANG on
// FL_IS_STUB / FL_IS_WASM, src/fl/channels/bus.h) instead of Bus::SPI.
// ===========================================================================

FL_TEST_CASE("APA102 with explicit host default bus compiles and routes consistently") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    constexpr fl::Bus kHostDefaultSpiBus = fl::DefaultBus<fl::SpiChipsetConfig>::value;
    ::CLEDController& controller =
        FastLED.addLedsSpiChannel<APA102, 88, 89, RGB, DATA_RATE_MHZ(6), kHostDefaultSpiBus>(leds, 2);
    (void)controller;

    // Does not crash, and the frame is still routed to the SPI-accepting
    // mock driver consistently with every other case in this file.
    FastLED.show();
    FL_CHECK_GT(mockDriverInstance().enqueueCount, 0);
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const fl::vector<u8> legacy = mockDriverInstance().capturedData.back();
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
}

// ===========================================================================
// MY9221 (#4636): not an SPI-channel chipset. Its addLedsSpiChannel<> body
// must hand back the bit-bang MY9221Controller (FastPin-driven), not an
// fl::Channel. Tests build with -fno-rtti, so instead of dynamic_cast this
// pins the observable consequence: adding the MY9221 controller adds no
// frame to what the capturing channel driver receives.
// ===========================================================================

FL_TEST_CASE("MY9221 legacy SPI-channel addLeds uses bit-bang MY9221Controller, not fl::Channel") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[4] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80),
                           CRGB(0x01, 0x02, 0x03), CRGB(0x00, 0x00, 0x00)};

    // Baseline: frames enqueued per show() by every channel registered by
    // earlier cases in this file.
    FastLED.show();
    const int baselineEnqueues = mockDriverInstance().enqueueCount;
    mockDriverInstance().reset();

    ::CLEDController& c = FastLED.addLedsSpiChannel<MY9221, 90, 91, RGB>(leds, 4);
    FL_CHECK(static_cast<void*>(&c) != nullptr);
    // New controllers default to binary dither; the fixture's setDither()
    // ran before this one existed, so disable it here for exact wire words.
    c.setDither(DISABLE_DITHER);

    my9221_wire::sampledBits().clear();
    FastLED.show();
    // The MY9221 path writes through FastPin, never the channel driver, so
    // the mock sees exactly the same number of frames as before.
    FL_CHECK_EQ(mockDriverInstance().enqueueCount, baselineEnqueues);

    // Wire check: command word 0x0010, then OUT3..OUT0 as 16-bit grayscale
    // words (8-bit values, MSB first), one DI sample per DCKI edge.
    const fl::u16 expectedWords[13] = {
        0x0010,
        0x00, 0x00, 0x00,  // OUT3 <- leds[3]
        0x01, 0x02, 0x03,  // OUT2 <- leds[2]
        0xFF, 0x00, 0x80,  // OUT1 <- leds[1]
        0x10, 0x20, 0x30,  // OUT0 <- leds[0]
    };
    const fl::vector<fl::u8>& bits = my9221_wire::sampledBits();
    FL_REQUIRE_EQ(bits.size(), (fl::size)(13 * 16));
    for (int w = 0; w < 13; ++w) {
        fl::u16 got = 0;
        for (int b = 0; b < 16; ++b) {
            got = static_cast<fl::u16>((got << 1) | bits[w * 16 + b]);
        }
        FL_CHECK_EQ(got, expectedWords[w]);
    }
}



// Synchronous draw-list isolation: older cases retain static controllers on the
// same modeled pins. Restore their enabled state without removing ownership.
struct My9221DrawIsolation {
    fl::vector<CLEDController*> previously_enabled;
    My9221DrawIsolation() {
        for (CLEDController* c = CLEDController::head(); c; c = c->next()) {
            if (c->getEnabled()) previously_enabled.push_back(c);
            c->setEnabled(false);
        }
    }
    ~My9221DrawIsolation() {
        for (CLEDController* c = CLEDController::head(); c; c = c->next()) c->setEnabled(false);
        for (CLEDController* c : previously_enabled) c->setEnabled(true);
    }
};

static void resetMy9221Capture() {
    My9221TimeState& state = my9221TimeState();
    state.ns = 0; state.data = false; state.clock = false; state.events.clear();
    state.interrupts_enabled = true; state.guard_entries = 0;
}

template <fl::size N>
static void checkMy9221LiteralWords(const fl::u16 (&expected)[N], fl::u64 stop_ns) {
    const My9221TimeState& state = my9221TimeState();
    fl::vector<My9221TimeEvent> edges;
    fl::size last_clock_index = 0;
    for (fl::size i = 0; i < state.events.size(); ++i) {
        if (state.events[i].kind == My9221TimeEvent::Clock) {
            edges.push_back(state.events[i]); last_clock_index = i;
        }
    }
    FL_REQUIRE_EQ(edges.size(), N * 16);
    for (fl::size word = 0; word < N; ++word) {
        fl::u16 sampled = 0;
        for (int bit = 0; bit < 16; ++bit) {
            sampled = static_cast<fl::u16>((sampled << 1) |
                (edges[word * 16 + bit].data ? 1 : 0));
        }
        FL_CHECK_EQ(sampled, expected[word]);
    }
    checkMy9221Timing(state, edges, last_clock_index, stop_ns);
    FL_CHECK_TRUE(state.interrupts_enabled);
    FL_CHECK_EQ(state.guard_entries, 1);
}

FL_TEST_CASE("MY9221 public controller observes DDR and four timed DI latch pulses") {
    SpiLegacyGoldenFixture fixture;
    static CRGB leds[4] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80),
                           CRGB(0x01, 0x02, 0x03), CRGB(0x00, 0x00, 0x00)};
    CLEDController& controller =
        FastLED.addLedsSpiChannel<MY9221, 92, 93, RGB>(leds, 4);
    controller.setDither(DISABLE_DITHER);
    My9221TimeState& state = my9221TimeState();
    state.ns = 0;
    state.data = false;
    state.clock = false;
    state.events.clear();
    FastLED.show(); // Actual public production controller, never a copied writer.

    fl::vector<My9221TimeEvent> edges;
    fl::size last_clock_index = 0;
    for (fl::size i = 0; i < state.events.size(); ++i) {
        if (state.events[i].kind == My9221TimeEvent::Clock) {
            edges.push_back(state.events[i]);
            last_clock_index = i;
        }
    }
    FL_REQUIRE_EQ(edges.size(), static_cast<fl::size>(208));
    const fl::u16 expected_words[13] = {
        0x0010, 0, 0, 0, 1, 2, 3, 0xFF, 0, 0x80, 0x10, 0x20, 0x30,
    };
    for (int word = 0; word < 13; ++word) {
        fl::u16 sampled = 0;
        for (int bit = 0; bit < 16; ++bit) {
            sampled = static_cast<fl::u16>((sampled << 1) |
                                          (edges[word * 16 + bit].data ? 1 : 0));
        }
        FL_CHECK_EQ(sampled, expected_words[word]);
    }

    checkMy9221Timing(state, edges, last_clock_index, 210);
}


FL_TEST_CASE("MY9221 partial BGR chains repeat with final HIGH and cascade stop") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[17];
    for (CRGB& led : leds) led = CRGB(1, 0x55, 0x80);
    // Host controller destructor removes this real producer from the draw list.
    MY9221Controller<92, 93, BGR> controller;
    controller.init();
    controller.setDither(DISABLE_DITHER);
    const fl::u16 full[13] = {
        0x10, 0x80, 0x55, 1, 0x80, 0x55, 1, 0x80, 0x55, 1, 0x80, 0x55, 1};
    const fl::u16 one[13] = {0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x55, 1};
    const fl::u16 three[13] = {0x10, 0, 0, 0, 0x80, 0x55, 1, 0x80, 0x55, 1, 0x80, 0x55, 1};
    const int lengths[] = {1, 3, 17};
    for (int count : lengths) {
        controller.setLeds(leds, count);
        for (int repeat = 0; repeat < 2; ++repeat) {
            My9221TimeState& state = my9221TimeState();
            state.ns = 0; state.data = false; state.clock = false; state.events.clear();
            controller.showLeds(255); // Public production entrypoint.
            fl::vector<My9221TimeEvent> edges;
            fl::size last_clock_index = 0;
            for (fl::size i = 0; i < state.events.size(); ++i) {
                if (state.events[i].kind == My9221TimeEvent::Clock) {
                    edges.push_back(state.events[i]); last_clock_index = i;
                }
            }
            const int chips = count == 17 ? 5 : 1;
            FL_REQUIRE_EQ(edges.size(), static_cast<fl::size>(chips * 208));
            FL_CHECK_TRUE(edges.back().data);
            for (int chip = 0; chip < chips; ++chip) {
                const fl::u16* expected = chip < chips - 1 ? full : (count == 3 ? three : one);
                for (int word = 0; word < 13; ++word) {
                    fl::u16 sampled = 0;
                    for (int bit = 0; bit < 16; ++bit) {
                        sampled = static_cast<fl::u16>((sampled << 1) |
                            (edges[(chip * 13 + word) * 16 + bit].data ? 1 : 0));
                    }
                    FL_CHECK_EQ(sampled, expected[word]);
                }
            }
            // N=5 requires >250ns. The240ns DI low wait alone cannot pass.
            checkMy9221Timing(state, edges, last_clock_index, count == 17 ? 250 : 210);
        }
    }
    controller.setLeds(leds, 0);
    My9221TimeState& state = my9221TimeState();
    state.ns = 0; state.events.clear();
    controller.showLeds(255);
    FL_CHECK_TRUE(state.events.empty());
    FL_CHECK_EQ(state.ns, static_cast<fl::u64>(0));
}


FL_TEST_CASE("MY9221 ordinary and legacy global GRB routes match literal partial payload") {
    SpiLegacyGoldenFixture fixture;
    My9221DrawIsolation isolated;
    static CRGB leds[5] = {CRGB(1, 2, 3), CRGB(4, 5, 6), CRGB(7, 8, 9),
                           CRGB(10, 11, 12), CRGB(13, 14, 15)};
    CLEDController& ordinary = FastLED.addLeds<MY9221, 92, 93, GRB>(leds, 5);
    CLEDController& legacy = FastLED.addLedsSpiChannel<MY9221, 92, 93, GRB>(leds, 5);
    const fl::u16 expected[26] = {
        0x10, 11, 10, 12, 8, 7, 9, 5, 4, 6, 2, 1, 3,
        0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 13, 15};
    // Each route must independently satisfy the external literal, not merely
    // equal another implementation's output. Other draw-list entries are off.
    CLEDController* routes[] = {&ordinary, &legacy};
    for (CLEDController* selected : routes) {
        ordinary.setEnabled(false); legacy.setEnabled(false);
        selected->setEnabled(true);
        selected->setDither(DISABLE_DITHER);
        selected->setCorrection(CRGB(255, 255, 255));
        selected->setTemperature(CRGB(255, 255, 255));
        resetMy9221Capture();
        FastLED.show(255);
        checkMy9221LiteralWords(expected, 220);
    }
}

FL_TEST_CASE("MY9221 public scale correction and binary dither have literal frame payloads") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[4] = {CRGB(9, 17, 33), CRGB(13, 23, 45),
                    CRGB(0, 0, 0), CRGB(255, 1, 2)};
    MY9221Controller<92, 93, RGB> controller;
    controller.init(); controller.setLeds(leds, 4);
    controller.setCorrection(CRGB(255, 127, 63));
    controller.setTemperature(CRGB(255, 255, 255));
    // brightness128 => adjustment(128,64,32). Fixed scale is floor(x*(s+1)/256).
    // Phase0 Q16 => offsets(0,0,0), alternating with(2,4,8).
    // Phase1 Q144 => offsets(1,2,4), same after the alternating complement.
    // Black stays black; 255 saturates before scaling. No production scaling,
    // dither or serializer helper computes these independent literal words.
    const fl::u16 disabled[13] = {0x10, 128, 0, 0, 0, 0, 0, 6, 5, 5, 4, 4, 4};
    const fl::u16 phase0[13] = {0x10, 128, 1, 1, 0, 0, 0, 7, 6, 6, 4, 4, 4};
    const fl::u16 phase1[13] = {0x10, 128, 0, 0, 0, 0, 0, 7, 6, 6, 5, 4, 4};
    controller.setDither(DISABLE_DITHER);
    resetMy9221Capture(); controller.showLeds(128);
    checkMy9221LiteralWords(disabled, 210);
    const fl::u8 saved_phase = fl::detail::gDitherFrame;
    auto restore_phase = fl::make_scope_exit([saved_phase] { fl::detail::gDitherFrame = saved_phase; });
    controller.setDither(BINARY_DITHER);
    for (int repeat = 0; repeat < 2; ++repeat) {
        // Public showLeds begins a frame and advances the shared phase once.
        fl::detail::gDitherFrame = 255;
        resetMy9221Capture(); controller.showLeds(128);
        checkMy9221LiteralWords(phase0, 210);
        fl::detail::gDitherFrame = 0;
        resetMy9221Capture(); controller.showLeds(128);
        checkMy9221LiteralWords(phase1, 210);
    }
}

FL_TEST_CASE("MY9221 real dynamic reserve failure emits no partial frame and retries") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[17];
    for (CRGB& led : leds) led = CRGB(1, 2, 3);
    MY9221Controller<92, 93, RGB> controller; // Fresh real dynamic scratch vector.
    controller.init();
    controller.setLeds(leds, 17);
    controller.setDither(DISABLE_DITHER);
    My9221TimeState& state = my9221TimeState();
    state.ns = 0; state.data = false; state.clock = false; state.events.clear();
    state.interrupts_enabled = true;
    state.guard_entries = 0;
    My9221AllocationControl& allocation = my9221AllocationControl();
    allocation.rejected = 0;
    allocation.reject = true;
    // In this host target the default resource uses fl::Malloc->Alloc. Reject
    // the actual130-byte vector reserve, not an exposed/fixed scratch stand-in.
    fl::SetPSRamAllocator(my9221ControlledAllocate, my9221ControlledFree);
    auto restore = fl::make_scope_exit([] {
        my9221AllocationControl().reject = false;
        fl::SetPSRamAllocator(fl::malloc, fl::free); // Known host default route.
    });
    controller.showLeds(255);
    FL_CHECK_EQ(allocation.rejected, 1);
    FL_CHECK_EQ(state.guard_entries, 0);
    FL_CHECK_TRUE(state.interrupts_enabled);
    FL_CHECK_TRUE(state.events.empty());
    FL_CHECK_EQ(state.ns, static_cast<fl::u64>(0));
    allocation.reject = false;
    resetMy9221Capture(); controller.showLeds(255);
    const fl::u16 expected[65] = {
        0x10,1,2,3,1,2,3,1,2,3,1,2,3,
        0x10,1,2,3,1,2,3,1,2,3,1,2,3,
        0x10,1,2,3,1,2,3,1,2,3,1,2,3,
        0x10,1,2,3,1,2,3,1,2,3,1,2,3,
        0x10,0,0,0,0,0,0,0,0,0,1,2,3};
    checkMy9221LiteralWords(expected, 250);
    leds[0] = CRGB(4, 5, 6);
    fl::u16 updated[65];
    for (int i = 0; i < 65; ++i) updated[i] = expected[i];
    updated[10] = 4; updated[11] = 5; updated[12] = 6;
    resetMy9221Capture(); controller.showLeds(255);
    checkMy9221LiteralWords(updated, 250);
    controller.setLeds(leds, 0);
    resetMy9221Capture(); controller.showLeds(255);
    FL_CHECK_TRUE(state.events.empty());
    FL_CHECK_EQ(state.ns, static_cast<fl::u64>(0));
    FL_CHECK_EQ(state.guard_entries, 0);
    FL_CHECK_TRUE(state.interrupts_enabled);
}

FL_TEST_CASE("MY9221 public emission guards edges and restores prior interrupt state") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[1] = {CRGB(1, 2, 3)};
    MY9221Controller<92, 93, RGB> controller;
    controller.init();
    controller.setDither(DISABLE_DITHER);
    My9221TimeState& state = my9221TimeState();
    auto restore = fl::make_scope_exit([&state] { state.interrupts_enabled = true; });
    for (bool initially_enabled : {true, false}) {
        controller.setLeds(leds, 1);
        state.ns = 0; state.data = false; state.clock = false; state.events.clear();
        state.interrupts_enabled = initially_enabled;
        state.guard_entries = 0;
        controller.showLeds(255);
        fl::size edges = 0;
        for (const My9221TimeEvent& event : state.events) {
            if (event.kind == My9221TimeEvent::Clock) {
                ++edges;
                FL_CHECK_FALSE(event.interrupts_enabled);
            }
            // The long minimum-only latch wait must not extend the mask.
            if (event.kind == My9221TimeEvent::Wait && event.ns > 220000) {
                FL_CHECK_EQ(event.interrupts_enabled, initially_enabled);
            }
        }
        FL_CHECK_EQ(edges, static_cast<fl::size>(208));
        FL_CHECK_EQ(state.guard_entries, 1);
        FL_CHECK_EQ(state.interrupts_enabled, initially_enabled);
        controller.setLeds(leds, 0);
        state.events.clear(); state.guard_entries = 0;
        controller.showLeds(255);
        FL_CHECK_TRUE(state.events.empty());
        FL_CHECK_EQ(state.guard_entries, 0);
        FL_CHECK_EQ(state.interrupts_enabled, initially_enabled);
    }
}

}  // FL_TEST_FILE
