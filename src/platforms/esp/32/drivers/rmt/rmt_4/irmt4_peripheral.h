/// @file irmt4_peripheral.h
/// @brief Platform-independent lifecycle interface for the IDF 4 RMT driver.
///
/// ChannelEngineRMT4 uses the SDK-managed shared RX/TX ISR. Its byte
/// translators are out-of-line IRAM functions without virtual dispatch.
/// This interface retains low-level interrupt hooks for existing peripheral
/// implementations; the production TX engine does not install a custom ISR.

#pragma once

// IWYU pragma: private

// This interface is platform-agnostic (no ESP32 guard) so it can be
// included from host-side mock builds. Mirrors `IRMT5Peripheral`
// — the concrete real-hardware impl (`Rmt4PeripheralESP`) keeps the
// platform guard; this header does not.

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

namespace detail {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief RMT4 TX channel configuration (one strip / pin).
///
/// Captures everything `rmt_config()` needs plus the FastLED-specific
/// threshold count. Channel number is part of the config because the
/// engine reserves a contiguous physical memory span before SDK setup.
struct Rmt4ChannelConfig {
    int mChannel;      ///< RMT hardware channel (0..FASTLED_RMT_MAX_CHANNELS-1)
    int mGpioPin;      ///< GPIO pin for this channel
    u32 mClockDivider; ///< RMT clock divider (typically DIVIDER_RMT4 = 2)
    u8 mMemBlocks;     ///< RMT memory blocks per channel (typically
                       ///< FASTLED_RMT_MEM_BLOCKS)
    u16 mTxThreshold;  ///< Half-buffer threshold for refill interrupt
                       ///< (PULSES_PER_FILL_RMT4)

    Rmt4ChannelConfig() FL_NO_EXCEPT : mChannel(-1),
                                       mGpioPin(-1),
                                       mClockDivider(0),
                                       mMemBlocks(0),
                                       mTxThreshold(0) {}

    Rmt4ChannelConfig(int channel, int gpio_pin, u32 clk_div, u8 mem_blocks,
                      u16 threshold) FL_NO_EXCEPT : mChannel(channel),
                                                    mGpioPin(gpio_pin),
                                                    mClockDivider(clk_div),
                                                    mMemBlocks(mem_blocks),
                                                    mTxThreshold(threshold) {}
};

//=============================================================================
// ISR Trampoline
//=============================================================================

/// @brief Function pointer signature for the global RMT ISR.
///
/// Matches `intr_handler_t` from `esp_intr_alloc.h` without leaking the
/// ESP-IDF type through this interface. The concrete impl casts to the
/// real signature.
using Rmt4IsrHandler = void (*)(void *arg);

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for IDF-4.x RMT peripheral lifecycle calls.
///
/// Pure virtual. Concrete implementations:
/// - `Rmt4PeripheralESP` — thin wrapper around `driver/rmt.h` (real hw).
/// - (future) `Rmt4PeripheralMock` — host-side simulation for unit tests.
///
/// All methods return `bool` (true = success) so callers don't have to
/// reason about ESP-IDF `esp_err_t` codes at the engine layer.
class IRMT4Peripheral {
  public:
    virtual ~IRMT4Peripheral() = default;

    //=========================================================================
    // Channel Lifecycle
    //=========================================================================

    /// @brief Configure an RMT TX channel from a `Rmt4ChannelConfig`.
    ///
    /// Maps to `rmt_config()` with the standard FastLED clockless preset:
    /// idle-low output enabled, no carrier, no loop mode.
    virtual bool
    configureChannel(const Rmt4ChannelConfig &cfg) FL_NO_EXCEPT = 0;

    /// @brief Install the SDK-managed TX driver with no RX ring buffer.
    /// Requests IRAM and LEVEL3 interrupt allocation flags.
    virtual bool installDriver(int channel) FL_NO_EXCEPT = 0;

    /// @brief Uninstall the ESP-IDF RMT driver for one channel.
    /// Maps to `rmt_driver_uninstall(channel)`.
    virtual bool uninstallDriver(int channel) FL_NO_EXCEPT = 0;

    //=========================================================================
    // Interrupt Control
    //=========================================================================

    /// @brief Arm the half-buffer threshold interrupt.
    /// Maps to `rmt_set_tx_thr_intr_en(channel, enable, threshold)`.
    virtual bool setTxThresholdIntrEnable(int channel, bool enable,
                                          u16 threshold) FL_NO_EXCEPT = 0;

    /// @brief Toggle the per-channel TX interrupt enable bit.
    /// Maps to `rmt_set_tx_intr_en(channel, enable)`.
    virtual bool setTxIntrEnable(int channel, bool enable) FL_NO_EXCEPT = 0;

    //=========================================================================
    // GPIO Assignment
    //=========================================================================

    /// @brief Assign a GPIO to a channel.
    ///
    /// On IDF >= 4.4 maps to `rmt_set_gpio(channel, RMT_MODE_TX, pin,
    /// invert)`; on older IDF to `rmt_set_pin(channel, RMT_MODE_TX,
    /// pin)` and `invert` is ignored.
    virtual bool setGpio(int channel, int gpio_pin,
                         bool invert) FL_NO_EXCEPT = 0;

    //=========================================================================
    // Global ISR
    //=========================================================================

    /// @brief Install the global RMT ISR (one for all channels).
    /// Maps to `esp_intr_alloc(ETS_RMT_INTR_SOURCE, IRAM | LEVEL3,
    /// handler, arg, out_handle)`.
    ///
    /// `out_handle` receives the `intr_handle_t` cast to `void*` so the
    /// engine can later pass it back to `freeIsr()` without leaking the
    /// ESP-IDF type through this header.
    virtual bool installIsr(Rmt4IsrHandler handler, void *arg,
                            void **out_handle) FL_NO_EXCEPT = 0;

    /// @brief Free a previously-installed global RMT ISR.
    /// Maps to `esp_intr_free(handle)`. Safe to call with nullptr.
    virtual void freeIsr(void *handle) FL_NO_EXCEPT = 0;
};

} // namespace detail
} // namespace fl
