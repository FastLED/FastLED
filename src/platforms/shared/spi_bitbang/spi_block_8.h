// spi_block_8.h — 8-way Octal-SPI Blocking driver (inline bit-banging, platform-agnostic)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"
#include "spi_platform.h"

namespace fl {

/**
 * SpiBlock8 - 8-way (octal-pin) blocking soft-SPI driver (platform-agnostic bitbanging)
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
 * - 256-entry LUT maps byte values to 8-pin GPIO masks
 * - All 8 bits of each byte value are used
 * - Direct GPIO MMIO writes (same as ISR)
 * - Two-phase bit transmission (data+CLK_LOW, then CLK_HIGH)
 *
 * Typical Usage:
 *   SpiBlock8 spi;
 *   spi.setPinMapping(gpio_d0, gpio_d1, gpio_d2, gpio_d3,
 *                     gpio_d4, gpio_d5, gpio_d6, gpio_d7, gpio_clk);
 *   spi.loadBuffer(data, len);
 *   spi.transmit();  // Blocks until complete
 *
 * Performance:
 * - Higher throughput than ISR due to no interrupt overhead
 * - Lower latency (no interrupt context switch)
 * - Better timing precision (inline execution)
 *
 * Test Patterns:
 * - 0x00: All pins low (00000000)
 * - 0x01: D0 high, others low (00000001)
 * - 0x02: D1 high, others low (00000010)
 * - 0x03: D0+D1 high (00000011)
 * - 0x0F: D0-D3 high (00001111)
 * - 0x55: D0+D2+D4+D6 high (01010101)
 * - 0xAA: D1+D3+D5+D7 high (10101010)
 * - 0xF0: D4-D7 high (11110000)
 * - 0xFF: All pins high (11111111)
 */
class SpiBlock8 {
public:
    /// Maximum pins per lane (octal = 8)
    static constexpr int NUM_DATA_PINS = 8;

    /// Maximum buffer size
    static constexpr uint16_t MAX_BUFFER_SIZE = 256;

    SpiBlock8() = default;
    ~SpiBlock8() = default;

    /**
     * Configure pin mapping for 8 data pins + 1 clock
     * @param d0 GPIO number for data bit 0 (LSB)
     * @param d1 GPIO number for data bit 1
     * @param d2 GPIO number for data bit 2
     * @param d3 GPIO number for data bit 3
     * @param d4 GPIO number for data bit 4
     * @param d5 GPIO number for data bit 5
     * @param d6 GPIO number for data bit 6
     * @param d7 GPIO number for data bit 7 (MSB)
     * @param clk GPIO number for clock pin
     *
     * Initializes the 256-entry LUT to map byte values to GPIO masks
     * for the specified data pins.
     */
    void setPinMapping(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t clk) {
        // Store clock mask
        mClockMask = 1u << clk;

        // Build data pin masks array
        uint32_t dataPinMasks[NUM_DATA_PINS] = {
            1u << d0,  // Bit 0 (LSB)
            1u << d1,  // Bit 1
            1u << d2,  // Bit 2
            1u << d3,  // Bit 3
            1u << d4,  // Bit 4
            1u << d5,  // Bit 5
            1u << d6,  // Bit 6
            1u << d7   // Bit 7 (MSB)
        };

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Map all 8 bits to corresponding GPIO pins
        // - Generate set_mask (pins to set high) and clear_mask (pins to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            uint32_t setMask = 0;
            uint32_t clearMask = 0;

            // Process all 8 bits
            for (int bitPos = 0; bitPos < NUM_DATA_PINS; bitPos++) {
                if (byteValue & (1 << bitPos)) {
                    setMask |= dataPinMasks[bitPos];
                } else {
                    clearMask |= dataPinMasks[bitPos];
                }
            }

            mLUT[byteValue].set_mask = setMask;
            mLUT[byteValue].clear_mask = clearMask;
        }
    }

    /**
     * Load data buffer for transmission
     * @param data Pointer to data bytes
     * @param n Number of bytes (max 256)
     *
     * Each byte in the buffer represents 8 parallel bits to output.
     * All 8 bits of each byte are used.
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

// C++11 requires out-of-class definitions for static constexpr members that are ODR-used
// These definitions are in src/fl/static_constexpr_defs.cpp to avoid duplicate symbols

}  // namespace fl
