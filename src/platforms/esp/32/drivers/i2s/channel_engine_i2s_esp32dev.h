/// @file channel_engine_i2s_esp32dev.h
/// @brief `IChannelDriver` for classic-ESP32 I2S parallel-out LEDs
///        (#3471).
///
/// **Attribution: Yves Bazin** authored the original I2S1 parallel-out
/// clockless driver for classic ESP32 (`ClocklessI2S<>` +
/// `i2s_esp32dev`) — first shipped by FastLED circa 2018-2019 and the
/// proof-of-concept that classic ESP32 I2S1 in LCD/parallel mode could
/// drive multiple WS2812-family strips in lock-step. This engine
/// replaces that architecture with a general wave8/wave3 clockless
/// driver following the PARLIO pattern (fixed 8 MHz pixel clock,
/// per-channel `ChipsetTimingConfig` translated to a wave8 byte-LUT),
/// but every low-level register write and DMA-descriptor field in
/// the peripheral impl (`i2s_peripheral_esp32dev_esp.cpp.hpp`) traces
/// its lineage to Yves's original driver. The Yves files were deleted
/// at FastLED#3526 Phase 2e once the modern path reached parity.
///
/// Follows the same shape as the S3 sibling `ChannelEngineI2S`
/// (`channel_driver_i2s.h`):
///
///   - Constructor injects an `II2sPeripheralEsp32Dev` — real hardware
///     in production, mock in tests.
///   - Reports `Capabilities(true, true)` per the parallel-IO code-
///     review rule (see `agents/docs/cpp-standards.md`) — one engine
///     handles both clockless and SPI on the classic-ESP32 I2S
///     peripheral, dispatched per-channel at `show()` time based on
///     `data->isSpi()`.
///   - State machine: READY → enqueue → READY → show → BUSY →
///     poll* → READY (peripheral completion callback flips the flag).
///
/// ## Layering
///
/// `show()` linearly copies every enqueued channel's encoded pixel
/// bytes into a single DMA-capable scratch buffer, then calls
/// `peripheral->transmit()` once. The scratch buffer stays valid
/// until the peripheral's completion callback fires.
///
/// Wave8 pulse-major encoding lives inside the peripheral's
/// `transmit()` implementation, NOT in this engine's
/// `packScratchBuffer()`. Rationale: the encoded byte stream is a
/// hardware-specific DMA layout (16 lanes × 8 pulses per byte at the
/// wave8 pixel clock), so the encoding step belongs next to the DMA
/// register writes. The engine hands over raw channel bytes; the
/// peripheral chooses whether to run the encoder (`_esp` impl) or
/// capture the raw bytes for test observation (`_mock` impl).

#pragma once

// IWYU pragma: private

// No platform guard: the class body needs to be reachable from
// host-test TUs. The `BusTraits<Bus::FLEX_IO, 0>` gate that pulls
// this into production builds lives in `bus_traits.h` and gates on
// the classic-ESP32 I2S feature flag.

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/wave8.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/i2s/ii2s_peripheral_esp32dev.h"

namespace fl {

/// @brief `IChannelDriver` for the classic-ESP32 I2S peripheral.
class ChannelEngineI2sEsp32Dev : public IChannelDriver {
  public:
    /// @brief Construct with a peripheral (real or mock).
    /// @param i2s_port Which I2S block this engine drives (FastLED#3576
    ///        Phase 1): 1 = primary bank (`Bus::FLEX_IO` instance 0),
    ///        0 = second bank (`Bus::FLEX_IO` instance 1, contended
    ///        with the clocked-SPI driver via the port-claim registry).
    explicit ChannelEngineI2sEsp32Dev(
        fl::shared_ptr<II2sPeripheralEsp32Dev> peripheral,
        u8 i2s_port = 1) FL_NO_EXCEPT;

    ~ChannelEngineI2sEsp32Dev() override;

    //=========================================================================
    // IChannelDriver
    //=========================================================================

    bool canHandle(const ChannelDataPtr &data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        // Distinct names per bank so affinity binding and the
        // AutoResearch exclusive-driver selector can target each block.
        return (mI2sPort == 0) ? fl::string::from_literal("I2S0")
                               : fl::string::from_literal("I2S");
    }

    /// @brief Reports both clockless and SPI capability per the
    ///        parallel-IO rule. The engine dispatches per-channel at
    ///        `show()`.
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(/*clockless=*/true, /*spi=*/true);
    }

    //=========================================================================
    // Introspection (test-only public accessors)
    //=========================================================================

    /// @brief The peripheral this engine was constructed with. Tests
    ///        use this to reach into the mock without going through
    ///        the singleton.
    fl::shared_ptr<II2sPeripheralEsp32Dev> peripheral() const FL_NO_EXCEPT {
        return mPeripheral;
    }

