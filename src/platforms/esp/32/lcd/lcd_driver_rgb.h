/// @file lcd_driver_rgb.h
/// @brief ESP32 RGB LCD parallel LED driver with 4-pixel encoding
///
/// This driver uses the RGB LCD peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Supported platforms:
/// - ESP32-P4: RGB LCD controller (requires esp_lcd_panel_ops.h)
/// - Future ESP32 variants with RGB LCD support
///
/// Key features:
/// - Template-parameterized chipset binding (compile-time optimization)
/// - Automatic PCLK frequency calculation for WS2812 timing
/// - 4-pixel-per-bit encoding (8 bytes per bit)
/// - RGB LCD peripheral with HSYNC/VSYNC/DE signals
/// - Up to 16 parallel strips via data bus width

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
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "platforms/esp/esp_version.h"

#include "crgb.h"
#include "platforms/shared/clockless_timing.h"
#include "lcd_driver_common.h"
#include "lcd_driver_rgb_impl.h"

namespace fl {

/// @brief Configuration structure for RGB LCD driver
struct LcdRgbDriverConfig {
    int pclk_gpio;               ///< GPIO for pixel clock output
    int vsync_gpio = -1;         ///< GPIO for VSYNC (optional, -1 to disable)
    int hsync_gpio = -1;         ///< GPIO for HSYNC (optional, -1 to disable)
    int de_gpio = -1;            ///< GPIO for data enable (optional, -1 to disable)
    int disp_gpio = -1;          ///< GPIO for display enable (optional, -1 to disable)
    int data_gpios[16];          ///< GPIO numbers for data lanes D0-D15
    int num_lanes;               ///< Active lane count (1-16)
    int latch_us = 300;          ///< Reset gap duration (microseconds)
    bool use_psram = true;       ///< Allocate DMA buffers in PSRAM
    uint32_t pclk_hz_override = LCD_PCLK_HZ_OVERRIDE;  ///< Optional: Force specific PCLK
};

/// @brief RGB LCD parallel LED driver with template-based chipset binding
///
/// This driver uses the RGB LCD peripheral with 4-pixel encoding
/// to generate precise WS2812 timing on up to 16 parallel data lines.
///
/// @tparam LED_CHIPSET LED chipset timing trait (e.g., WS2812ChipsetTiming)
template <typename LED_CHIPSET>
class LcdRgbDriver {
public:
    /// @brief Fixed 4-pixel encoding for WS2812 timing
    /// At 3.2 MHz PCLK (312.5ns per pixel):
    /// - Bit 0: [HI, LO, LO, LO] = 312ns high, 938ns low
    /// - Bit 1: [HI, HI, LO, LO] = 625ns high, 625ns low
    static constexpr uint32_t N_PIXELS = 4;

    /// @brief Bytes per bit (4 pixels × 2 bytes per pixel)
    static constexpr uint32_t BYTES_PER_BIT = N_PIXELS * 2;

    /// @brief Calculate timing using shared ClocklessTiming module
    static constexpr ClocklessTimingResult calculate_timing() {
        if constexpr (LCD_PCLK_HZ_OVERRIDE > 0) {
            // If override is set, use it directly
            auto result = ClocklessTiming::calculate_optimal_pclk(
                LED_CHIPSET::T1(), LED_CHIPSET::T2(), LED_CHIPSET::T3(),
                N_PIXELS, 1000000, 40000000, true
            );
            result.pclk_hz = LCD_PCLK_HZ_OVERRIDE;
            result.slot_ns = 1000000000UL / LCD_PCLK_HZ_OVERRIDE;
            return result;
        }

        return ClocklessTiming::calculate_optimal_pclk(
            LED_CHIPSET::T1(),
            LED_CHIPSET::T2(),
            LED_CHIPSET::T3(),
            N_PIXELS,        // 4 pixels per bit
            1000000,         // 1 MHz min
            40000000,        // 40 MHz max (conservative for WS2812)
            true             // Round to MHz
        );
    }

    /// @brief Timing result (computed at compile-time)
    static constexpr ClocklessTimingResult TIMING = calculate_timing();

    /// @brief Optimized PCLK frequency (Hz)
    static constexpr uint32_t PCLK_HZ = TIMING.pclk_hz;

    /// @brief Pixel duration (nanoseconds)
    static constexpr uint32_t PIXEL_NS = TIMING.slot_ns;

