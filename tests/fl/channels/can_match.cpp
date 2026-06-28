/// @file tests/fl/channels/can_match.cpp
/// @brief Unit tests for fl::canMatch against synthetic
///        DriverCapabilities + PinGroup[] fixtures (#3186).
///
/// These tests instantiate NO driver class — they construct the
/// capability data directly. That's the whole point of the free
/// function: matching logic is testable without any driver or
/// peripheral mocks.

#include "fl/channels/can_match.h"
#include "fl/channels/capabilities.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

// ---------- Fixture helpers ------------------------------------------

namespace {

// RMT5-style: clockless any-pin, generous timing.
DriverCapabilities makeRmtCaps() FL_NO_EXCEPT {
    DriverCapabilities c;
    c.name = fl::string_view("RMT5");
    c.priority = 7;
    c.supports_clockless = true;
    c.supports_spi = false;
    c.max_total_channels = 8;
    c.clockless_frequency = FreqRange{300000u, 2000000u};
    c.spi_frequency = FreqRange::any();
    // 80 MHz APB, dividers 1..255, 15-bit phase counter.
    c.clockless_timing = ClocklessTimingCapability{
        80000000u, 1u, 255u, 32767u
    };
    return c;
}

PinGroup makeRmtClocklessGroup(fl::u16 instance_id = 0, fl::u8 cap = 1) FL_NO_EXCEPT {
    PinGroup g;
    g.instance_id = instance_id;
    g.protocol = Protocol::Clockless;
    g.data_pins = PinSet::anyOutput();
    g.clock_pins = PinSet::none();
    g.max_concurrent = cap;
    g.frequency = FreqRange::any();
    return g;
}

DriverCapabilities makeSpiCaps() FL_NO_EXCEPT {
    DriverCapabilities c;
    c.name = fl::string_view("SPI");
    c.priority = 5;
    c.supports_clockless = false;
    c.supports_spi = true;
    c.max_total_channels = 2;
    c.clockless_frequency = FreqRange::any();
    c.spi_frequency = FreqRange{78000u, 80000000u};
    return c;
}

PinGroup makeFixedSpiGroup() FL_NO_EXCEPT {
    static fl::i16 kData[] = {11};
    static fl::i16 kClock[] = {12};
    PinGroup g;
    g.instance_id = 2;
    g.protocol = Protocol::Spi;
    g.data_pins = PinSet::allowlist(kData);
    g.clock_pins = PinSet::allowlist(kClock);
    g.max_concurrent = 1;
    g.frequency = FreqRange{78000u, 80000000u};
    return g;
}

PinGroup makeSharedClockSpiGroup() FL_NO_EXCEPT {
    static fl::i16 kData[] = {9, 10, 11, 12};
    static fl::i16 kClock[] = {16};
    PinGroup g;
    g.instance_id = 1;
    g.protocol = Protocol::Spi;
    g.data_pins = PinSet::allowlist(kData);
    g.clock_pins = PinSet::allowlist(kClock);
    g.max_concurrent = 4;
    g.frequency = FreqRange::atMost(30000000u);
    return g;
}

ChipsetClocklessTiming makeWs2812Timing() FL_NO_EXCEPT {
    return ChipsetClocklessTiming{
        NanosRange{200u, 500u},
        NanosRange{700u, 1000u},
        NanosRange{700u, 1000u},
        NanosRange{200u, 500u},
    };
}

ChannelRequest makeBulkClocklessRequest(fl::i16 start, fl::u32 count, fl::i16 stride = 1) FL_NO_EXCEPT {
    ChannelRequest r;
    r.protocol = Protocol::Clockless;
    r.clock_pin = -1;
    r.timing = makeWs2812Timing();
    for (fl::u32 i = 0; i < count; ++i) {
        fl::i16 p = start + static_cast<fl::i16>(i) * stride;
        if (p >= 0 && static_cast<fl::u32>(p) < kPinSetCapacity) {
            r.data_pins.set(static_cast<fl::u32>(p));
        }
    }
    return r;
}

}  // namespace

