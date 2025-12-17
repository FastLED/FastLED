// spi_block_4.h — 4-way Quad-SPI Blocking driver (inline bit-banging, platform-agnostic)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"
#include "spi_platform.h"

namespace fl {

/**
 * SpiBlock4 - 4-way (quad-pin) blocking soft-SPI driver (platform-agnostic bitbanging)
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
 * - 256-entry LUT maps byte values to 4-pin GPIO masks
 * - Only uses lower 4 bits of byte value (upper 4 bits ignored)
 * - Direct GPIO MMIO writes (same as ISR)
 * - Two-phase bit transmission (data+CLK_LOW, then CLK_HIGH)
 *
 * Typical Usage:
 *   SpiBlock4 spi;
 *   spi.setPinMapping(gpio_d0, gpio_d1, gpio_d2, gpio_d3, gpio_clk);
 *   spi.loadBuffer(data, len);
 *   spi.transmit();  // Blocks until complete
 *
 * Performance:
 * - Higher throughput than ISR due to no interrupt overhead
 * - Lower latency (no interrupt context switch)
 * - Better timing precision (inline execution)
 *
 * Test Patterns:
 * - 0x00: All pins low (0000)
 * - 0x01: D0 high, others low (0001)
 * - 0x02: D1 high, others low (0010)
 * - 0x03: D0+D1 high, D2+D3 low (0011)
 * - 0x04: D2 high, others low (0100)
 * - 0x05: D0+D2 high, D1+D3 low (0101)
 * - 0x06: D1+D2 high, D0+D3 low (0110)
 * - 0x07: D0+D1+D2 high, D3 low (0111)
 * - 0x08: D3 high, others low (1000)
 * - 0x09: D0+D3 high, D1+D2 low (1001)
 * - 0x0A: D1+D3 high, D0+D2 low (1010)
 * - 0x0B: D0+D1+D3 high, D2 low (1011)
 * - 0x0C: D2+D3 high, D0+D1 low (1100)
 * - 0x0D: D0+D2+D3 high, D1 low (1101)
 * - 0x0E: D1+D2+D3 high, D0 low (1110)
 * - 0x0F: All pins high (1111)
 */
class SpiBlock4 {
public:
    /// Maximum pins per lane (quad = 4)
    static constexpr int NUM_DATA_PINS = 4;

    /// Maximum buffer size
    static constexpr uint16_t MAX_BUFFER_SIZE = 256;

    SpiBlock4() = default;
    ~SpiBlock4() = default;

    /**
     * Configure pin mapping for 4 data pins + 1 clock
     * @param d0 GPIO number for data bit 0 (LSB)
     * @param d1 GPIO number for data bit 1
     * @param d2 GPIO number for data bit 2
     * @param d3 GPIO number for data bit 3 (MSB)
     * @param clk GPIO number for clock pin
     *
     * Initializes the 256-entry LUT to map byte values to GPIO masks
     * for the specified data pins.
     */
    void setPinMapping(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t clk) {
        // Store clock mask
        mClockMask = 1u << clk;

        // Build data pin masks array
        uint32_t dataPinMasks[NUM_DATA_PINS] = {
            1u << d0,  // Bit 0 (LSB)
            1u << d1,  // Bit 1
            1u << d2,  // Bit 2
            1u << d3   // Bit 3 (MSB)
        };

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Extract lower 4 bits
        // - Map each bit to corresponding GPIO pin
        // - Generate set_mask (pins to set high) and clear_mask (pins to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            uint32_t setMask = 0;
            uint32_t clearMask = 0;

            // Only process lower 4 bits (upper 4 bits ignored)
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
     * Each byte in the buffer represents 4 parallel bits to output.
     * Only the lower 4 bits of each byte are used.
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
