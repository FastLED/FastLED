/// @file spi_hw_i2s_esp32.cpp
/// @brief ESP32 I2S-based 16-lane SPI hardware implementation
///
/// This file provides the implementation of SpiHwI2SESP32, which wraps
/// Yves' I2SClockBasedLedDriver to provide FastLED's SpiHw16 interface.

#include "spi_hw_i2s_esp32.h"
#include "fl/dbg.h"
#include "platforms/esp/is_esp.h"

// The I2S parallel mode driver only works on ESP32 and ESP32-S2
// ESP32-S3: Use LCD_CAM peripheral instead (see lcd_driver_i80.h in FastLED)
// ESP32-C3, C2, C5, C6, H2, P4: Have completely different I2S peripheral architecture (no parallel mode)
#if defined(ESP32) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) && !defined(FL_IS_ESP_32C3) && !defined(FL_IS_ESP_32C5) && !defined(FL_IS_ESP_32C6) && !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4)

// Compatibility for ESP-IDF 3.3: heap_caps_aligned_alloc was added in IDF 4.1
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR <= 3
static inline void* heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    (void)alignment;  // IDF 3.3 heap_caps_malloc doesn't support alignment
    return heap_caps_malloc(size, caps);
}
#endif

namespace fl {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SpiHwI2SESP32::SpiHwI2SESP32(int bus_id)
    : interleaved_buffer_(nullptr),
      buffer_size_(0),
      clock_pin_(-1),
      clock_speed_hz_(0),
      num_strips_(0),
      num_leds_per_strip_(0),
      bus_id_(bus_id),
      is_initialized_(false) {
    // Initialization happens in begin()
}

SpiHwI2SESP32::~SpiHwI2SESP32() {
    end();
}

// ============================================================================
// SpiHw16 Interface Implementation
// ============================================================================

bool SpiHwI2SESP32::begin(const Config& config) {
    // Step 1: Extract data pins from config
    data_pins_ = extract_data_pins(config);
    num_strips_ = static_cast<int>(data_pins_.size());
    clock_pin_ = config.clock_pin;
    clock_speed_hz_ = config.clock_speed_hz;

    // Step 2: Validate pin configuration
    if (!validate_pins(clock_pin_, data_pins_)) {
        FL_WARN("SpiHwI2SESP32: Invalid pin configuration");
        return false;
    }

    // Step 3: Validate lane count (1-16)
    if (num_strips_ < 1 || num_strips_ > 16) {
        FL_WARN("SpiHwI2SESP32: Invalid lane count " << num_strips_ << " (must be 1-16)");
        return false;
    }

    // Step 4: Calculate clock dividers
    int clock_mhz = calculate_clock_mhz(clock_speed_hz_);
    if (clock_mhz < 1 || clock_mhz > 40) {
        FL_WARN("SpiHwI2SESP32: Invalid clock speed " << clock_speed_hz_ << " Hz (must be 1-40 MHz)");
        return false;
    }

    // Step 5: Allocate initial buffer (size will be adjusted on first write)
    // Initial size: 1000 LEDs per strip × 3 bytes RGB × num_strips
    const size_t initial_leds = 1000;
    buffer_size_ = initial_leds * 3 * num_strips_;  // RGB only (driver adds APA102 frames)
    interleaved_buffer_ = allocate_dma_buffer(buffer_size_);

    if (interleaved_buffer_ == nullptr) {
        FL_WARN("SpiHwI2SESP32: Failed to allocate DMA buffer (" << buffer_size_ << " bytes)");
        return false;
    }

    // Step 6: Initialize Yves driver
    // Note: Yves driver expects interleaved buffer pointer, pin arrays, clock pin, etc.
    // The driver will allocate its own internal DMA buffers for transposed data
    driver_.initled(
        interleaved_buffer_,         // LED data buffer (will be updated on each write)
        data_pins_.data(),            // Data pin array
        clock_pin_,                   // Clock pin
        num_strips_,                  // Number of strips
        static_cast<int>(initial_leds),  // Initial LEDs per strip (will be updated on write)
        clock_mhz                     // Clock speed in MHz
    );

    // Initialization complete
    is_initialized_ = true;
    return true;
}

void SpiHwI2SESP32::end() {
    // Wait for any pending transmission to complete
    if (is_initialized_) {
        waitComplete(5000);  // 5 second timeout
    }

    // Free interleaved buffer
    if (interleaved_buffer_ != nullptr) {
        heap_caps_free(interleaved_buffer_);
        interleaved_buffer_ = nullptr;
    }

    is_initialized_ = false;

    // Note: Yves driver destructor will clean up its own resources
}

DMABuffer SpiHwI2SESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!is_initialized_) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Step 1: Wait for previous transmission if busy
    if (isBusy()) {
        if (!waitComplete(5000)) {  // 5 second timeout
            FL_WARN("SpiHwI2SESP32: Timeout waiting for previous transmission");
            return DMABuffer(SPIError::BUSY);
        }
    }

    // Step 2: Calculate required buffer size
    // Format: interleaved [strip0_byte0, strip1_byte0, ..., strip0_byte1, ...]
    size_t required_size = bytes_per_lane * num_strips_;

    // Step 3: Check if buffer needs resizing
    if (required_size > buffer_size_) {
        // Need to reallocate larger buffer
        uint8_t* new_buffer = allocate_dma_buffer(required_size);
        if (new_buffer == nullptr) {
            FL_WARN("SpiHwI2SESP32: Failed to allocate larger buffer (" << required_size << " bytes)");
            return DMABuffer(SPIError::ALLOCATION_FAILED);
        }

        // Free old buffer
        if (interleaved_buffer_ != nullptr) {
            heap_caps_free(interleaved_buffer_);
        }

        interleaved_buffer_ = new_buffer;
        buffer_size_ = required_size;
        driver_.leds = interleaved_buffer_;  // Update driver's pointer
    }

    // Step 4: Update LED count if changed
    size_t num_leds = bytes_per_lane / 3;  // Assume RGB format (3 bytes per LED)
    if (static_cast<int>(num_leds) != num_leds_per_strip_) {
        num_leds_per_strip_ = static_cast<int>(num_leds);
        driver_.num_led_per_strip = num_leds_per_strip_;
    }

    // Step 5: Return DMABuffer that SPIBusManager will write to
    // SPIBusManager will call SPITransposer::transpose16() to fill this buffer
    // We store this buffer and will copy its data to interleaved_buffer_ in transmit()
    current_buffer_ = DMABuffer(required_size);
    return current_buffer_;
}

