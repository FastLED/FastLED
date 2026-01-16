// spi_isr_1.h — 1-way Single-SPI ISR wrapper (platform-agnostic bitbanging)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"

namespace fl {

/**
 * SpiIsr1 - 1-way (single-pin) soft-SPI ISR driver (platform-agnostic bitbanging)
 *
 * This is the simplest variant of the parallel SPI ISR driver, using only
 * 1 data pin + 1 clock pin. Ideal for baseline testing and validation of the
 * ISR engine.
 *
 * Key Differences from multi-way variants:
 * - Only 1 data pin (instead of 2, 4, or 8)
 * - Simplest LUT initialization (only 2 unique states: 0 or 1)
 * - Perfect for debugging and understanding ISR behavior
 * - Can be used for actual single-strip LED control
 * - Lowest GPIO requirements (just 2 pins total)
 *
 * Architecture:
 * - Reuses the same ISR code (fl_parallel_spi_isr_rv.h/cpp)
 * - 256-entry LUT maps byte values to 1-pin GPIO masks
 * - Only uses bit 0 of byte value (upper 7 bits ignored)
 * - ISR operates at highest priority for minimal jitter
 *
 * Typical Usage:
 *   SpiIsr1 spi;
 *   spi.setPinMapping(gpio_data, gpio_clk);
 *   spi.setupISR(1600000);  // 1.6MHz timer = 800kHz SPI
 *   spi.loadBuffer(data, len);
 *   spi.arm();
 *   while(spi.isBusy()) { }
 *   spi.stopISR();
 *
 * Test Patterns:
 * - 0x00: Data pin low (0)
 * - 0x01: Data pin high (1)
 * - 0xAA: Alternating 0/1 pattern (tests multiple bytes)
 * - 0xFF: All ones
 */
class SpiIsr1 {
public:
    /// Status bit definitions
    static constexpr uint32_t STATUS_BUSY = 1u;
    static constexpr uint32_t STATUS_DONE = 2u;

    /// Maximum pins per lane (single = 1)
    static constexpr int NUM_DATA_PINS = 1;

    SpiIsr1() = default;
    ~SpiIsr1() = default;

    /**
     * Configure pin mapping for 1 data pin + 1 clock
     * @param data GPIO number for data pin
     * @param clk GPIO number for clock pin
     *
     * Automatically initializes the 256-entry LUT to map byte values
     * to GPIO mask for the specified data pin.
     */
    void setPinMapping(uint8_t data, uint8_t clk) {
        // Store clock mask
        fl_spi_set_clock_mask(1u << clk);

        // Build pin mask
        uint32_t dataPinMask = 1u << data;

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Extract bit 0
        // - Map to GPIO pin
        // - Generate set_mask (pin to set high) or clear_mask (pin to clear low)
        PinMaskEntry* lut = fl_spi_get_lut_array();

        for (int byteValue = 0; byteValue < 256; byteValue++) {
            // Only process bit 0 (upper 7 bits ignored)
            if (byteValue & 1) {
                // Bit 0 is set - set data pin high
                lut[byteValue].set_mask = dataPinMask;
                lut[byteValue].clear_mask = 0;
            } else {
                // Bit 0 is clear - clear data pin low
                lut[byteValue].set_mask = 0;
                lut[byteValue].clear_mask = dataPinMask;
            }
        }
    }

    /**
     * Alternative: Configure pin mapping using clock mask directly
     * @param data GPIO number for data pin
     * @param clockMask Pre-computed GPIO mask for clock pin (e.g., 1 << 8)
     */
    void setPinMappingWithMask(uint8_t data, uint32_t clockMask) {
        fl_spi_set_clock_mask(clockMask);

        uint32_t dataPinMask = 1u << data;

        PinMaskEntry* lut = fl_spi_get_lut_array();
        for (int v = 0; v < 256; v++) {
            if (v & 1) {
                lut[v].set_mask = dataPinMask;
                lut[v].clear_mask = 0;
            } else {
                lut[v].set_mask = 0;
                lut[v].clear_mask = dataPinMask;
            }
        }
    }

