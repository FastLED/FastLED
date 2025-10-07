/// @file parlio_driver.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED driver interface
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Key features:
/// - Simultaneous output to multiple LED strips
/// - DMA-based transmission (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - Template-parameterized for different channel counts

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "driver/parlio_tx.h"
#include "driver/gpio.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/namespace.h"
#include "fl/printf.h"
#include "platforms/shared/clockless_timing.h"

namespace fl {

/// @brief Buffer breaking strategy for DMA transmission
enum class ParlioBufferStrategy {
    /// @brief Single monolithic buffer (original implementation)
    /// May experience visible glitches if DMA gaps occur mid-component
    MONOLITHIC = 0,

    /// @brief Break buffers at LSB boundaries of each color component
    /// Ensures DMA gaps only affect LSB, making errors imperceptible (±1 brightness)
    /// Breaks after each complete color byte: G[7:0], R[7:0], B[7:0]
    BREAK_PER_COLOR = 1
};

/// @brief Configuration structure for PARLIO LED driver
struct ParlioDriverConfig {
    int clk_gpio;                ///< GPIO number for clock output
    int data_gpios[16];          ///< GPIO numbers for data lanes (up to 16)
    int num_lanes;               ///< Active lane count (1, 2, 4, 8, or 16)
    uint32_t clock_freq_hz;      ///< PARLIO clock frequency (e.g., 12000000 for 12MHz)
    ParlioBufferStrategy buffer_strategy; ///< Buffer breaking strategy (default: BREAK_PER_COLOR)
};

/// @brief Abstract base for PARLIO driver (enables runtime polymorphism)
class ParlioLedDriverBase {
public:
    virtual ~ParlioLedDriverBase() = default;
    virtual bool begin(const ParlioDriverConfig& config, uint16_t num_leds) = 0;
    virtual void end() = 0;
    virtual void set_strip(uint8_t channel, CRGB* leds) = 0;
    virtual void show_grb() = 0;
    virtual void show_rgb() = 0;
    virtual void show_bgr() = 0;
    virtual void wait() = 0;
    virtual bool is_initialized() const = 0;
};

/// @brief PARLIO parallel LED driver with template-based configuration
///
/// This driver uses the ESP32-P4's Parallel IO TX peripheral to simultaneously
/// drive multiple LED strips with hardware-timed output and DMA transfers.
///
/// @tparam DATA_WIDTH Number of parallel data lanes (1, 2, 4, 8, or 16)
/// @tparam CHIPSET Chipset timing trait (e.g., WS2812ChipsetTiming)
template <uint8_t DATA_WIDTH, typename CHIPSET>
class ParlioLedDriver : public ParlioLedDriverBase {
public:
    static_assert(DATA_WIDTH >= 1 && DATA_WIDTH <= 16, "DATA_WIDTH must be 1-16");
    static_assert((DATA_WIDTH == 1) || (DATA_WIDTH == 2) || (DATA_WIDTH == 4) ||
                  (DATA_WIDTH == 8) || (DATA_WIDTH == 16),
                  "DATA_WIDTH must be power of 2 (1, 2, 4, 8, or 16)");

    /// @brief Default clock frequency for WS2812 timing
    static constexpr uint32_t DEFAULT_CLOCK_FREQ_HZ = 12000000;  // 12 MHz

    /// @brief Constructor
    ParlioLedDriver();

    /// @brief Destructor
    ~ParlioLedDriver();

    /// @brief Initialize driver with GPIO pins and LED count
    ///
    /// @param config Driver configuration (pins, lane count, clock frequency)
    /// @param num_leds Number of LEDs per strip
    /// @return true if initialization succeeded
    bool begin(const ParlioDriverConfig& config, uint16_t num_leds) override;

    /// @brief Shutdown driver and free resources
    void end() override;

    /// @brief Set LED strip data pointer for a specific channel
    ///
    /// @param channel Channel index (0 to DATA_WIDTH-1)
    /// @param leds Pointer to LED data array
    void set_strip(uint8_t channel, CRGB* leds) override;

    /// @brief Show LED data - transmit to all strips
    ///
    /// @tparam RGB_ORDER Color order (e.g., GRB, RGB, BGR)
    template<EOrder RGB_ORDER = GRB>
    void show();

    /// @brief Wait for current transmission to complete
    void wait() override;

    /// @brief Check if driver is initialized
    bool is_initialized() const override;

    /// @brief Virtual show methods for base class interface
    void show_grb() override;
    void show_rgb() override;
    void show_bgr() override;

private:
    /// @brief Pack LED data into PARLIO format
    ///
    /// For each LED position and each of 24 color bits (in specified order, MSB-first):
    /// - Collect the same bit position from all DATA_WIDTH strips
    /// - Pack into single output byte
    ///
    /// @tparam RGB_ORDER Color order for output (GRB for WS2812)
    template<EOrder RGB_ORDER>
    void pack_data();

    /// @brief Map output position to CRGB byte offset
    ///
    /// CRGB is stored in memory as: struct { uint8_t r, g, b; }
    /// So byte offsets are: r=0, g=1, b=2
    ///
    /// @tparam RGB_ORDER Desired output color order
    /// @param output_pos Position in output stream (0, 1, or 2)
    /// @return Byte offset into CRGB structure
    template<EOrder RGB_ORDER>
    static constexpr uint8_t get_crgb_byte_offset(uint8_t output_pos);

    /// @brief PARLIO TX completion callback
    static bool IRAM_ATTR parlio_tx_done_callback(parlio_tx_unit_handle_t tx_unit,
                                                   const parlio_tx_done_event_data_t* edata,
                                                   void* user_ctx);

    ParlioDriverConfig config_;     ///< Driver configuration
    uint16_t num_leds_;             ///< Number of LEDs per strip
    CRGB* strips_[16];              ///< Pointers to LED data for each strip
    parlio_tx_unit_handle_t tx_unit_; ///< PARLIO TX unit handle
    uint8_t* dma_buffer_;           ///< DMA buffer for bit-packed data (monolithic mode)
    uint8_t* dma_sub_buffers_[3];   ///< Sub-buffers for BREAK_PER_COLOR mode (G, R, B)
    size_t buffer_size_;            ///< Size of DMA buffer in bytes (total for all sub-buffers)
    size_t sub_buffer_size_;        ///< Size of each sub-buffer in bytes (BREAK_PER_COLOR mode)
    SemaphoreHandle_t xfer_done_sem_; ///< Semaphore for transfer completion
    volatile bool dma_busy_;        ///< Flag indicating DMA transfer in progress
};

// Include template implementation
// allow-include-after-namespace
#include "parlio_driver_impl.h"

}  // namespace fl