bool SpiHwI2SESP32::transmit(TransmitMode mode) {
    (void)mode;  // Async mode only for now

    if (!is_initialized_) {
        return false;
    }

    // Copy data from current_buffer_ to interleaved_buffer_
    // SPIBusManager writes to current_buffer_, but Yves driver reads from interleaved_buffer_
    if (current_buffer_.ok()) {
        fl::span<uint8_t> src = current_buffer_.data();
        if (src.size() <= buffer_size_) {
            fl::memcpy(interleaved_buffer_, src.data(), src.size());
        } else {
            FL_WARN("SpiHwI2SESP32: Buffer size mismatch in transmit()");
            return false;
        }
    } else {
        FL_WARN("SpiHwI2SESP32: No valid buffer to transmit");
        return false;
    }

    // Trigger I2S DMA transmission (async)
    // Data is now in interleaved_buffer_ (Yves driver will read from this)
    driver_.showPixels();

    return true;
}

bool SpiHwI2SESP32::waitComplete(uint32_t timeout_ms) {
    if (!is_initialized_) {
        return false;
    }

    // Check if transmission in progress
    if (!driver_.isDisplaying) {
        return true;  // Nothing to wait for
    }

    // Wait for semaphore with timeout
    // Note: Yves driver uses xSemaphoreTake in showPixels() which blocks until complete
    // The driver sets isDisplaying=false when done
    // We need to poll isDisplaying flag with timeout

    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (driver_.isDisplaying) {
        vTaskDelay(pdMS_TO_TICKS(1));  // Check every 1ms

        if (timeout_ms != UINT32_MAX) {  // Check for infinite timeout
            TickType_t elapsed_ticks = xTaskGetTickCount() - start_ticks;
            if (elapsed_ticks >= timeout_ticks) {
                return false;  // Timeout
            }
        }
    }

    return true;
}

bool SpiHwI2SESP32::isBusy() const {
    return is_initialized_ && driver_.isDisplaying;
}

bool SpiHwI2SESP32::isInitialized() const {
    return is_initialized_;
}

int SpiHwI2SESP32::getBusId() const {
    return bus_id_;
}

const char* SpiHwI2SESP32::getName() const {
    return "I2S0";
}

// ============================================================================
// Private Helper Functions
// ============================================================================

bool SpiHwI2SESP32::validate_pins(int clock_pin, const fl::vector<int>& data_pins) {
    // Check pin count (1-16)
    if (data_pins.size() < 1 || data_pins.size() > 16) {
        FL_WARN("SpiHwI2SESP32: Invalid pin count " << data_pins.size() << " (must be 1-16)");
        return false;
    }

    // Check clock pin range
    if (clock_pin < 0 || clock_pin >= GPIO_NUM_MAX) {
        FL_WARN("SpiHwI2SESP32: Invalid clock pin " << clock_pin);
        return false;
    }

    // Check for flash pins (6-11 on ESP32)
    if (clock_pin >= 6 && clock_pin <= 11) {
        FL_WARN("SpiHwI2SESP32: Clock pin " << clock_pin << " conflicts with flash (6-11 forbidden)");
        return false;
    }

    // Check data pins
    fl::vector<bool> seen(GPIO_NUM_MAX, false);
    seen[clock_pin] = true;  // Mark clock pin as used

    for (int pin : data_pins) {
        // Check range
        if (pin < 0 || pin >= GPIO_NUM_MAX) {
            FL_WARN("SpiHwI2SESP32: Invalid data pin " << pin);
            return false;
        }

        // Check for flash pins
        if (pin >= 6 && pin <= 11) {
            FL_WARN("SpiHwI2SESP32: Data pin " << pin << " conflicts with flash (6-11 forbidden)");
            return false;
        }

        // Check for duplicates
        if (seen[pin]) {
            FL_WARN("SpiHwI2SESP32: Duplicate pin " << pin);
            return false;
        }
        seen[pin] = true;
    }

    return true;
}

