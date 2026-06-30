/// @file lpc_sct_dma_engine.cpp
/// @brief Host-side tests for the LPC845 SCT+DMA channels-API engine
///        (FastLED #3459 scaffold) plus a TX→RX byte-flow demonstration
///        through the LPC SCT-capture RX device.
///
/// The engine itself ships on LPC845 silicon (gated on `FL_IS_ARM_LPC_845`).
/// To exercise the `IChannelDriver` contract (canHandle filtering, state
/// machine, capabilities, name) without hardware, the engine class is also
/// gated to compile under `FASTLED_STUB_IMPL` — same convention as
/// `ChannelEngineObjectFLED` in
/// `tests/platforms/arm/teensy/teensy4_common/drivers/objectfled/`.
///
/// The "TX→RX round-trip" half of this file does NOT call the engine's
/// `show()` body (it is a documented `TODO(3459)` no-op until the SCT
/// chunk-encode lift from `clockless_arm_lpc_pwm_dma.h` lands). Instead it
/// demonstrates the integration shape: the same WS2812 byte stream that the
/// engine's future `show()` will emit, when encoded as a HIGH/LOW edge
/// sequence, round-trips losslessly through the LPC SCT-capture RX device.
/// On host that device falls through to `NativeRxDevice` (per
/// `src/fl/channels/rx.cpp.hpp:266`), so the decode path is the same code
/// that runs on real LPC845 silicon once the RX SCT/DMA `begin()` body is
/// invoked.

#include "FastLED.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/types.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/cstring.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using DriverState = IChannelDriver::DriverState;

namespace {

// WS2812B timing config — same nominal values as the rx_sct_capture tests
// (`tests/fl/channels/rx_sct_capture.cpp`). Total period = T1+T2+T3 = 1250 ns
// → inside the engine's [1000, 2500] ns `canHandle()` window.
ChipsetTimingConfig ws2812_timing_config() {
    return ChipsetTimingConfig(/*T1=*/350, /*T2=*/450, /*T3=*/450,
                               /*reset_us=*/50, "WS2812B");
}

// Out-of-window timing for canHandle rejection — total period 300 ns
// (below the 1000 ns floor; same shape ObjectFLED's tests use to reject
// HD-class chipsets that need different encoding).
ChipsetTimingConfig too_fast_timing_config() {
    return ChipsetTimingConfig(100, 100, 100, 50, "TooFast");
}

ChannelDataPtr makeClocklessChannelData(int pin, fl::size num_leds,
                                         const u8* rgb_bytes,
                                         const ChipsetTimingConfig* timing = nullptr) {
    ChipsetTimingConfig t = timing ? *timing : ws2812_timing_config();
    fl::vector_psram<u8> encoded(num_leds * 3);
    if (rgb_bytes != nullptr) {
        for (fl::size i = 0; i < num_leds * 3; ++i) {
            encoded[i] = rgb_bytes[i];
        }
    }
    return ChannelData::create(pin, t, fl::move(encoded));
}

// WS2812 edge-pair decoder timings — same tolerance as
// `tests/fl/channels/rx_sct_capture.cpp::ws2812_timing()`.
ChipsetTiming4Phase ws2812_decoder_timing() {
    ChipsetTiming4Phase t{};
    t.t0h_min_ns = 250;   t.t0h_max_ns = 550;
    t.t0l_min_ns = 700;   t.t0l_max_ns = 1000;
    t.t1h_min_ns = 650;   t.t1h_max_ns = 950;
    t.t1l_min_ns = 300;   t.t1l_max_ns = 600;
    t.reset_min_us = 50;
    t.gap_tolerance_ns = 0;
    return t;
}

// Encode one byte (MSB first) as 8 WS2812 edge pairs. Same helper shape as
// the existing rx_sct_capture test; reproduced here so the two test files
// stay independently buildable.
void appendByte(fl::vector<EdgeTime>& edges, u8 byte) {
    for (int bit = 7; bit >= 0; --bit) {
        const bool one = ((byte >> bit) & 0x1) != 0;
        edges.push_back(EdgeTime(/*high=*/true,  one ? 800u : 400u));
        edges.push_back(EdgeTime(/*high=*/false, one ? 450u : 850u));
    }
}

fl::span<const EdgeTime> toSpan(const fl::vector<EdgeTime>& v) {
    return fl::span<const EdgeTime>(v.data(), v.size());  // ok span from pointer — fl::vector → span<const T> conversion does not exist on this code path
}

}  // namespace

// =============================================================================
// IChannelDriver contract — name, capabilities, default state
// =============================================================================

FL_TEST_CASE("LPC SCT-DMA engine: name is LPC_SCT_DMA") {
    ChannelEngineLpcSctDma engine;
    FL_CHECK(fl::strcmp(engine.getName().c_str(), "LPC_SCT_DMA") == 0);
}

FL_TEST_CASE("LPC SCT-DMA engine: clockless-only capabilities") {
    ChannelEngineLpcSctDma engine;
    const auto caps = engine.getCapabilities();
    FL_CHECK(caps.supportsClockless == true);
    // SPI is the SIBLING peripheral — handled by spi_arm_lpc_dma.h
    // (PR #3454), NOT by this engine. This is the documented
    // "different-peripheral-same-protocol" exception to the parallel-IO
    // unification rule.
    FL_CHECK(caps.supportsSpi == false);
}

FL_TEST_CASE("LPC SCT-DMA engine: initial state is READY") {
    ChannelEngineLpcSctDma engine;
    FL_CHECK(engine.poll() == DriverState::READY);
}

// =============================================================================
// canHandle() filtering
// =============================================================================

