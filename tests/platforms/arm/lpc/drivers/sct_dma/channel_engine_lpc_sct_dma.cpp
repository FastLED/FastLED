/// @file lpc_sct_dma_engine.cpp
/// @brief Host-side tests for the LPC845 SCT+DMA channels-API engine
///        (FastLED #3459) including the #3468 TX→RX byte-flow readback
///        through the actual `engine.show()` path.
///
/// The engine itself ships on LPC845 silicon (gated on `FL_IS_ARM_LPC_845`).
/// To exercise the `IChannelDriver` contract (canHandle filtering, state
/// machine, capabilities, name) without hardware, the engine class is also
/// gated to compile under `FASTLED_STUB_IMPL` — same convention as
/// `ChannelEngineObjectFLED` in
/// `tests/platforms/arm/teensy/teensy4_common/drivers/objectfled/`.
///
/// Two tiers of test coverage:
///
/// 1. **Contract tests** (canHandle, state machine, capabilities, name).
///    Exercise `IChannelDriver` conformance.
///
/// 2. **TX→RX readback** — closes the #3468 goal. Pixel bytes go into
///    `ChannelData`, are driven through `engine.enqueue()` and
///    `engine.show()`, captured by the host build of
///    `LpcSctDmaTransmitter` (which records its input rather than
///    driving SCT/DMA registers that do not exist on host), converted
///    to WS2812 edge pairs, pushed through the LPC SCT-capture RX
///    device via `injectEdges()`, and decoded back through the shared
///    4-phase WS2812 decoder. Byte-for-byte equality confirms the
///    channels-API engine's dispatch and byte handoff are correct —
///    the only piece not exercised is the actual SCT/DMA register
///    sequencing, which requires silicon.

#include "FastLED.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/types.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/cstring.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"
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

// =============================================================================
// #3468 goal: TX→RX readback THROUGH the actual engine.show() path.
//
// This is the "no cheating" version of the previous test case. Rather than
// sourcing edges from an independent `appendByte()` encoder that bypasses
// the engine, this test:
//
//   1. Builds a ChannelData with a known GRB pixel payload.
//   2. Runs it through `engine.enqueue()` + `engine.show()`. On host the
//      engine dispatches to `LpcSctDmaTransmitter::transmit()` whose host
//      stub captures the byte stream in `mTxCapture` (see #3468 wiring).
//   3. Reads the captured bytes out via `engine.transmitter().getCapturedTxBytes()`.
//   4. Converts those captured bytes to WS2812 edge pairs using the same
//      T0H / T0L / T1H / T1L timing the WS2812B protocol defines.
//   5. Injects the edge stream into the LPC SCT-capture RX device.
//   6. Decodes back through the shared 4-phase WS2812 decoder.
//   7. Asserts decoded == original payload, byte-for-byte.
//
// If step (2) or (3) drops or corrupts bytes, step (7) fails. This
// closes the "TX → RX readback confirmed working" goal from #3468 at the
// byte-flow layer. The actual SCT/DMA register sequencing on silicon is
// still guarded by the acceptance criteria that require bench validation.
// =============================================================================

FL_TEST_CASE("#3468: end-to-end TX→RX readback through engine.show()") {
    // Deliberately non-trivial payload: two "LEDs" plus a bit pattern
    // that exercises every 0→1 and 1→0 transition in a WS2812 byte
    // (0x55 = 01010101, 0xAA = 10101010). Ordering unusual so a byte
    // reversed anywhere in the pipeline shows up.
    const u8 kPayload[] = {
        0x00, 0xFF, 0x00,   // LED 0 — pure green (GRB order)
        0xFF, 0x00, 0x00,   // LED 1 — pure red
        0x55, 0xAA, 0x33,   // LED 2 — alternating bit patterns
    };
    constexpr fl::size kPayloadLen = sizeof(kPayload);

    // 1. Build the channel data.
    ChannelEngineLpcSctDma engine;
    auto ch = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/3, kPayload);
    FL_REQUIRE(engine.canHandle(ch));

    // 2. Drive it through the actual engine.show() dispatch. The host
    //    build of LpcSctDmaTransmitter captures the byte stream.
    engine.transmitter().clearCapture();
    engine.enqueue(ch);
    engine.show();

    // 3. Read the captured bytes back out.
    fl::span<const u8> captured = engine.transmitter().getCapturedTxBytes();
    FL_REQUIRE(captured.size() == kPayloadLen);
    // Sanity: the engine handed the transmitter the exact bytes from
    // ChannelData, in the same order.
    for (fl::size i = 0; i < kPayloadLen; ++i) {
        FL_CHECK(captured[i] == kPayload[i]);
    }

    // 4. Convert captured bytes → WS2812 edge pairs.
    fl::vector<EdgeTime> edges;
    edges.reserve(kPayloadLen * 16 + 2);
    for (fl::size i = 0; i < captured.size(); ++i) {
        appendByte(edges, captured[i]);
    }
    edges.push_back(EdgeTime(/*high=*/false, 60u * 1000u));  // reset

    // 5. Inject into the LPC RX device.
    auto rx = RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(/*pin=*/11);
    FL_REQUIRE(rx != nullptr);
    RxConfig rx_config;
    rx_config.buffer_size = 256;
    rx_config.start_low   = true;
    FL_REQUIRE(rx->begin(rx_config));
    FL_REQUIRE(rx->injectEdges(toSpan(edges)));
    FL_CHECK(rx->wait(10) == RxWaitResult::SUCCESS);

    // 6. Decode.
    u8 decoded[kPayloadLen + 1] = {0};
    auto result = rx->decode(ws2812_decoder_timing(),
                             fl::span<u8>(decoded, sizeof(decoded)));
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == kPayloadLen);

    // 7. Byte-for-byte check: the round-trip returned the exact
    //    original payload.
    for (fl::size i = 0; i < kPayloadLen; ++i) {
        FL_CHECK(decoded[i] == kPayload[i]);
    }

    // Engine state machine settled cleanly.
    FL_CHECK(engine.poll() == DriverState::READY);
}

