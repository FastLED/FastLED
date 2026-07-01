/// @file ii2s_peripheral_esp32dev.h
/// @brief Virtual interface for the classic-ESP32 I2S peripheral used
///        as an LED parallel-out engine (#3471).
///
/// Companion to `ii2s_lcd_cam_peripheral.h` (S3 LCD_CAM sibling). Same
/// shape, same idioms — lifecycle + DMA buffer alloc + async transmit
/// + ISR callback registration. Concrete implementations:
///
/// - `I2sPeripheralEsp32DevEsp` — real hardware (`driver/i2s.h` on
///   IDF 4.x + direct `soc/i2s_struct.h` register touches where the
///   IDF surface isn't enough).
/// - `I2sPeripheralEsp32DevMock` — host-side simulation for unit
///   tests. Single-threaded by design (no `std::thread`, no async
///   callbacks — tests advance simulation manually via
///   `simulateTransmitDone()`), matching the #3462 / #3463 / #3469
///   precedent.
///
/// ## Async DMA streaming surface
///
/// The interface is deliberately built around an async submit +
/// completion-callback model even though the initial real-hw impl
/// may lean on a blocking `waitTransmitDone()` for simplicity. That
/// keeps callers stable when the follow-up switches to true DMA
/// streaming with ISR-driven completion:
///
///   1. `initialize(config)` — one-shot at engine construction.
///   2. `allocateBuffer(size_bytes)` — DMA-capable memory chunk. The
///      buffer stays valid for the lifetime of the peripheral or
///      until `freeBuffer()` is called.
///   3. `registerTransmitCallback(cb, user_ctx)` — one global
///      completion hook that fires from the DMA-done ISR. `cb` runs
///      in ISR context and must be short + ISR-safe (no logging, no
///      heap).
///   4. `transmit(buffer, size_bytes)` — kicks off the DMA transfer
///      asynchronously and returns immediately. The buffer must
///      remain valid until the callback fires.
///   5. `waitTransmitDone(timeout_ms)` — optional convenience for
///      callers that want a blocking wait rather than a callback.
///      Returns quickly (does not delay) if the peripheral is idle.
///   6. `isBusy()` — non-blocking status poll for the engine's
///      `poll()` method.
///
/// ## What's OUT of the interface
///
/// - Direct `I2S0.*` / `I2S1.*` register access from the ISR hot
///   path — those stay inline in the concrete impl to preserve
///   IRAM placement (see the RMT4 wrapper #3461 rationale).
/// - DMA descriptor mutation from the completion ISR — stays in the
///   concrete impl.

#pragma once

// IWYU pragma: private

// This interface is platform-agnostic (no ESP32 guard) so it can be
// included from host-side mock builds. Mirrors `II2sLcdCamPeripheral`
// and `IRMT4Peripheral`.

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief Classic-ESP32 I2S peripheral configuration.
///
/// Captures the shape of the parallel-out I2S setup FastLED needs to
/// drive multiple LED strips through a single DMA stream. Individual
/// bit-lane -> GPIO assignments live in a follow-on array so a caller
/// can grow the strip count without changing the config struct.
struct I2sEsp32DevPeripheralConfig {
    /// @brief I2S port number (0 or 1). Classic ESP32 has both; S2
    ///        has only I2S0 and different behaviour. Default = 1 so
    ///        the peripheral doesn't fight with the DAC output on
    ///        boards that use I2S0 for audio.
    u8 mI2sPort;

    /// @brief Pixel clock in Hz. For WS2812B family, ~2.4 MHz gives
    ///        the right per-bit slot width when combined with the
    ///        3-slot Wave8 encoding (T0H / T0L / T1H / T1L math).
    u32 mPixelClockHz;

    /// @brief Parallel bit width, i.e. number of concurrent LED
    ///        strips this peripheral will drive. Classic ESP32 I2S
    ///        supports up to 16 in parallel-out mode. Range: 1..16.
    u8 mBitWidth;

