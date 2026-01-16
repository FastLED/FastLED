// spi_isr_16.h — 16-way parallel soft-SPI ISR wrapper (platform-agnostic bitbanging)
#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/bit_cast.h"
#include "spi_isr_engine.h"

#ifdef FL_SPI_ISR_VALIDATE
// C++ wrapper types for GPIO events (provides type safety and convenience methods)
enum class GPIOEventType : uint8_t {
    StateStart  = FASTLED_GPIO_EVENT_STATE_START,
    StateDone   = FASTLED_GPIO_EVENT_STATE_DONE,
    SetBits     = FASTLED_GPIO_EVENT_SET_BITS,
    ClearBits   = FASTLED_GPIO_EVENT_CLEAR_BITS,
    ClockLow    = FASTLED_GPIO_EVENT_CLOCK_LOW,
    ClockHigh   = FASTLED_GPIO_EVENT_CLOCK_HIGH
};

// C++ wrapper for FastLED_GPIO_Event (same memory layout, adds convenience methods)
struct GPIOEvent {
    uint8_t  event_type;
    uint8_t  padding[3];
    union {
        uint32_t gpio_mask;
        uint32_t state_info;
    } payload;

    GPIOEventType type() const { return static_cast<GPIOEventType>(event_type); }
};

// Verify memory layout matches between C and C++ structures
// Use FL_OFFSETOF macro (defined in fl/cstddef.h) instead of <stddef.h>
static_assert(sizeof(GPIOEvent) == sizeof(FastLED_GPIO_Event),
              "C++ GPIOEvent must match C FastLED_GPIO_Event layout");
static_assert(FL_OFFSETOF(GPIOEvent, event_type) == FL_OFFSETOF(FastLED_GPIO_Event, event_type),
              "event_type offset must match");
static_assert(FL_OFFSETOF(GPIOEvent, payload) == FL_OFFSETOF(FastLED_GPIO_Event, payload),
              "payload offset must match");
#endif

namespace fl {

/**
 * SpiIsr16 - High-priority 16-way parallel soft-SPI ISR driver (platform-agnostic bitbanging)
 *
 * This class provides a zero-volatile-read ISR-based parallel SPI implementation.
 * It can operate at the highest available interrupt priority level for minimal jitter.
 *
 * Features:
 * - 16-bit parallel data output + 1 clock pin
 * - ISR performs only MMIO writes (no volatile reads)
 * - Edge-triggered doorbell for producer/consumer synchronization
 * - Two-phase bit engine (data+CLK low, then CLK high)
 * - Platform-agnostic via abstraction layer
 *
 * Usage:
 *   SpiIsr16 spi;
 *   spi.setClockMask(1 << 8);  // Clock on GPIO8
 *   spi.loadLUT(setMasks, clearMasks);  // Pin mapping
 *   spi.setupISR(1600000);  // 1.6MHz timer (800kHz SPI)
 *   spi.loadBuffer(data, len);
 *   spi.arm();  // Start transfer
 *   while(spi.isBusy()) { }
 *   spi.stopISR();
 */
class SpiIsr16 {
public:
    /// Status bit definitions
    static constexpr uint32_t STATUS_BUSY = 1u;
    static constexpr uint32_t STATUS_DONE = 2u;

    SpiIsr16() = default;
    ~SpiIsr16() = default;

    /**
     * Configure GPIO clock mask (single bit for clock pin)
     * Example: setClockMask(1 << 8) for GPIO8
     */
    void setClockMask(uint32_t mask) {
        fl_spi_set_clock_mask(mask);
    }

    /**
     * Set number of bytes to transmit in next burst
     * Max: 256 bytes
     */
    void setTotalBytes(uint16_t n) {
        fl_spi_set_total_bytes(n);
    }

    /**
     * Set a single data byte at index i
     */
    void setDataByte(uint16_t i, uint8_t v) {
        fl_spi_set_data_byte(i, v);
    }

    /**
     * Set lookup table entry for a byte value
     * @param value Byte value (0-255)
     * @param setMask GPIO bits to set high
     * @param clearMask GPIO bits to set low
     */
    void setLUTEntry(uint8_t value, uint32_t setMask, uint32_t clearMask) {
        fl_spi_set_lut_entry(value, setMask, clearMask);
    }

    /**
     * Bulk load data buffer
     * @param data Pointer to data bytes
     * @param n Number of bytes (max 256)
     */
    void loadBuffer(const uint8_t* data, uint16_t n) {
        if (!data) return;
        if (n > 256) n = 256;
        for (uint16_t i = 0; i < n; ++i) {
            fl_spi_set_data_byte(i, data[i]);
        }
        fl_spi_set_total_bytes(n);
    }

    /**
     * Bulk load pin lookup table
     * @param setMasks Array of 256 set masks
     * @param clearMasks Array of 256 clear masks
     * @param count Number of entries to load (default 256)
     */
    void loadLUT(const uint32_t* setMasks, const uint32_t* clearMasks, size_t count = 256) {
        if (!setMasks || !clearMasks) return;
        if (count > 256) count = 256;
        for (size_t v = 0; v < count; ++v) {
            fl_spi_set_lut_entry(static_cast<uint8_t>(v), setMasks[v], clearMasks[v]);
        }
    }

    /**
     * Setup ISR and timer
     * @param timer_hz Timer frequency in Hz (should be 2× target SPI bit rate)
     * @return 0 on success, error code on failure
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
     * Allows direct initialization without individual setLUTEntry calls
     *
     * Example:
     *   PinMaskEntry* lut = spi.getLUTArray();
     *   for (int v = 0; v < 256; v++) {
     *       lut[v].set_mask = ...;
     *       lut[v].clear_mask = ...;
     *   }
     */
    static PinMaskEntry* getLUTArray() {
        return fl_spi_get_lut_array();
    }

    /**
     * Get mutable reference to data buffer array (256 bytes)
     * Allows direct buffer access without individual setDataByte calls
     *
     * Example:
     *   uint8_t* data = spi.getDataArray();
     *   memcpy(data, source, length);
     */
    static uint8_t* getDataArray() {
        return fl_spi_get_data_array();
    }

#ifdef FL_SPI_ISR_VALIDATE
    /**
     * Get GPIO event log (only available when FASTLED_SPI_VALIDATE is defined)
     * Returns pointer to array of GPIO events captured during ISR execution
     */
    static const GPIOEvent* getValidationEvents() {
        return fl::bit_cast<const GPIOEvent*>(fl_spi_get_validation_events());
    }

    /**
     * Get number of GPIO events captured
     */
    static uint16_t getValidationEventCount() {
        return fl_spi_get_validation_event_count();
    }
#endif
};

}  // namespace fl
