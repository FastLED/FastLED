// spi_block_1.h — 1-way Single-SPI Blocking driver (inline bit-banging, platform-agnostic)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"
#include "spi_platform.h"

namespace fl {

/**
 * SpiBlock1 - 1-way (single-pin) blocking soft-SPI driver (platform-agnostic bitbanging)
 *
 * This is a main-thread blocking implementation that uses inline bit-banging
 * with the same GPIO manipulation logic as the ISR-based implementation.
 *
 * Key Differences from ISR variant:
 * - Runs inline on main thread (no ISR context switching)
 * - Simple blocking API (transmit() blocks until complete)
 * - Lower overhead (no interrupt latency or jitter)
 * - Better timing precision (inline execution)
 * - Higher throughput (no interrupt scheduling delays)
 * - Simpler code (no async complexity)
 *
 * When to Use:
 * - Use Blocking SPI when:
 *   - Simple LED update pattern
 *   - Lower overhead needed (no ISR context switching)
 *   - Blocking during LED update is acceptable
 *   - More predictable timing required (no interrupt jitter)
 *   - Lower code complexity preferred
 *
 * - Use ISR SPI when:
 *   - Non-blocking LED updates needed
 *   - Main thread must remain responsive during LED updates
 *   - Complex application with multiple tasks
 *
 * Architecture:
 * - Uses same bit-banging logic as ISR implementation
 * - 256-entry LUT maps byte values to 1-pin GPIO masks
 * - Only uses bit 0 of byte value (upper 7 bits ignored)
 * - Direct GPIO MMIO writes (same as ISR)
 * - Two-phase bit transmission (data+CLK_LOW, then CLK_HIGH)
 *
 * Typical Usage:
 *   SpiBlock1 spi;
 *   spi.setPinMapping(gpio_data, gpio_clk);
 *   spi.loadBuffer(data, len);
 *   spi.transmit();  // Blocks until complete
 *
 * Performance:
 * - Higher throughput than ISR due to no interrupt overhead
 * - Lower latency (no interrupt context switch)
 * - Better timing precision (inline execution)
 */
class SpiBlock1 {
public:
    /// Maximum pins per lane (single = 1)
    static constexpr int NUM_DATA_PINS = 1;

    /// Maximum buffer size
    static constexpr uint16_t MAX_BUFFER_SIZE = 256;

    SpiBlock1() = default;
    ~SpiBlock1() = default;

    /**
     * Configure pin mapping for 1 data pin + 1 clock
     * @param data GPIO number for data pin
     * @param clk GPIO number for clock pin
     *
     * Initializes the 256-entry LUT to map byte values to GPIO masks
     * for the specified data pin.
     */
    void setPinMapping(uint8_t data, uint8_t clk) {
        // Store clock mask
        mClockMask = 1u << clk;

        // Build data pin mask
        uint32_t dataPinMask = 1u << data;

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Extract bit 0
        // - Map to GPIO pin
        // - Generate set_mask (pin to set high) or clear_mask (pin to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            // Only process bit 0 (upper 7 bits ignored)
            if (byteValue & 1) {
                // Bit 0 is set - set data pin high
                mLUT[byteValue].set_mask = dataPinMask;
                mLUT[byteValue].clear_mask = 0;
            } else {
                // Bit 0 is clear - clear data pin low
                mLUT[byteValue].set_mask = 0;
                mLUT[byteValue].clear_mask = dataPinMask;
            }
        }
    }

    /**
     * Load data buffer for transmission
     * @param data Pointer to data bytes
     * @param n Number of bytes (max 256)
     *
     * Each byte in the buffer represents 1 bit to output on the data pin.
     * Only bit 0 of each byte is used.
     */
    void loadBuffer(const uint8_t* data, uint16_t n) {
        if (!data) return;
        if (n > MAX_BUFFER_SIZE) n = MAX_BUFFER_SIZE;

        mBuffer = data;
        mBufferLen = n;
    }

    /**
     * Transmit data buffer using inline bit-banging
     *
     * This method blocks until transmission is complete.
     * Uses the same two-phase bit-banging logic as ISR implementation:
     * - Phase 0: Set data pins + force CLK low
     * - Phase 1: Raise CLK high to latch data
     *
     * Performance: Higher throughput than ISR due to no interrupt overhead
     */
    void transmit() {
        if (!mBuffer || mBufferLen == 0) return;

        // Inline bit-banging loop (same logic as ISR implementation)
        for (uint16_t i = 0; i < mBufferLen; i++) {
            uint8_t byte = mBuffer[i];

            // Phase 0: Present data + force CLK low
            uint32_t pins_to_set = mLUT[byte].set_mask;
            uint32_t pins_to_clear = mLUT[byte].clear_mask | mClockMask;

            FL_GPIO_WRITE_SET(pins_to_set);      // data-high bits
            FL_GPIO_WRITE_CLEAR(pins_to_clear);  // data-low bits + CLK low

            // Phase 1: Raise CLK high to latch data
            FL_GPIO_WRITE_SET(mClockMask);
        }
    }

    /**
     * Get buffer pointer (for inspection)
     */
    const uint8_t* getBuffer() const {
        return mBuffer;
    }

    /**
     * Get buffer length (for inspection)
     */
    uint16_t getBufferLength() const {
        return mBufferLen;
    }

    /**
     * Get LUT array (for advanced users who want direct LUT control)
     */
    PinMaskEntry* getLUTArray() {
        return mLUT;
    }

private:
    uint32_t mClockMask = 0;           ///< Clock pin mask
    PinMaskEntry mLUT[256];            ///< 256-entry lookup table
    const uint8_t* mBuffer = nullptr;  ///< Data buffer pointer
    uint16_t mBufferLen = 0;           ///< Buffer length
};

}  // namespace fl