    /**
     * Bulk load data buffer
     * @param data Pointer to data bytes
     * @param n Number of bytes (max 256)
     *
     * Each byte in the buffer represents 1 bit to output on the data pin.
     * Only bit 0 of each byte is used.
     */
    void loadBuffer(const uint8_t* data, uint16_t n) {
        if (!data) return;
        if (n > 256) n = 256;

        uint8_t* dest = fl_spi_get_data_array();
        for (uint16_t i = 0; i < n; ++i) {
            dest[i] = data[i];
        }
        fl_spi_set_total_bytes(n);
    }

    /**
     * Setup ISR and timer
     * @param timer_hz Timer frequency in Hz (should be 2× target SPI bit rate)
     * @return 0 on success, error code on failure
     *
     * Example: For 800kHz SPI output, use 1600000 Hz timer
     */
    int setupISR(uint32_t timer_hz) {
        return fl_spi_platform_isr_start(timer_hz);
    }

    /**
     * Stop ISR and timer
     */
    void stopISR() {
        fl_spi_platform_isr_stop();
    }

    /**
     * Arm a transfer (caller must ensure visibility delay first)
     * Increments doorbell counter to trigger ISR edge detection
     */
    void arm() {
        fl_spi_arm();
    }

    /**
     * Check if ISR is currently transmitting
     */
    bool isBusy() const {
        return (fl_spi_status_flags() & STATUS_BUSY) != 0;
    }

    /**
     * Get raw status flags
     */
    uint32_t statusFlags() const {
        return fl_spi_status_flags();
    }

    /**
     * Acknowledge DONE flag (clears it)
     */
    void ackDone() {
        fl_spi_ack_done();
    }

    /**
     * Visibility delay (ensures memory writes are visible to ISR)
     * Typical value: 10 microseconds
     */
    static void visibilityDelayUs(uint32_t us) {
        fl_spi_visibility_delay_us(us);
    }

    /**
     * Reset ISR state (between runs)
     */
    static void resetState() {
        fl_spi_reset_state();
    }

    /**
     * Get mutable reference to LUT array (256 entries)
     * For advanced users who want direct LUT control
     */
    static PinMaskEntry* getLUTArray() {
        return fl_spi_get_lut_array();
    }

    /**
     * Get mutable reference to data buffer array (256 bytes)
     * For advanced users who want direct buffer access
     */
    static uint8_t* getDataArray() {
        return fl_spi_get_data_array();
    }

#ifdef FL_SPI_ISR_VALIDATE
    /**
     * Get GPIO event log (only available when FL_SPI_ISR_VALIDATE is defined)
     * Returns pointer to array of GPIO events captured during ISR execution
     */
    static const FastLED_GPIO_Event* getValidationEvents() {
        return fl_spi_get_validation_events();
    }

    /**
     * Get number of GPIO events captured
     */
    static uint16_t getValidationEventCount() {
        return fl_spi_get_validation_event_count();
    }

    // C++ wrapper types for GPIO events
    enum class GPIOEventType : uint8_t {
        StateStart  = FASTLED_GPIO_EVENT_STATE_START,
        StateDone   = FASTLED_GPIO_EVENT_STATE_DONE,
        SetBits     = FASTLED_GPIO_EVENT_SET_BITS,
        ClearBits   = FASTLED_GPIO_EVENT_CLEAR_BITS,
        ClockLow    = FASTLED_GPIO_EVENT_CLOCK_LOW,
        ClockHigh   = FASTLED_GPIO_EVENT_CLOCK_HIGH
    };

    struct GPIOEvent {
        uint8_t  event_type;
        uint8_t  padding[3];
        union {
            uint32_t gpio_mask;
            uint32_t state_info;
        } payload;

        GPIOEventType type() const { return static_cast<GPIOEventType>(event_type); }
    };

    static const GPIOEvent* getValidationEventsTyped() {
        return reinterpret_cast<const GPIOEvent*>(fl_spi_get_validation_events()); // ok reinterpret cast
    }
#endif
};

}  // namespace fl