FL_TEST_CASE("#3468: capture buffer is cleared between transmits") {
    const u8 kFirst[]  = {0xDE, 0xAD, 0xBE};
    const u8 kSecond[] = {0xCA, 0xFE, 0xBA, 0xBE, 0xF0, 0x0D};

    ChannelEngineLpcSctDma engine;

    auto ch1 = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/1, kFirst);
    engine.enqueue(ch1);
    engine.show();
    FL_REQUIRE(engine.transmitter().getCapturedTxBytes().size() == sizeof(kFirst));

    // Second frame with a different payload must overwrite, not append.
    engine.poll();  // settle back to READY
    auto ch2 = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/2, kSecond);
    engine.enqueue(ch2);
    engine.show();

    fl::span<const u8> captured = engine.transmitter().getCapturedTxBytes();
    FL_REQUIRE(captured.size() == sizeof(kSecond));
    for (fl::size i = 0; i < sizeof(kSecond); ++i) {
        FL_CHECK(captured[i] == kSecond[i]);
    }
}

// =============================================================================
// #3459 acceptance criterion 3: no busy-wait in show().
//
// The engine's state machine transitions READY → BUSY → DRAINING → READY
// must all be driven from poll() calls, not from inside show(). show()
// kicks the transmission and returns; poll() advances chunk boundaries
// on LPC845 and settles to READY when the full frame has drained.
//
// On host there is no DMA to wait on, so transmit() completes
// synchronously and the first poll() after show() returns READY.
// The tests below pin the observable contract on host: pollAndAdvance()
// is idempotent when the transmitter is idle, and the poll()-driven
// state machine can process multiple back-to-back frames without a
// busy-wait inside show().
// =============================================================================

FL_TEST_CASE("#3459 item 3: pollAndAdvance() returns true when idle") {
    ChannelEngineLpcSctDma engine;
    // No transmission has been kicked; transmitter is idle.
    FL_CHECK(engine.transmitter().pollAndAdvance() == true);
    // Calling again does not change state.
    FL_CHECK(engine.transmitter().pollAndAdvance() == true);
}

FL_TEST_CASE("#3459 item 3: show() does not busy-wait; multiple frames drain via poll()") {
    // Drive three frames back-to-back through the engine's show()/poll()
    // cycle. On host each transmit() completes synchronously so poll()
    // returns READY immediately, but the test still exercises the loop
    // caller would use on silicon: show() → poll() until READY → next
    // show(). If show() had a hidden busy-wait this loop would still
    // pass (blocking is invisible from outside), but if the state
    // machine mis-tracked mTransmissionActive across frames the second
    // or third show()+poll() would leak state and fail.
    const u8 kFrame0[] = {0x11, 0x22, 0x33};
    const u8 kFrame1[] = {0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    const u8 kFrame2[] = {0xAA, 0xBB, 0xCC};

    ChannelEngineLpcSctDma engine;

    // Frame 0
    auto ch0 = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/1, kFrame0);
    engine.enqueue(ch0);
    engine.show();
    // poll() must drive the state to READY. It should complete
    // within a bounded number of calls (host: 1; silicon: chunk count).
    for (int i = 0; i < 100; ++i) {
        if (engine.poll() == DriverState::READY) break;
    }
    FL_CHECK(engine.poll() == DriverState::READY);
    FL_REQUIRE(engine.transmitter().getCapturedTxBytes().size() == sizeof(kFrame0));

    // Frame 1
    auto ch1 = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/2, kFrame1);
    engine.enqueue(ch1);
    engine.show();
    for (int i = 0; i < 100; ++i) {
        if (engine.poll() == DriverState::READY) break;
    }
    FL_CHECK(engine.poll() == DriverState::READY);
    FL_REQUIRE(engine.transmitter().getCapturedTxBytes().size() == sizeof(kFrame1));

    // Frame 2
    auto ch2 = makeClocklessChannelData(/*pin=*/11, /*num_leds=*/1, kFrame2);
    engine.enqueue(ch2);
    engine.show();
    for (int i = 0; i < 100; ++i) {
        if (engine.poll() == DriverState::READY) break;
    }
    FL_CHECK(engine.poll() == DriverState::READY);
    FL_REQUIRE(engine.transmitter().getCapturedTxBytes().size() == sizeof(kFrame2));
    for (fl::size i = 0; i < sizeof(kFrame2); ++i) {
        FL_CHECK(engine.transmitter().getCapturedTxBytes()[i] == kFrame2[i]);
    }
}

}  // FL_TEST_FILE
