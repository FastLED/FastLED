#pragma once
#include "platforms/esp/is_esp.h"

// The I2S parallel mode driver only works on ESP32 and ESP32-S2
// ESP32-S3: Use LCD_CAM peripheral instead (see lcd_driver_i80.h in FastLED)
// ESP32-C3, C2, C5, C6, H2, P4: Have completely different I2S peripheral architecture (no parallel mode)
#if defined(ESP32) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) && !defined(FL_IS_ESP_32C3) && !defined(FL_IS_ESP_32C5) && !defined(FL_IS_ESP_32C6) && !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4)

/// @file spi_hw_i2s_esp32.h
/// @brief ESP32 I2S-based 16-lane SPI hardware implementation
///
/// This file provides the ESP32-specific implementation of SpiHw16 using
/// Yves' I2S parallel mode driver for hardware-accelerated multi-strip SPI output.
///
/// **Key Features:**
/// - Up to 16 parallel SPI strips via I2S0 peripheral
/// - DMA-based transmission (async, zero CPU overhead during output)
/// - PSRAM+DMA support for large installations (8K+ LEDs per strip)
/// - Automatic APA102/SK9822 framing (start/end frames handled by Yves driver)
/// - Clock speeds from 1-40 MHz (configurable, chipset-dependent)
///
/// **Hardware Requirements:**
/// - ESP32 (original) or ESP32-S2 only
/// - ESP32-S3: NOT supported - use LCD_CAM peripheral instead (see lcd_driver_i80.h)
/// - I2S0 peripheral (uses parallel output mode, not audio mode)
/// - GPIO pins: Clock + Data (see pin mapping below)
/// - PSRAM highly recommended for >1000 LEDs per strip
///
/// **Pin Mapping:**
/// - Data pins: I2S0O_DATA_OUT8-23 (GPIO offset +8)
///   - ESP32: GPIOs 8-23 for data output (via GPIO matrix)
///   - ESP32-S2: Any GPIOs can be mapped (more flexible)
/// - Clock pin: I2S0_BCLK (any GPIO via GPIO matrix)
///
/// **Integration with Yves Driver:**
/// This implementation wraps `I2SClockBasedLedDriver` to adapt it to FastLED's
/// SpiHw16 interface. Key adaptations:
/// - Receive pre-interleaved buffer from SPIBusManager (via SPITransposer)
/// - Manage PSRAM+DMA buffer allocation
/// - Add error handling (Yves uses void returns + ESP_LOGE)
/// - Validate pin constraints
///
/// **Memory Management:**
/// The driver uses PSRAM+DMA when available (mandatory for large installations):
/// - ESP32-S3: EDMA supports PSRAM directly (`MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA`)
/// - ESP32 classic: Prefers internal DMA RAM, PSRAM requires cache workarounds
/// - Fallback: Internal RAM if PSRAM unavailable (limits to ~1000 LEDs per strip)
///
/// **Example:**
/// @code
/// #include <FastLED.h>
///
/// int data_pins[] = {8, 9, 10, 11, 12, 13, 14, 15};  // 8 strips
/// fl::Spi spi(18, data_pins, fl::SPI_HW);  // Clock on GPIO 18
/// if (!spi.ok()) {
///     FL_WARN("SPI init failed");
///     return;
/// }
///
/// // Write RGB data (APA102 frames added automatically)
/// spi.write(strip0, strip1, strip2, strip3, strip4, strip5, strip6, strip7);
/// spi.wait();  // Block until DMA completes
/// @endcode

#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/numeric_limits.h"
#include "platforms/shared/spi_hw_16.h"
#include "platforms/shared/spi_types.h"

// Include Yves' I2S driver
#include "third_party/yves/I2SClockBasedLedDriver/I2SClockBasedLedDriver.h"

