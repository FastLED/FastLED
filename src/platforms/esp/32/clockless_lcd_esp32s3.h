/// @file clockless_lcd_esp32s3.h
/// @brief ESP32-S3 LCD/I80 parallel LED driver with memory-optimized 3-word encoding
///
/// This driver uses the ESP32-S3 LCD_CAM peripheral (I80 mode) to drive up to 16
/// identical WS28xx-style LED strips in parallel with automatic PCLK optimization.
///
/// Supported platforms:
/// - ESP32-S3: LCD_CAM peripheral with I80 interface (requires hal/lcd_ll.h)
///
/// Key features:
/// - Template-parameterized chipset binding (compile-time optimization)
/// - Automatic PCLK frequency calculation for optimal memory efficiency
/// - 3-word-per-bit encoding (6 bytes per bit) - same as I2S driver
/// - Pre-computed bit templates with bit-masking
/// - Memory usage: 144 KB per 1000 LEDs (identical to I2S driver)
/// NOTES: This should eventually be merged into the BlinkPattern sketch example.


#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This file is only for ESP32-S3"
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
#include "hal/lcd_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_periph.h"
#include "platforms/esp/esp_version.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "platforms/shared/clockless_timing.h"
#include "clockless_lcd_esp32s3_impl.hpp"

// Data alignment for PSRAM transfers
#ifndef LCD_DRIVER_PSRAM_DATA_ALIGNMENT
#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#endif

// Allow override for debugging/testing (not recommended for production)
#ifndef LCD_PCLK_HZ_OVERRIDE
#define LCD_PCLK_HZ_OVERRIDE 0
#endif

namespace fl {

/// @brief Configuration structure for LCD LED driver
struct LcdDriverConfig {
    int gpio_pins[16];           ///< GPIO numbers for data lanes D0-D15
    int num_lanes;               ///< Active lane count (1-16)
    int latch_us = 300;          ///< Reset gap duration (microseconds)
    bool use_psram = true;       ///< Allocate DMA buffers in PSRAM
    uint32_t pclk_hz_override = LCD_PCLK_HZ_OVERRIDE;  ///< Optional: Force specific PCLK
};

/// @brief Memory-optimized LCD parallel LED driver with template-based chipset binding
///
/// This driver achieves the same memory efficiency as the I2S driver (6 bytes per bit)
/// while providing per-chipset PCLK optimization and compile-time type safety.
///
/// @tparam CHIPSET Chipset timing trait (e.g., WS2812ChipsetTiming)
template <typename CHIPSET>
class LcdLedDriver {
public:
    /// @brief Fixed 3-word encoding for memory efficiency (matches I2S driver)
    static constexpr uint32_t N_BIT = 3;

    /// @brief Bytes per bit (3 words × 2 bytes)
    static constexpr uint32_t BYTES_PER_BIT = N_BIT * 2;

    /// @brief Calculate timing using shared ClocklessTiming module
    static constexpr ClocklessTimingResult calculate_timing() {
        if (LCD_PCLK_HZ_OVERRIDE > 0) {
            // If override is set, still use ClocklessTiming for validation
            // but we'll use the override frequency
            auto result = ClocklessTiming::calculate_optimal_pclk(
                CHIPSET::T1(), CHIPSET::T2(), CHIPSET::T3(),
                N_BIT, 1000000, 80000000, true
            );
            result.pclk_hz = LCD_PCLK_HZ_OVERRIDE;
            result.slot_ns = 1000000000UL / LCD_PCLK_HZ_OVERRIDE;
            return result;
        }

        return ClocklessTiming::calculate_optimal_pclk(
            CHIPSET::T1(),
            CHIPSET::T2(),
            CHIPSET::T3(),
            N_BIT,           // 3 words per bit
            1000000,         // 1 MHz min
            80000000,        // 80 MHz max
            true             // Round to MHz
        );
    }

    /// @brief Timing result (computed at compile-time)
    static constexpr ClocklessTimingResult TIMING = calculate_timing();