    /// @brief Optional: invert output polarity per strip. Bit `i`
    ///        set = strip `i` inverted. Only used when the encoder
    ///        needs polarity inversion for a given chipset — most
    ///        WS2812-family strips use default (non-inverted).
    u16 mInvertMask;

    I2sEsp32DevPeripheralConfig() FL_NO_EXCEPT : mI2sPort(1),
                                                 mPixelClockHz(0),
                                                 mBitWidth(0),
                                                 mInvertMask(0) {}

    I2sEsp32DevPeripheralConfig(u8 port, u32 clk, u8 width) FL_NO_EXCEPT
        : mI2sPort(port),
          mPixelClockHz(clk),
          mBitWidth(width),
          mInvertMask(0) {}
};

//=============================================================================
// Callback types
//=============================================================================

/// @brief Transmit-done ISR callback signature.
///
/// Runs in ISR context on real hardware. `user_ctx` is the pointer
/// passed to `registerTransmitCallback()`. Callback must not log,
/// allocate, or block.
using I2sEsp32DevTxDoneCallback = void (*)(void *user_ctx);

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for the classic-ESP32 I2S peripheral in
///        parallel-out mode. Pure virtual — see the file docblock for
///        the design rationale and the concrete impls.
class II2sPeripheralEsp32Dev {
  public:
    virtual ~II2sPeripheralEsp32Dev() = default;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    /// @brief Configure and enable the peripheral. Must succeed
    ///        before any other method can be used.
    virtual bool
    initialize(const I2sEsp32DevPeripheralConfig &config) FL_NO_EXCEPT = 0;

    /// @brief Disable the peripheral and release ESP-IDF resources.
    virtual void deinitialize() FL_NO_EXCEPT = 0;

    /// @brief True after `initialize()` succeeded and before
    ///        `deinitialize()` is called.
    virtual bool isInitialized() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // DMA-capable buffer management
    //=========================================================================

    /// @brief Allocate a DMA-capable buffer aligned for the I2S FIFO.
    ///        Returns nullptr on failure. Ownership stays with the
    ///        peripheral until `freeBuffer()` is called.
    virtual u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT = 0;

    /// @brief Free a buffer allocated via `allocateBuffer()`. nullptr
    ///        is a silent no-op.
    virtual void freeBuffer(u8 *buffer) FL_NO_EXCEPT = 0;

    //=========================================================================
    // Async transmission
    //=========================================================================

    /// @brief Kick off an async DMA transfer of `size_bytes` from
    ///        `buffer`. Returns immediately. The buffer must remain
    ///        valid until the completion callback fires or
    ///        `waitTransmitDone()` returns.
    ///
    /// @return true on success, false if the peripheral is not
    ///         initialised, is already busy, or the DMA setup failed.
    virtual bool transmit(const u8 *buffer, size_t size_bytes) FL_NO_EXCEPT = 0;

    /// @brief Block until the most recent `transmit()` completes.
    /// @param timeout_ms  Max wait; 0 = non-blocking poll.
    /// @return true on completion, false on timeout or error.
    virtual bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT = 0;

    /// @brief True while a DMA transfer is in flight. Non-blocking.
    virtual bool isBusy() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // ISR callback registration
    //=========================================================================

    /// @brief Register the DMA-done ISR callback. Overwrites any
    ///        prior registration. `cb=nullptr` clears the hook.
    /// @return true on success, false if the peripheral doesn't
    ///         support callbacks in the current state.
    virtual bool registerTransmitCallback(I2sEsp32DevTxDoneCallback cb,
                                          void *user_ctx) FL_NO_EXCEPT = 0;

    //=========================================================================
    // Introspection
    //=========================================================================

    /// @brief The config passed to the most recent `initialize()`.
    virtual const I2sEsp32DevPeripheralConfig &
    getConfig() const FL_NO_EXCEPT = 0;
};

} // namespace fl