uint8_t* SpiHwI2SESP32::allocate_dma_buffer(size_t size) {
    uint8_t* buffer = nullptr;

#if CONFIG_IDF_TARGET_ESP32S3 || defined(ESP32S3)
    // ESP32-S3: EDMA supports PSRAM directly
    buffer = static_cast<uint8_t*>(heap_caps_aligned_alloc(
        4,  // 4-byte alignment for DMA
        size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT
    ));

    if (buffer != nullptr) {
        return buffer;  // PSRAM+DMA allocation succeeded
    }

    FL_WARN("SpiHwI2SESP32: PSRAM+DMA allocation failed, falling back to internal RAM");
#endif

    // Fallback: Try internal DMA RAM (all ESP32 variants)
    buffer = static_cast<uint8_t*>(heap_caps_aligned_alloc(
        4,  // 4-byte alignment for DMA
        size,
        MALLOC_CAP_DMA | MALLOC_CAP_8BIT
    ));

    if (buffer == nullptr) {
        FL_WARN("SpiHwI2SESP32: Internal DMA RAM allocation failed (" << size << " bytes)");
    }

    return buffer;
}

int SpiHwI2SESP32::calculate_clock_mhz(uint32_t target_hz) {
    // Convert Hz to MHz, rounding to nearest integer
    int mhz = static_cast<int>((target_hz + 500000) / 1000000);

    // Clamp to valid range (1-40 MHz)
    if (mhz < 1) mhz = 1;
    if (mhz > 40) mhz = 40;

    return mhz;
}

int SpiHwI2SESP32::count_active_lanes(const Config& config) {
    int count = 0;
    if (config.data0_pin >= 0) ++count;
    if (config.data1_pin >= 0) ++count;
    if (config.data2_pin >= 0) ++count;
    if (config.data3_pin >= 0) ++count;
    if (config.data4_pin >= 0) ++count;
    if (config.data5_pin >= 0) ++count;
    if (config.data6_pin >= 0) ++count;
    if (config.data7_pin >= 0) ++count;
    if (config.data8_pin >= 0) ++count;
    if (config.data9_pin >= 0) ++count;
    if (config.data10_pin >= 0) ++count;
    if (config.data11_pin >= 0) ++count;
    if (config.data12_pin >= 0) ++count;
    if (config.data13_pin >= 0) ++count;
    if (config.data14_pin >= 0) ++count;
    if (config.data15_pin >= 0) ++count;
    return count;
}

fl::vector<int> SpiHwI2SESP32::extract_data_pins(const Config& config) {
    fl::vector<int> pins;
    pins.reserve(16);

    if (config.data0_pin >= 0) pins.push_back(config.data0_pin);
    if (config.data1_pin >= 0) pins.push_back(config.data1_pin);
    if (config.data2_pin >= 0) pins.push_back(config.data2_pin);
    if (config.data3_pin >= 0) pins.push_back(config.data3_pin);
    if (config.data4_pin >= 0) pins.push_back(config.data4_pin);
    if (config.data5_pin >= 0) pins.push_back(config.data5_pin);
    if (config.data6_pin >= 0) pins.push_back(config.data6_pin);
    if (config.data7_pin >= 0) pins.push_back(config.data7_pin);
    if (config.data8_pin >= 0) pins.push_back(config.data8_pin);
    if (config.data9_pin >= 0) pins.push_back(config.data9_pin);
    if (config.data10_pin >= 0) pins.push_back(config.data10_pin);
    if (config.data11_pin >= 0) pins.push_back(config.data11_pin);
    if (config.data12_pin >= 0) pins.push_back(config.data12_pin);
    if (config.data13_pin >= 0) pins.push_back(config.data13_pin);
    if (config.data14_pin >= 0) pins.push_back(config.data14_pin);
    if (config.data15_pin >= 0) pins.push_back(config.data15_pin);

    return pins;
}

// ============================================================================
// Factory Function - Strong Definition Overrides Weak Default
// ============================================================================

// This strong definition overrides the weak default in spi_hw_16.cpp
// It registers the ESP32 I2S controller with the SpiHw16 factory system
fl::vector<SpiHw16*> SpiHw16::createInstances() {
    fl::vector<SpiHw16*> controllers;

    // Single static instance (I2S0 only available on ESP32)
    static SpiHwI2SESP32 i2s0_controller(0);
    controllers.push_back(&i2s0_controller);

    return controllers;
}

} // namespace fl

#endif // defined(ESP32) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) etc.