// ---------- Single-pin matching --------------------------------

FL_TEST_CASE("canMatch: RMT-style caps accept any clockless pin") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: SPI driver rejects clockless request with NoProtocol") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoProtocol);
}

FL_TEST_CASE("canMatch: clockless driver rejects SPI request with NoProtocol") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 12);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoProtocol);
}

FL_TEST_CASE("canMatch: empty groups yields NoPin even when protocol matches") {
    auto caps = makeRmtCaps();
    fl::span<const PinGroup> empty_groups;
    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, empty_groups, req) == HandleResult::NoPin);
}

FL_TEST_CASE("canMatch: empty data_pins bitset yields NoPin") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup() };
    ChannelRequest empty_req;     // no bits set
    empty_req.protocol = Protocol::Clockless;
    FL_REQUIRE(fl::canMatch(caps, groups, empty_req) == HandleResult::NoPin);
}

// ---------- Fixed-pin SPI ---------------------------------------------

FL_TEST_CASE("canMatch: fixed SPI pair (11,12) matches its exact group") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 12);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: fixed SPI rejects wrong data pin with NoPin") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 9, 12);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPin);
}

FL_TEST_CASE("canMatch: fixed SPI rejects wrong clock pin with NoPinPair") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 10);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPinPair);
}

// ---------- Shared-clock SPI (FlexIO-style) --------------------------

FL_TEST_CASE("canMatch: shared-clock group accepts any data pin from allowlist + shared clock") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeSharedClockSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 10, 16);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: shared-clock group rejects mismatched clock with NoPinPair") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeSharedClockSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 10, 17);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPinPair);
}

// ---------- Frequency ranges ------------------------------------------

FL_TEST_CASE("canMatch: SPI frequency 10 MHz fits range") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 12,
        ChipsetClocklessTiming::unspecified(), fl::optional<fl::u32>(10000000u));
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: SPI frequency 100 MHz exceeds range -> NoFrequency") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 12,
        ChipsetClocklessTiming::unspecified(), fl::optional<fl::u32>(100000000u));
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoFrequency);
}

FL_TEST_CASE("FreqRange any() / exactly() / atLeast() / atMost() behave correctly") {
    FL_REQUIRE(FreqRange::any().isUnspecified());
    FL_REQUIRE(FreqRange::any().contains(0u));
    FL_REQUIRE(FreqRange::any().contains(0xFFFFFFFFu));
    FL_REQUIRE(!FreqRange::exactly(800000u).contains(799999u));
    FL_REQUIRE(FreqRange::exactly(800000u).contains(800000u));
    FL_REQUIRE(!FreqRange::exactly(800000u).contains(800001u));
    FL_REQUIRE(FreqRange::atLeast(1000u).contains(1000u));
    FL_REQUIRE(FreqRange::atLeast(1000u).contains(0xFFFFFFFFu));
    FL_REQUIRE(!FreqRange::atLeast(1000u).contains(999u));
    FL_REQUIRE(FreqRange::atMost(1000u).contains(0u));
    FL_REQUIRE(FreqRange::atMost(1000u).contains(1000u));
    FL_REQUIRE(!FreqRange::atMost(1000u).contains(1001u));
}

// ---------- Clockless timing: divider loop ---------------------------

FL_TEST_CASE("canMatch: RMT80MHz hits WS2812 windows at some divider") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: 1 MHz driver (tick=1000ns) can't hit T0H window {60,100} -> NoTiming") {
    DriverCapabilities caps;
    caps.name = fl::string_view("Slow1MHz");
    caps.priority = 1;
    caps.supports_clockless = true;
    caps.clockless_frequency = FreqRange::any();
    caps.clockless_timing = ClocklessTimingCapability{
        1000000u, 1u, 1u, 32767u
    };
    PinGroup groups[] = { makeRmtClocklessGroup() };

    ChannelRequest req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1,
        ChipsetClocklessTiming{
            NanosRange{60u, 100u},
            NanosRange{200u, 600u},
            NanosRange{200u, 600u},
            NanosRange{60u, 100u},
        });
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoTiming);
}

