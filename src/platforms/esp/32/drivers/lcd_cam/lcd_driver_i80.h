/// @file lcd_driver_i80.h
/// @brief ESP32 I80/LCD_CAM parallel LED driver with memory-optimized 3-word encoding
///
/// This driver uses the LCD_CAM peripheral in I80 mode to drive up to 16
/// identical WS28xx-style LED strips in parallel with automatic PCLK optimization.
///
/// Supported platforms:
/// - ESP32-S3: LCD_CAM peripheral with I80 interface (requires hal/lcd_ll.h)
/// - ESP32-P4: I80 interface (if available on hardware)
///
/// Key features:
/// - Template-parameterized chipset binding (compile-time optimization)
/// - Automatic PCLK frequency calculation for optimal memory efficiency
/// - 3-word-per-bit encoding (6 bytes per bit) - same as I2S driver
/// - Pre-computed bit templates with bit-masking
/// - Memory usage: 144 KB per 1000 LEDs (identical to I2S driver)

#pragma once

// Feature-based platform detection for I80 LCD peripheral
// This driver requires:
// - ESP LCD panel I/O API (esp_lcd_panel_io.h)
// - LCD HAL layer (hal/lcd_hal.h and hal/lcd_ll.h)
// - LCD peripheral register definitions (soc/lcd_periph.h)
#if !__has_include("esp_lcd_panel_ops.h")
#error "I80 LCD driver requires esp_lcd_panel_ops.h (ESP-IDF LCD component)"
#endif

#if !__has_include("hal/lcd_hal.h") || !__has_include("hal/lcd_ll.h")
#error "I80 LCD peripheral not available on this platform - requires LCD_CAM or I80 interface"
#endif

#if !__has_include("soc/lcd_periph.h")
#error "I80 LCD peripheral not available on this platform - missing LCD peripheral definitions"
#endif

#include "sdkconfig.h"

#include "fl/stdint.h"
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
#include "platforms/shared/clockless_timing.h"
#include "lcd_driver_common.h"
#include "lcd_driver_base.h"

namespace fl {

/// @brief Runtime timing configuration for I80 LCD driver
/// Stores compile-time chipset timing values as runtime values for the concrete base class
struct LcdI80TimingConfig {
    uint32_t T1;          ///< T1 timing (nanoseconds)
    uint32_t T2;          ///< T2 timing (nanoseconds)
    uint32_t T3;          ///< T3 timing (nanoseconds)
    uint32_t reset_us;    ///< Reset/latch time (microseconds)
    uint32_t pclk_hz;     ///< Optimized PCLK frequency (Hz)
    uint32_t slot_ns;     ///< Slot duration (nanoseconds)
    uint32_t actual_T1;   ///< Actual T1 after quantization (nanoseconds)
    uint32_t actual_T2;   ///< Actual T2 after quantization (nanoseconds)
    uint32_t actual_T3;   ///< Actual T3 after quantization (nanoseconds)
    float error_T1;       ///< T1 timing error percentage
    float error_T2;       ///< T2 timing error percentage
    float error_T3;       ///< T3 timing error percentage
    const char* chipset_name; ///< Chipset name for logging
};

/// @brief Concrete I80 LED driver base class (non-template)
///
/// This class contains all the actual implementation, accepting runtime timing configuration.
/// The template wrapper LcdI80Driver<CHIPSET> extracts compile-time values and passes them here.
class LcdI80DriverBase : public LcdDriverBase {
public:
    /// @brief Fixed 3-word encoding for memory efficiency (matches I2S driver)
    static constexpr uint32_t N_BIT = 3;

    /// @brief Bytes per bit (3 words × 2 bytes)
    static constexpr uint32_t BYTES_PER_BIT = N_BIT * 2;

    /// @brief Constructor with timing configuration
    ///
    /// @param timing_config Runtime timing configuration extracted from chipset template
    LcdI80DriverBase(const LcdI80TimingConfig& timing_config)
        : LcdDriverBase()
        , timing_(timing_config)
        , config_{}
        , template_bit0_{}
        , template_bit1_{}
        , bus_handle_(nullptr)
        , io_handle_(nullptr)
    {
    }