    /// @brief Size in bytes of the last DMA-capable scratch buffer
    ///        allocated by `show()`. Zero if `show()` has never been
    ///        called or the previous transmission has fully cleaned
    ///        up. Test-only visibility for lifecycle assertions.
    size_t currentScratchBufferSize() const FL_NO_EXCEPT {
        return mScratchSize;
    }

    /// @brief Snapshot of the driver's own state machine (not the
    ///        peripheral's `isBusy()`). Test hook.
    DriverState currentState() const FL_NO_EXCEPT { return mState; }

  private:
    /// @brief Ensure the scratch buffer is at least `required` bytes.
    ///        Reallocates via the peripheral if the current buffer
    ///        is too small. Returns false on allocation failure.
    bool ensureScratchBuffer(size_t required) FL_NO_EXCEPT;

    /// @brief Copy encoded pixel bytes from every enqueued channel
    ///        into the scratch buffer as a linear concatenation.
    ///        Wave8 pulse-major encoding is the peripheral's
    ///        responsibility (see the class docstring's Layering
    ///        section). Returns the total byte count written.
    size_t packScratchBuffer() FL_NO_EXCEPT;

    /// @brief Free the current scratch buffer (if any). Called when
    ///        the engine is destructed or a new allocation is needed
    ///        that's larger than the current one.
    void freeScratchBuffer() FL_NO_EXCEPT;

    /// @brief Static trampoline registered with the peripheral's
    ///        completion callback slot. Casts `user_ctx` back to
    ///        `this` and calls `onTransmitDone()`.
    static void onTransmitDoneTrampoline(void *user_ctx) FL_NO_EXCEPT;

    /// @brief Fire when the peripheral's DMA transfer completes.
    ///        Runs in ISR context on real hardware; on the mock it
    ///        runs on the test thread.
    void onTransmitDone() FL_NO_EXCEPT;

    fl::shared_ptr<II2sPeripheralEsp32Dev> mPeripheral;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;

    // DMA-capable scratch buffer for the current frame.
    u8 *mScratchBuffer;
    size_t mScratchSize;

    DriverState mState;

    // Set true when the peripheral's completion callback has fired
    // since the most recent `show()`. Consumed by `poll()`.
    bool mTransmitCompleted;

    bool mPeripheralInitialized;

    // FastLED#3526 Phase 2c — SPI-mode delegate. When `show()` sees a
    // batch of SPI channels it forwards to the existing tested
    // `ChannelDriverI2sSpi` (which owns its own peripheral instance
    // via the `I2sSpiPeripheralEsp` singleton). Both drivers wrap the
    // same I2S1 hardware; only one is active at a time. Lazy-init on
    // first SPI batch so builds that never use SPI don't link the
    // delegate.
    fl::shared_ptr<IChannelDriver> mSpiDelegate;

    // FastLED#3569 stack-overflow fix — wave8 byte LUT is 2 KB
    // (`Wave8Byte lut[256]` × 8 bytes), too big for the `show()` task
    // stack. Cached here as an instance member (filled in place via the
    // out-param `buildWave8ByteExpansionLUT` overload) and rebuilt
    // whenever the cached T1/T2/T3 timing changes (typically once,
    // since default is fixed WS2812B 800 kHz until per-channel timing
    // dispatch lands).
    Wave8ByteExpansionLut mWave8ByteLut;
    u32 mCachedT1;
    u32 mCachedT2;
    u32 mCachedT3;
    bool mWave8LutValid;

    // FastLED#3576 Phase 1 — which I2S block this engine drives (0/1).
    u8 mI2sPort;
};

/// @brief Factory that builds a `ChannelEngineI2sEsp32Dev` wrapping a
///        real hardware peripheral, returned as a `shared_ptr<IChannelDriver>`
///        suitable for registration with the channel manager.
///
/// FastLED#3526 Phase 2d — used by the `BusTraits<Bus::FLEX_IO, 1>`
/// specialization on classic ESP32 to expose the modern engine at
/// slot 1 (slot 0 stays with the I2S-SPI driver during rollout).
///
/// On non-classic-ESP32 targets or builds without `FASTLED_ESP32_HAS_I2S`,
/// returns nullptr so the channel-manager registration path skips
/// cleanly.
fl::shared_ptr<IChannelDriver> createI2sEsp32DevEngine() FL_NO_EXCEPT;

/// @brief Port-selecting factory (FastLED#3576 Phase 1). `port == 1` is
///        the primary bank (identical to the no-arg overload); `port ==
///        0` builds the second bank ("I2S0", `Bus::FLEX_IO` instance 1)
///        wrapping the per-port peripheral singleton. I2S0 is contended
///        with the clocked-SPI driver — the port-claim registry
///        arbitrates at initialize() time.
fl::shared_ptr<IChannelDriver> createI2sEsp32DevEngine(u8 port) FL_NO_EXCEPT;

} // namespace fl