FL_TEST_CASE("canMatch: clockless_timing unspecified means timing is trivially compatible") {
    DriverCapabilities caps;
    caps.name = fl::string_view("DmaPattern");
    caps.priority = 1;
    caps.supports_clockless = true;
    caps.clockless_frequency = FreqRange::any();
    caps.clockless_timing = ClocklessTimingCapability::unspecified();
    PinGroup groups[] = { makeRmtClocklessGroup() };

    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

// ---------- PinSet semantics ------------------------------------------

FL_TEST_CASE("PinSet::AnyOutputPin accepts pins 0..127 and rejects negatives and out-of-range") {
    PinSet ps = PinSet::anyOutput();
    FL_REQUIRE(ps.accepts(0));
    FL_REQUIRE(ps.accepts(4));
    FL_REQUIRE(ps.accepts(127));
    FL_REQUIRE(!ps.accepts(-1));
    FL_REQUIRE(!ps.accepts(128));
}

FL_TEST_CASE("PinSet::None rejects everything") {
    PinSet ps = PinSet::none();
    FL_REQUIRE(!ps.accepts(0));
    FL_REQUIRE(!ps.accepts(4));
    FL_REQUIRE(!ps.accepts(-1));
}

FL_TEST_CASE("PinSet::allowlist accepts only listed pins") {
    static fl::i16 kArr[] = {2, 4, 6};
    PinSet ps = PinSet::allowlist(kArr);
    FL_REQUIRE(ps.accepts(2));
    FL_REQUIRE(ps.accepts(4));
    FL_REQUIRE(ps.accepts(6));
    FL_REQUIRE(!ps.accepts(3));
    FL_REQUIRE(!ps.accepts(5));
    FL_REQUIRE(!ps.accepts(0));
}

FL_TEST_CASE("PinSet::fixed creates a single-pin allowlist") {
    PinSet ps = PinSet::fixed(7);
    FL_REQUIRE(ps.accepts(7));
    FL_REQUIRE(!ps.accepts(6));
    FL_REQUIRE(!ps.accepts(8));
}

FL_TEST_CASE("PinSet::acceptsAll: AnyOutput trivially accepts any bitset") {
    PinSet ps = PinSet::anyOutput();
    PinBitset b;
    b.set(2u); b.set(7u); b.set(33u);
    FL_REQUIRE(ps.acceptsAll(b));
}

FL_TEST_CASE("PinSet::acceptsAll: Allowlist accepts subset, rejects superset with extras") {
    static fl::i16 kArr[] = {2, 4, 6};
    PinSet ps = PinSet::allowlist(kArr);

    PinBitset subset;
    subset.set(2u); subset.set(4u);
    FL_REQUIRE(ps.acceptsAll(subset));

    PinBitset has_extra;
    has_extra.set(2u); has_extra.set(5u);
    FL_REQUIRE(!ps.acceptsAll(has_extra));
}

// ---------- Multi-pin (bulk) matching ---------------------------------

FL_TEST_CASE("canMatch: 4-pin bulk fits group with max_concurrent=4") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup(0, 4) };
    auto req = makeBulkClocklessRequest(2, 4);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: 9-pin bulk on capacity=8 group yields NoCapacity") {
    auto caps = makeRmtCaps();
    PinGroup groups[] = { makeRmtClocklessGroup(0, 8) };
    auto req = makeBulkClocklessRequest(2, 9);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoCapacity);
}

FL_TEST_CASE("canMatch: driver-wide max_total_channels is enforced before group walk") {
    // Driver publishes max_total_channels=4 but a group with capacity 16.
    // A 5-pin bulk request is rejected at the driver-aggregate gate,
    // not silently accepted by the over-generous group.
    auto caps = makeRmtCaps();
    caps.max_total_channels = 4;
    PinGroup groups[] = { makeRmtClocklessGroup(0, 16) };
    auto req = makeBulkClocklessRequest(2, 5);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoCapacity);
}