// ESP32 SDK headers
#ifdef ESP32
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace fl {

/// @brief ESP32 I2S-based 16-lane SPI hardware implementation
/// @details Wraps Yves' I2SClockBasedLedDriver to provide SpiHw16 interface
///          for hardware-accelerated parallel SPI output.
///
/// **Buffer Management:**
/// - Receives pre-interleaved buffer from SPIBusManager via acquireDMABuffer()
/// - SPIBusManager calls SPITransposer::transpose16() to fill the buffer
/// - Format: [strip0_led0, strip1_led0, ..., strip0_led1, strip1_led1, ...]
/// - Yves driver handles transposition to I2S parallel format
/// - APA102 start/end frames managed by Yves driver
///
/// **Thread Safety:**
/// - Uses FreeRTOS semaphore for async completion signaling
/// - acquireDMABuffer() waits if previous transmission in progress
/// - Multiple transmit() calls without waitComplete() will fail (must wait first)
///
/// **Error Handling:**
/// - Initialization failures: NOT_INITIALIZED, ALLOCATION_FAILED, INVALID_PARAMETER
/// - Runtime failures: BUSY, BUFFER_TOO_LARGE
class SpiHwI2SESP32 : public SpiHw16 {
public:
    /// @brief Construct ESP32 I2S SPI hardware controller
    /// @param bus_id Bus identifier (0 for I2S0, only one I2S bus supported on ESP32)
    /// @note Call begin() to initialize with pin configuration
    SpiHwI2SESP32(int bus_id = 0);

    /// @brief Destructor - releases buffers and I2S resources
    ~SpiHwI2SESP32() override;

    /// @brief Initialize I2S peripheral with given configuration
    /// @param config Hardware configuration (clock pin, data pins, speed)
    /// @returns true if initialized successfully, false on error
    /// @note Counts active lanes from data0_pin through data15_pin (non-negative pins)
    /// @note Validates pin constraints, allocates buffers, initializes I2S peripheral
    bool begin(const Config& config) override;

    /// @brief Shutdown I2S peripheral and release resources
    /// @note Waits for any pending transmission to complete
    void end() override;

    /// @brief Acquire writable DMA buffer for zero-copy transmission
    /// @param bytes_per_lane Number of bytes needed per lane
    /// @returns DMABuffer containing buffer span or error code
    /// @note Automatically waits if previous transmission active
    /// @note SPIBusManager will call SPITransposer::transpose16() to fill this buffer
    /// @note Buffer format: interleaved [strip0_byte0, strip1_byte0, ..., strip0_byte1, ...]
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Transmit data from previously acquired DMA buffer
    /// @param mode Transmission mode (SYNC or ASYNC, currently async only)
    /// @returns true if transmitted successfully, false on error
    /// @note Must call acquireDMABuffer() before this
    /// @note Triggers I2S DMA transmission (returns immediately, DMA continues in background)
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;

    /// @brief Wait for async transmission to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (fl::numeric_limits<uint32_t>::max() = infinite)
    /// @returns true if completed successfully, false on timeout
    /// @note Uses FreeRTOS semaphore for efficient blocking
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override;

    /// @brief Check if a transmission is currently in progress
    /// @returns true if busy, false if idle
    bool isBusy() const override;

    /// @brief Get initialization status
    /// @returns true if initialized, false otherwise
    bool isInitialized() const override;

    /// @brief Get the SPI bus number/ID for this controller
    /// @returns 0 (I2S0 is the only bus on ESP32)
    int getBusId() const override;

    /// @brief Get the platform-specific peripheral name for this controller
    /// @returns "I2S0"
    const char* getName() const override;

private:
    // ========================================================================
    // Initialization Helpers
    // ========================================================================

    /// @brief Validate GPIO pin numbers for ESP32 constraints
    /// @param clock_pin Clock pin to validate
    /// @param data_pins Data pins to validate (only non-negative pins checked)
    /// @returns true if all pins valid, false otherwise
    /// @note Checks: valid range, no flash pins (6-11), no duplicates
    bool validate_pins(int clock_pin, const fl::vector<int>& data_pins);

    /// @brief Allocate DMA-capable buffer (PSRAM or internal RAM)
    /// @param size Buffer size in bytes
    /// @returns Pointer to buffer, or nullptr on failure
    /// @note Tries PSRAM+DMA first (ESP32-S3 EDMA), falls back to internal DMA RAM
    uint8_t* allocate_dma_buffer(size_t size);

    /// @brief Calculate clock dividers for target frequency
    /// @param target_hz Target clock frequency in Hz
    /// @returns Clock frequency in MHz (for Yves driver API)
    /// @note ESP32 I2S base clock is 80 MHz (PLL_D2_CLK)
    int calculate_clock_mhz(uint32_t target_hz);

    /// @brief Count active lanes from Config struct
    /// @param config Configuration with data0_pin through data15_pin
    /// @returns Number of active lanes (non-negative pins)
    int count_active_lanes(const Config& config);

    /// @brief Extract active data pins from Config struct
    /// @param config Configuration with data0_pin through data15_pin
    /// @returns Vector of active pin numbers
    fl::vector<int> extract_data_pins(const Config& config);

    // ========================================================================
    // Member Variables
    // ========================================================================

    I2SClockBasedLedDriver mDriver;      ///< Yves' I2S driver instance
    uint8_t* mInterleavedBuffer;        ///< Interleaved buffer for Yves driver
    size_t mBufferSize;                 ///< Size of interleaved buffer in bytes
    DMABuffer mCurrentBuffer;           ///< Current DMABuffer from acquireDMABuffer() (data copied to mInterleavedBuffer in transmit())
    fl::vector<int> mDataPins;          ///< Copy of data pin numbers
    int mClockPin;                      ///< Clock pin number
    uint32_t mClockSpeedHz;            ///< Clock speed in Hz
    int mNumStrips;                     ///< Number of strips (lane count)
    int mNumLedsPerStrip;             ///< Number of LEDs per strip (0 until first write)
    int mBusId;                         ///< Bus ID (always 0 for I2S0)
    bool mIsInitialized;                ///< True if initialization succeeded
};

} // namespace fl

#endif // ESP32 (excluding S3/C3/C2/C5/C6/H2)
