/// @file rx_sct_capture.cpp
/// @brief Host-side decoder tests for the LPC SCT-capture RX backend
///        scaffold (FastLED#3015).
///
/// The real LPC845 SCT input-capture + DMA path requires bench
/// validation on silicon — that work is the follow-up tracked by #3015.
/// What this test file locks down today is the contract everything else
/// in the loopback chain depends on:
///
///   1. `RxBackend::LPC_SCT_CAPTURE` is dispatchable on every platform
///      (real device on LPC, NativeRxDevice on stub, DummyRxDevice
///      elsewhere). Tested implicitly via the toString round-trip and
///      the factory's non-null guarantee.
///   2. The WS2812 edge-pair → byte decoder accepts a synthetic edge
///      stream and round-trips it to the original GRB bytes.
///   3. `injectEdges()` is fully functional on the LPC backend (the
///      hardware-side capture path will populate the same buffer once
///      the SCT/DMA setup lands).

#include "FastLED.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/channel.h"
#include "fl/channels/rx/config.h"
#include "fl/channels/rx/types.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/cstring.h"
#include "fl/stl/static_assert.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// WS2812B 4-phase decoder timings. Same nominal values as `WS2812Chipset`:
//   T0H = 400 ns, T0L = 850 ns, T1H = 800 ns, T1L = 450 ns.
// We use ±150 ns tolerance so an idealised generator (no jitter) is
// comfortably inside the window without making bit-1 / bit-0 ambiguous.
ChipsetTiming4Phase ws2812_timing() {
    ChipsetTiming4Phase t{};
    t.t0h_min_ns = 250;   t.t0h_max_ns = 550;
    t.t0l_min_ns = 700;   t.t0l_max_ns = 1000;
    t.t1h_min_ns = 650;   t.t1h_max_ns = 950;
    t.t1l_min_ns = 300;   t.t1l_max_ns = 600;
    t.reset_min_us = 50;
    t.gap_tolerance_ns = 0;
    return t;
}

// Emit one byte's worth of edge pairs (MSB first) into the buffer.
template <fl::size N>
void appendByte(fl::vector<EdgeTime>& edges, u8 byte) {
    for (int bit = 7; bit >= 0; --bit) {
        bool one = ((byte >> bit) & 0x1) != 0;
        edges.push_back(EdgeTime(true,  one ? 800u : 400u));   // HIGH
        edges.push_back(EdgeTime(false, one ? 450u : 850u));   // LOW
    }
    (void)N;
}

// Wrap a `fl::vector<EdgeTime>` as a span for injectEdges().
fl::span<const EdgeTime> toSpan(const fl::vector<EdgeTime>& v) {
    return fl::span<const EdgeTime>(v.data(), v.size());  // ok span from pointer — fl::vector → span<const T> conversion does not exist on this code path
}

}  // namespace

FL_TEST_CASE("RxBackend toString includes LPC_SCT_CAPTURE") {
    FL_CHECK(fl::strcmp(toString(RxBackend::LPC_SCT_CAPTURE), "LPC_SCT_CAPTURE") == 0);
    FL_CHECK(fl::strcmp(toString(RxDeviceType::LPC_SCT_CAPTURE), "LPC_SCT_CAPTURE") == 0);
}

FL_TEST_CASE("RxDevice::create<LPC_SCT_CAPTURE> returns a non-null device on every platform") {
    auto device = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(11);  // P0_11 on LPC845-BRK
    FL_REQUIRE(device != nullptr);
    FL_REQUIRE(device->name() != nullptr);
    FL_CHECK(device->getPin() == 11);
}

FL_TEST_CASE("LPC SCT-capture: injectEdges -> decode round-trips a known WS2812 GRB byte stream") {
    auto device = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(11);
    FL_REQUIRE(device != nullptr);

    RxConfig config;
    config.buffer_size = 256;
    config.start_low   = true;
    FL_REQUIRE(device->begin(config));

    // Encode three "LEDs" worth of bytes (9 bytes total, GRB).
    // 0xFF 0x00 0x00 = red    (GRB order, with G/B = 0)
    // 0x00 0xFF 0x00 = green  (GRB order: G first)
    // 0x00 0x00 0xFF = blue
    const u8 kPayload[] = {
        0x00, 0xFF, 0x00,   // green
        0xFF, 0x00, 0x00,   // red
        0x00, 0x00, 0xFF,   // blue
    };
    constexpr size_t kPayloadLen = sizeof(kPayload);

    fl::vector<EdgeTime> edges;
    edges.reserve(kPayloadLen * 16 + 2);
    for (size_t i = 0; i < kPayloadLen; ++i) {
        appendByte<0>(edges, kPayload[i]);
    }
    // Cap the stream with a WS2812 reset pulse so the decoder cleanly
    // terminates on the final byte boundary.
    edges.push_back(EdgeTime(false, 60u * 1000u));  // 60 µs LOW = reset

    FL_REQUIRE(device->injectEdges(toSpan(edges)));

    // Drain the wait() state-machine so finished() returns true. With
    // no real ISR firing on host, this is synchronous.
    FL_CHECK(device->wait(10) == RxWaitResult::SUCCESS);
    FL_CHECK(device->finished());

    u8 out[kPayloadLen + 1] = {0};
    auto result = device->decode(ws2812_timing(), fl::span<u8>(out, sizeof(out)));
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == kPayloadLen);

    for (size_t i = 0; i < kPayloadLen; ++i) {
        FL_CHECK(out[i] == kPayload[i]);
    }
}

FL_TEST_CASE("LPC SCT-capture: decoder rejects an empty edge buffer") {
    auto device = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(11);
    FL_REQUIRE(device != nullptr);

    RxConfig config;
    FL_REQUIRE(device->begin(config));
    // No edges injected — decode should fail with INVALID_ARGUMENT.

    u8 out[8] = {0};
    auto result = device->decode(ws2812_timing(), fl::span<u8>(out, sizeof(out)));
    FL_CHECK(!result.ok());
    FL_CHECK(result.error() == DecodeError::INVALID_ARGUMENT);
}

FL_TEST_CASE("LPC SCT-capture: getRawEdgeTimes mirrors what was injected") {
    auto device = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(11);
    FL_REQUIRE(device != nullptr);

    RxConfig config;
    FL_REQUIRE(device->begin(config));

    fl::vector<EdgeTime> edges;
    appendByte<0>(edges, 0xA5);  // 10100101 — alternating bits exercise both T0/T1
    FL_REQUIRE(device->injectEdges(toSpan(edges)));

    EdgeTime out[16] = {};
    size_t n = device->getRawEdgeTimes(fl::span<EdgeTime>(out, 16));
    FL_CHECK(n == 16);  // 8 bits × 2 edges per bit

    // Spot-check a few edges. Bit 7 = 1, bit 6 = 0, bit 5 = 1, bit 4 = 0.
    FL_CHECK(out[0].high == 1);  FL_CHECK(out[0].ns == 800);   // bit7 high (1)
    FL_CHECK(out[1].high == 0);  FL_CHECK(out[1].ns == 450);   // bit7 low
    FL_CHECK(out[2].high == 1);  FL_CHECK(out[2].ns == 400);   // bit6 high (0)
    FL_CHECK(out[3].high == 0);  FL_CHECK(out[3].ns == 850);   // bit6 low
}

}  // FL_TEST_FILE