FL_TEST_CASE("canMatch: precise tick math (u64 ratio) — exactly at upper bound passes") {
    // 80 MHz APB / divider 8 = 10 MHz effective clock.
    // Window {1, 100} for T0H: with u64 ratio math, n_lo = ceil(1*1e7/1e9)
    // = 1, n_hi = floor(100*1e7/1e9) = 1. So N=1 is valid (phase =
    // 1/1e7 sec = 100 ns, inside {1, 100}). Confirms the boundary-precise
    // u64 numerator matches expected behavior.
    DriverCapabilities caps;
    caps.name = fl::string_view("Tight");
    caps.priority = 1;
    caps.supports_clockless = true;
    caps.clockless_frequency = FreqRange::any();
    caps.clockless_timing = ClocklessTimingCapability{
        80000000u, 8u, 8u, 32767u
    };
    PinGroup groups[] = { makeRmtClocklessGroup() };

    auto req = ChannelRequest::singlePin(Protocol::Clockless, 4, -1,
        ChipsetClocklessTiming{
            NanosRange{1u, 100u},
            NanosRange{200u, 800u},
            NanosRange{200u, 800u},
            NanosRange{1u, 100u},
        });
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: bulk with off-allowlist pin yields NoPin") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeSharedClockSpiGroup() };

    ChannelRequest req;
    req.protocol = Protocol::Spi;
    req.data_pins.set(9u);
    req.data_pins.set(13u);   // 13 not in {9,10,11,12}
    req.clock_pin = 16;
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPin);
}

// ---------- Contiguity ------------------------------------------------

FL_TEST_CASE("canMatch: pins_must_be_contiguous accepts a contiguous run") {
    PinGroup g = makeRmtClocklessGroup(0, 4);
    g.pins_must_be_contiguous = true;
    PinGroup groups[] = { g };

    auto caps = makeRmtCaps();
    auto req = makeBulkClocklessRequest(4, 4);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: pins_must_be_contiguous rejects gappy bitset with NoPin") {
    PinGroup g = makeRmtClocklessGroup(0, 4);
    g.pins_must_be_contiguous = true;
    PinGroup groups[] = { g };

    auto caps = makeRmtCaps();
    auto req = makeBulkClocklessRequest(4, 4, /*stride*/ 2);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPin);
}

FL_TEST_CASE("canMatch: single-pin bulk request is trivially contiguous") {
    PinGroup g = makeRmtClocklessGroup(0, 4);
    g.pins_must_be_contiguous = true;
    PinGroup groups[] = { g };

    auto caps = makeRmtCaps();
    auto req = ChannelRequest::singlePin(Protocol::Clockless, 7, -1, makeWs2812Timing());
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

// ---------- Multi-group: best group wins ------------------------------

FL_TEST_CASE("canMatch: among multiple groups, first matching wins") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = {
        makeFixedSpiGroup(),
        makeSharedClockSpiGroup(),
    };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 12);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::Yes);
}

FL_TEST_CASE("canMatch: with two non-matching groups, strongest rejection surfaces") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = {
        makeFixedSpiGroup(),
        makeSharedClockSpiGroup(),
    };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 99, 12);
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPin);
}

FL_TEST_CASE("canMatch: NoPinPair beats NoFrequency in strongest-rejection ranking") {
    auto caps = makeSpiCaps();
    PinGroup groups[] = { makeFixedSpiGroup() };
    auto req = ChannelRequest::singlePin(Protocol::Spi, 11, 99,
        ChipsetClocklessTiming::unspecified(), fl::optional<fl::u32>(100000000u));
    FL_REQUIRE(fl::canMatch(caps, groups, req) == HandleResult::NoPinPair);
}

}  // FL_TEST_FILE