    /// @brief Destructor
    ~LcdI80DriverBase() {
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

    /// @brief Attach per-lane LED strip data (overload for config-aware attachment)
    ///
    /// @param strips Array of CRGB pointers (size = num_lanes from config)
    void attachStrips(CRGB** strips) {
        LcdDriverBase::attachStrips(strips, config_.num_lanes);
    }

    /// @brief Encode current LED data and start DMA transfer
    ///
    /// @return true if transfer started, false if previous transfer still active
    bool show();

    /// @brief Get actual timing after quantization (nanoseconds)
    void getActualTiming(uint32_t& T1_actual, uint32_t& T2_actual, uint32_t& T3_actual) const {
        T1_actual = timing_.actual_T1;
        T2_actual = timing_.actual_T2;
        T3_actual = timing_.actual_T3;
    }

    /// @brief Get timing error percentage
    void getTimingError(float& err_T1, float& err_T2, float& err_T3) const {
        err_T1 = timing_.error_T1;
        err_T2 = timing_.error_T2;
        err_T3 = timing_.error_T3;
    }

    /// @brief Get timing configuration
    const LcdI80TimingConfig& getTiming() const { return timing_; }

    /// @brief Get slot count per bit
    constexpr uint32_t getSlotsPerBit() const { return N_BIT; }

    /// @brief Get optimized PCLK frequency (Hz)
    uint32_t getPclkHz() const { return timing_.pclk_hz; }

    /// @brief Get estimated frame time (microseconds)
    uint32_t getFrameTimeUs() const {
        return ClocklessTiming::calculate_frame_time_us(
            num_leds_, 24, N_BIT, timing_.slot_ns, config_.latch_us
        );
    }

private:
    /// @brief Generate bit-0 and bit-1 templates (called during initialization)
    void generateTemplates();

    /// @brief Encode frame data into DMA buffer using templates
    ///
    /// @param buffer_index Buffer index (0 or 1)
    void encodeFrame(int buffer_index);

    /// @brief DMA transfer complete callback (static, ISR context, IRAM)
    static bool IRAM_ATTR dmaCallback(esp_lcd_panel_io_handle_t panel_io,
                                      esp_lcd_panel_io_event_data_t* edata,
                                      void* user_ctx);

    // Timing configuration (runtime values)
    LcdI80TimingConfig timing_;

    // Configuration (driver-specific)
    LcdDriverConfig config_;

    // Pre-computed bit templates (3 words each for 3-slot encoding)
    uint16_t template_bit0_[N_BIT];
    uint16_t template_bit1_[N_BIT];

    // ESP-LCD handles (I80-specific)
    esp_lcd_i80_bus_handle_t bus_handle_;
    esp_lcd_panel_io_handle_t io_handle_;
};

/// @brief Template wrapper for LcdI80DriverBase with compile-time chipset binding
///
/// This thin wrapper extracts compile-time timing values from the LED_CHIPSET template
/// parameter and passes them as runtime configuration to the concrete base class.
///
/// @tparam LED_CHIPSET LED chipset timing trait (e.g., WS2812ChipsetTiming)
template <typename LED_CHIPSET>
class LcdI80Driver : public LcdI80DriverBase {
public:
    /// @brief Fixed 3-word encoding for memory efficiency (matches I2S driver)
    static constexpr uint32_t N_BIT = 3;

    /// @brief Helper to calculate slot_ns from pclk_hz (avoids division-by-zero warning)
    static constexpr uint32_t calculate_slot_ns(uint32_t pclk_hz) {
        return pclk_hz > 0 ? 1000000000UL / pclk_hz : 0;
    }

    /// @brief Calculate timing using shared ClocklessTiming module
    static constexpr ClocklessTimingResult calculate_timing() {
        if constexpr (LCD_PCLK_HZ_OVERRIDE > 0) {
            // If override is set, still use ClocklessTiming for validation
            // but we'll use the override frequency
            auto result = ClocklessTiming::calculate_optimal_pclk(
                LED_CHIPSET::T1, LED_CHIPSET::T2, LED_CHIPSET::T3,
                N_BIT, 1000000, 80000000, true
            );
            // Use override frequency (constexpr guarantees > 0 in this branch)
            result.pclk_hz = LCD_PCLK_HZ_OVERRIDE;
            result.slot_ns = calculate_slot_ns(LCD_PCLK_HZ_OVERRIDE);
            return result;
        }

        return ClocklessTiming::calculate_optimal_pclk(
            LED_CHIPSET::T1,
            LED_CHIPSET::T2,
            LED_CHIPSET::T3,
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

    /// @brief Constructor - converts compile-time timing to runtime config
    LcdI80Driver()
        : LcdI80DriverBase(LcdI80TimingConfig{
            LED_CHIPSET::T1,        // T1
            LED_CHIPSET::T2,        // T2
            LED_CHIPSET::T3,        // T3
            LED_CHIPSET::RESET,     // reset_us
            PCLK_HZ,                // pclk_hz
            SLOT_NS,                // slot_ns
            TIMING.actual_T1_ns,    // actual_T1
            TIMING.actual_T2_ns,    // actual_T2
            TIMING.actual_T3_ns,    // actual_T3
            TIMING.error_T1,        // error_T1
            TIMING.error_T2,        // error_T2
            TIMING.error_T3,        // error_T3
            LED_CHIPSET::name()     // chipset_name
        })
    {
    }
};

}  // namespace fl