FL_TEST_CASE("LPC SCT-DMA engine: canHandle accepts in-window WS2812 timing") {
    ChannelEngineLpcSctDma engine;
    auto ch = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/3, nullptr);
    FL_CHECK(engine.canHandle(ch) == true);
}

FL_TEST_CASE("LPC SCT-DMA engine: canHandle rejects too-fast timing (< 1000 ns total)") {
    ChannelEngineLpcSctDma engine;
    auto cfg = too_fast_timing_config();
    auto ch = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/3,
                                       nullptr, &cfg);
    FL_CHECK(engine.canHandle(ch) == false);
}

FL_TEST_CASE("LPC SCT-DMA engine: canHandle rejects null channel data") {
    ChannelEngineLpcSctDma engine;
    ChannelDataPtr empty;
    FL_CHECK(engine.canHandle(empty) == false);
}

// =============================================================================
// State machine — enqueue / show / poll transitions
// =============================================================================

FL_TEST_CASE("LPC SCT-DMA engine: show with empty queue stays READY") {
    ChannelEngineLpcSctDma engine;
    engine.show();
    FL_CHECK(engine.poll() == DriverState::READY);
}

FL_TEST_CASE("LPC SCT-DMA engine: enqueue+show transitions through BUSY/DRAINING back to READY") {
    ChannelEngineLpcSctDma engine;
    auto ch = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/1, nullptr);

    engine.enqueue(ch);
    // show() kicks the transmission and the v1 scaffold returns
    // immediately. poll() either reports BUSY/DRAINING (real DMA path
    // once the TODO(3459) body lands) or READY (scaffold no-op).
    engine.show();
    const auto first_poll = engine.poll();
    FL_CHECK(first_poll == DriverState::READY ||
             first_poll == DriverState::BUSY ||
             first_poll == DriverState::DRAINING);

    // A subsequent poll MUST eventually settle at READY. In the v1
    // scaffold this happens on the first poll; the real implementation
    // will settle once the DMA channel ACTIVE flag drops.
    FL_CHECK(engine.poll() == DriverState::READY);
}

FL_TEST_CASE("LPC SCT-DMA engine: enqueue null is a no-op") {
    ChannelEngineLpcSctDma engine;
    ChannelDataPtr null_ch;
    engine.enqueue(null_ch);
    engine.show();
    FL_CHECK(engine.poll() == DriverState::READY);
}

// =============================================================================
// TX → RX byte-flow demonstration
//
// The engine's `show()` body is a documented `TODO(3459)` no-op today, so this
// test does NOT drive the wire through the engine. What it does demonstrate is
// that the byte stream the future `show()` will emit (raw GRB pixel bytes
// stored in `ChannelData`) round-trips losslessly through the LPC SCT-capture
// RX device — confirming the validation side of the planned self-loopback
// harness (#3456) is ready and just needs the engine's TX body filled in.
//
// On host, `RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>` falls through to
// `NativeRxDevice` (see `src/fl/channels/rx.cpp.hpp:266`). On real LPC845
// silicon it returns `LpcSctRxChannel` which has a fully-implemented SCT
// input-capture + DMA path in `rx_sct_capture.cpp.hpp` (both
// `FASTLED_LPC_RX_SCT` polling and `FASTLED_LPC_RX_SCT_DMA` autonomous DMA
// variants). The decode path is the same code on both.
// =============================================================================

FL_TEST_CASE("LPC SCT-DMA engine → LPC RX device byte-flow round-trip") {
    // ---- 1. Build the channel data the engine WOULD transmit. -----------
    // Three "LEDs" in GRB order: green, red, blue. Same payload as the
    // existing rx_sct_capture round-trip test for direct comparison.
    const u8 kPayload[] = {
        0x00, 0xFF, 0x00,   // green
        0xFF, 0x00, 0x00,   // red
        0x00, 0x00, 0xFF,   // blue
    };
    constexpr fl::size kPayloadLen = sizeof(kPayload);

    ChannelEngineLpcSctDma engine;
    auto ch = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/3, kPayload);

    // Engine accepts the channel and the state-machine flow completes.
    FL_REQUIRE(engine.canHandle(ch));
    engine.enqueue(ch);
    engine.show();
    FL_CHECK(engine.poll() == DriverState::READY);

    // ---- 2. Encode the same byte stream as WS2812 edge pairs. -----------
    // This is what the engine's future show() body will push out via
    // SCT+DMA. The encoding is the WS2812 protocol — independent of how
    // the bits get to the wire (bit-bang, PWM+DMA, FlexPWM, etc).
    fl::vector<EdgeTime> edges;
    edges.reserve(kPayloadLen * 16 + 2);
    for (fl::size i = 0; i < kPayloadLen; ++i) {
        appendByte(edges, kPayload[i]);
    }
    edges.push_back(EdgeTime(/*high=*/false, 60u * 1000u));  // 60 µs reset

    // ---- 3. Push the edge stream through the LPC RX device. -------------
    auto rx = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(/*pin=*/11);
    FL_REQUIRE(rx != nullptr);
    RxConfig rx_config;
    rx_config.buffer_size = 256;
    rx_config.start_low   = true;
    FL_REQUIRE(rx->begin(rx_config));
    FL_REQUIRE(rx->injectEdges(toSpan(edges)));
    FL_CHECK(rx->wait(10) == RxWaitResult::SUCCESS);

    // ---- 4. Decode and verify byte-for-byte. ----------------------------
    u8 decoded[kPayloadLen + 1] = {0};
    auto result = rx->decode(ws2812_decoder_timing(),
                             fl::span<u8>(decoded, sizeof(decoded)));
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == kPayloadLen);
    for (fl::size i = 0; i < kPayloadLen; ++i) {
        FL_CHECK(decoded[i] == kPayload[i]);
    }
}

}  // FL_TEST_FILE
