/// @file parlio_channel.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED channel interface
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Key features:
/// - Simultaneous output to multiple LED strips
/// - DMA-based transmission (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - Runtime-configured for different channel counts and chipset timings

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "fl/stdint.h"
#include "pixeltypes.h"  // For CRGB definition

namespace fl {

// Forward declarations
struct ChipsetTimingConfig;
template<typename T, typename Deleter> class unique_ptr;

/// @brief Configuration structure for PARLIO LED channel
struct ParlioChannelConfig {
    int clk_gpio;                ///< [UNUSED] GPIO number for clock output - internal clock is used instead
    int data_gpios[16];          ///< GPIO numbers for data lanes (up to 16)
    int num_lanes;               ///< Active lane count (1, 2, 4, 8, or 16)
    uint32_t clock_freq_hz;      ///< PARLIO clock frequency (0 = use default 3.2 MHz for WS2812)
    bool is_rgbw;                ///< True for RGBW (4-component) LEDs like SK6812 (default: false)
    bool auto_clock_adjustment;  ///< Enable dynamic clock freq based on LED count (default: false)
};

/// @brief Abstract interface for PARLIO LED channel
///
/// This interface provides platform-independent access to the ESP32-P4's
/// Parallel IO TX peripheral for driving multiple LED strips in parallel.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IParlioChannel {
public:
    /// @brief Default clock frequency for WS2812 timing
    /// 3.2 MHz = 800kHz WS2812 data rate × 4 clocks per LED bit
    static constexpr uint32_t DEFAULT_CLOCK_FREQ_HZ = 3200000;  // 3.2 MHz

    /// @brief Factory function to create a PARLIO driver instance
    /// @param timing Chipset timing configuration (T1, T2, T3, etc.)
    /// @param data_width Number of parallel data lanes (1, 2, 4, 8, or 16)
    /// @return Unique pointer to driver instance
    static unique_ptr<IParlioChannel> create(const ChipsetTimingConfig& timing, uint8_t data_width);

    /// @brief Virtual destructor
    virtual ~IParlioChannel() = default;

    /// @brief Initialize driver with GPIO pins and LED count
    ///
    /// @param config Driver configuration (pins, lane count, clock frequency)
    /// @param num_leds Number of LEDs per strip
    /// @return true if initialization succeeded
    virtual bool begin(const ParlioChannelConfig& config, uint16_t num_leds) = 0;

    /// @brief Shutdown driver and free resources
    virtual void end() = 0;

    /// @brief Set LED strip data pointer for a specific channel
    ///
    /// @param channel Channel index (0 to DATA_WIDTH-1)
    /// @param leds Pointer to LED data array
    virtual void set_strip(uint8_t channel, CRGB* leds) = 0;

    /// @brief Show LED data - transmit to all strips
    /// Assumes data in CRGB buffer is already in correct RGB order
    virtual void show() = 0;

    /// @brief Wait for current transmission to complete
    virtual void wait() = 0;

    /// @brief Check if driver is initialized
    virtual bool is_initialized() const = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IParlioChannel() = default;

    // Non-copyable, non-movable
    IParlioChannel(const IParlioChannel&) = delete;
    IParlioChannel& operator=(const IParlioChannel&) = delete;
    IParlioChannel(IParlioChannel&&) = delete;
    IParlioChannel& operator=(IParlioChannel&&) = delete;
};

}  // namespace fl