    /// @brief Constructor
    LcdRgbDriver()
        : config_{}
        , num_leds_(0)
        , strips_{}
        , template_bit0_{}
        , template_bit1_{}
        , panel_handle_(nullptr)
        , buffers_{nullptr, nullptr}
        , buffer_size_(0)
        , front_buffer_(0)
        , xfer_done_sem_(nullptr)
        , dma_busy_(false)
        , frame_counter_(0)
    {
        for (int i = 0; i < 16; i++) {
            strips_[i] = nullptr;
        }
    }

    /// @brief Destructor
    ~LcdRgbDriver() {
        end();
    }

    /// @brief Initialize driver with GPIO pins and LED count
    ///
    /// @param config Driver configuration (pins, lane count, options)
    /// @param leds_per_strip Number of LEDs in each strip (uniform across lanes)
    /// @return true on success, false on error
    bool begin(const LcdRgbDriverConfig& config, int leds_per_strip);

    /// @brief Shutdown driver and free resources
    void end();

    /// @brief Attach per-lane LED strip data
    ///
    /// @param strips Array of CRGB pointers (size = num_lanes from config)
    void attachStrips(CRGB** strips) {
        for (int i = 0; i < config_.num_lanes && i < 16; i++) {
            strips_[i] = strips[i];
        }
    }

    /// @brief Attach single strip to specific lane
    ///
    /// @param lane Lane index (0 to num_lanes-1)
    /// @param strip Pointer to LED data array
    void attachStrip(int lane, CRGB* strip) {
        if (lane >= 0 && lane < 16) {
            strips_[lane] = strip;
        }
    }

    /// @brief Encode current LED data and start DMA transfer
    ///
    /// @return true if transfer started, false if previous transfer still active
    bool show();

    /// @brief Block until current DMA transfer completes
    void wait();

    /// @brief Check if DMA transfer is in progress
    ///
    /// @return true if busy, false if idle
    bool busy() const { return dma_busy_; }

    /// @brief Get actual timing after quantization (nanoseconds)
    void getActualTiming(uint32_t& T1_actual, uint32_t& T2_actual, uint32_t& T3_actual) const {
        T1_actual = TIMING.actual_T1_ns;
        T2_actual = TIMING.actual_T2_ns;
        T3_actual = TIMING.actual_T3_ns;
    }

    /// @brief Get timing error percentage
    void getTimingError(float& err_T1, float& err_T2, float& err_T3) const {
        err_T1 = TIMING.error_T1;
        err_T2 = TIMING.error_T2;
        err_T3 = TIMING.error_T3;
    }

    /// @brief Get timing calculation result
    constexpr ClocklessTimingResult getTiming() const { return TIMING; }

    /// @brief Get pixels per bit
    constexpr uint32_t getPixelsPerBit() const { return N_PIXELS; }

    /// @brief Get optimized PCLK frequency (Hz)
    constexpr uint32_t getPclkHz() const { return PCLK_HZ; }

    /// @brief Get estimated frame time (microseconds)
    uint32_t getFrameTimeUs() const {
        return ClocklessTiming::calculate_frame_time_us(
            num_leds_, 24, N_PIXELS, PIXEL_NS, config_.latch_us
        );
    }

    /// @brief Get buffer memory usage (bytes, per buffer)
    size_t getBufferSize() const {
        return buffer_size_;
    }

private:
    /// @brief Generate bit-0 and bit-1 templates (called during initialization)
    void generateTemplates();

    /// @brief Encode frame data into DMA buffer using templates
    ///
    /// @param buffer_index Buffer index (0 or 1)
    void encodeFrame(int buffer_index);

    /// @brief RGB panel draw complete callback (static, ISR context, IRAM)
    static bool IRAM_ATTR drawCallback(esp_lcd_panel_handle_t panel,
                                       void* edata,
                                       void* user_ctx);

    // Configuration
    LcdRgbDriverConfig config_;
    int num_leds_;
    CRGB* strips_[16];

    // Pre-computed bit templates (4 pixels each for 4-pixel encoding)
    uint16_t template_bit0_[N_PIXELS];
    uint16_t template_bit1_[N_PIXELS];

    // ESP-LCD RGB panel handle
    esp_lcd_panel_handle_t panel_handle_;

    // DMA buffers (double-buffered)
    uint16_t* buffers_[2];
    size_t buffer_size_;
    int front_buffer_;

    // Synchronization
    SemaphoreHandle_t xfer_done_sem_;
    volatile bool dma_busy_;
    uint32_t frame_counter_;
};

}  // namespace fl
