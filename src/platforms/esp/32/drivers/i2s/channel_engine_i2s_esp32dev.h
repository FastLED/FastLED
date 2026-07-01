/// @file channel_engine_i2s_esp32dev.h
/// @brief `IChannelDriver` for classic-ESP32 I2S parallel-out LEDs
///        (#3471).
///
/// Ground-up rewrite of the classic-ESP32 I2S LED path. Deprecates and
/// eventually removes the third-party Yves Bazin driver
/// (`i2s_esp32dev.h` / `clockless_i2s_esp32.h`). Follows the same
/// shape as the S3 sibling `ChannelEngineI2S`
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
/// ## Staging
///
/// Stage 1 (this file's initial impl): pre-computation strategy.
/// `show()` linearly copies every enqueued channel's encoded pixel
/// bytes into a single DMA-capable scratch buffer, then calls
/// `peripheral->transmit()` once. The buffer stays valid until the
/// completion callback fires. This keeps the engine's state machine
/// simple, exercises the full peripheral surface (allocateBuffer /
/// transmit / registerTransmitCallback / freeBuffer), and is fully
/// testable against the mock. It does NOT yet run the parallel
/// bit-transpose or the wave8 slot encoding.
///
/// Stage 2 (follow-up PR against the same file): the encoder step
/// runs the actual per-lane bit-transpose + wave8 slot expansion so
/// the emitted waveform drives real WS2812 timing.
///
/// The peripheral interface + engine state machine are stable across
/// stages — Stage 2 replaces the body of `packScratchBuffer()`
/// without touching the surface.

#pragma once

// IWYU pragma: private

// No platform guard: the class body needs to be reachable from
// host-test TUs. The `BusTraits<Bus::FLEX_IO, 0>` gate that pulls
// this into production builds lives in `bus_traits.h` and gates on
// the classic-ESP32 I2S feature flag.

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
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
    explicit ChannelEngineI2sEsp32Dev(
        fl::shared_ptr<II2sPeripheralEsp32Dev> peripheral) FL_NO_EXCEPT;

    ~ChannelEngineI2sEsp32Dev() override;

    //=========================================================================
    // IChannelDriver
    //=========================================================================

    bool canHandle(const ChannelDataPtr &data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("I2S");
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
    ///        into the scratch buffer. Stage 1: linear concatenation.
    ///        Stage 2: parallel bit-transpose + wave8 slot expansion.
    ///        Returns the total byte count written.
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
};

} // namespace fl