    /// @brief Optimized PCLK frequency (Hz)
    static constexpr uint32_t PCLK_HZ = TIMING.pclk_hz;

    /// @brief Slot duration (nanoseconds)
    static constexpr uint32_t SLOT_NS = TIMING.slot_ns;

    /// @brief Constructor
    LcdLedDriver()
        : config_{}
        , num_leds_(0)
        , strips_{}
        , template_bit0_{}
        , template_bit1_{}
        , bus_handle_(nullptr)
        , io_handle_(nullptr)
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
    ~LcdLedDriver() {
        end();
    }

    /// @brief Initialize driver with GPIO pins and LED count
    ///
    /// @param config Driver configuration (pins, lane count, options)
    /// @param leds_per_strip Number of LEDs in each strip (uniform across lanes)
    /// @return true on success, false on error
    bool begin(const LcdDriverConfig& config, int leds_per_strip);

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

    /// @brief Get slot count per bit
    constexpr uint32_t getSlotsPerBit() const { return N_BIT; }

    /// @brief Get optimized PCLK frequency (Hz)
    constexpr uint32_t getPclkHz() const { return PCLK_HZ; }

    /// @brief Get estimated frame time (microseconds)
    uint32_t getFrameTimeUs() const {
        return ClocklessTiming::calculate_frame_time_us(
            num_leds_, 24, N_BIT, SLOT_NS, config_.latch_us
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

    /// @brief DMA transfer complete callback (static, ISR context)
    static bool IRAM_ATTR dmaCallback(esp_lcd_panel_io_handle_t panel_io,
                                      esp_lcd_panel_io_event_data_t* edata,
                                      void* user_ctx);

    // Configuration
    LcdDriverConfig config_;
    int num_leds_;
    CRGB* strips_[16];

    // Pre-computed bit templates (3 words each for 3-slot encoding)
    uint16_t template_bit0_[N_BIT];
    uint16_t template_bit1_[N_BIT];

    // ESP-LCD handles
    esp_lcd_i80_bus_handle_t bus_handle_;
    esp_lcd_panel_io_handle_t io_handle_;

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

// Forward declarations for wrapper API (matches I2S driver pattern)
namespace fl {

/// @brief LCD_Esp32 wrapper class that uses RectangularDrawBuffer
/// This provides the same interface as I2S_Esp32 and ObjectFled
template <typename CHIPSET>
class LCD_Esp32 {
  public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

// Base version of this class allows dynamic pins and chipset selection.
template <EOrder RGB_ORDER, typename CHIPSET>
class ClocklessController_LCD_Esp32Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    LCD_Esp32<CHIPSET> mLCD_Esp32;
    int mPin;

  public:
    ClocklessController_LCD_Esp32Base(int pin): mPin(pin) {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mLCD_Esp32.beginShowLeds(mPin, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mLCD_Esp32.showPixels(mPin, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mLCD_Esp32.endShowLeds();
    }
};


// Template parameter for the data pin so that it conforms to the API.
template <int DATA_PIN, EOrder RGB_ORDER, typename CHIPSET>
class ClocklessController_LCD_Esp32
    : public ClocklessController_LCD_Esp32Base<RGB_ORDER, CHIPSET> {
  private:
    typedef ClocklessController_LCD_Esp32Base<RGB_ORDER, CHIPSET> Base;

    // Compile-time check for invalid pins
    static_assert(!(DATA_PIN == 19 || DATA_PIN == 20),
                  "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32-S2/S3 and CANNOT be used for LED output. "
                  "Using these pins WILL BREAK USB flashing capability. Please choose a different pin.");

  public:
    ClocklessController_LCD_Esp32(): Base(DATA_PIN) {};
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

};

// Convenience aliases for common chipsets (WS2812 is the default)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_LCD_Esp32_WS2812 = ClocklessController_LCD_Esp32<DATA_PIN, RGB_ORDER, WS2812ChipsetTiming>;

} // namespace fl
