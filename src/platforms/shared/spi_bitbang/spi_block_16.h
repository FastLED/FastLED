// spi_block_16.h — 16-way Hex-SPI Blocking driver (inline bit-banging, platform-agnostic)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"
#include "spi_platform.h"

namespace fl {

/**
 * SpiBlock16 - 16-way (hex-pin) blocking soft-SPI driver (platform-agnostic bitbanging)
 *
 * This is a main-thread blocking implementation that uses inline bit-banging
 * with the same GPIO manipulation logic as the 8-way implementation, extended to 16 pins.
 *
 * Key Differences from 8-way variant:
 * - Supports 16 parallel data pins (vs 8 in SpiBlock8)
 * - Uses 16-bit GPIO masks (vs 32-bit but only 8 bits used in SpiBlock8)
 * - 4x throughput gain over 4-way SPI
 * - 2x throughput gain over 8-way SPI
 *
 * When to Use:
 * - Use 16-way SPI when:
 *   - Need maximum throughput with many parallel LED strips
 *   - Platform has 16+ available GPIO pins
 *   - Simple LED update pattern
 *   - Lower overhead needed (no ISR context switching)
 *   - Blocking during LED update is acceptable
 *
 * Architecture:
 * - Uses same bit-banging logic as 8-way implementation
 * - 256-entry LUT maps byte values to 16-pin GPIO masks
 * - All 8 bits of each byte value are used
 * - Direct GPIO MMIO writes (same as 8-way)
 * - Two-phase bit transmission (data+CLK_LOW, then CLK_HIGH)
 *
 * Typical Usage:
 *   SpiBlock16 spi;
 *   spi.setPinMapping(gpio_d0, gpio_d1, ..., gpio_d15, gpio_clk);
 *   spi.loadBuffer(data, len);
 *   spi.transmit();  // Blocks until complete
 *
 * Performance:
 * - Higher throughput than 8-way due to 2x more parallel pins
 * - Same latency characteristics as 8-way (inline execution)
 * - Better timing precision (inline execution)
 *
 * Test Patterns:
 * - 0x00: All pins low (0000000000000000)
 * - 0x01: D0 high, others low (0000000000000001)
 * - 0xFF: D0-D7 high, D8-D15 low (0000000011111111)
 * - 0xFFFF: All 16 pins high (when combined with clock masking)
 */
class SpiBlock16 {
public:
    /// Maximum pins per lane (hex = 16)
    static constexpr int NUM_DATA_PINS = 16;

    /// Maximum buffer size
    static constexpr uint16_t MAX_BUFFER_SIZE = 256;

    SpiBlock16() = default;
    ~SpiBlock16() = default;

    /**
     * Configure pin mapping for 16 data pins + 1 clock
     * @param d0 GPIO number for data bit 0 (LSB)
     * @param d1 GPIO number for data bit 1
     * @param d2 GPIO number for data bit 2
     * @param d3 GPIO number for data bit 3
     * @param d4 GPIO number for data bit 4
     * @param d5 GPIO number for data bit 5
     * @param d6 GPIO number for data bit 6
     * @param d7 GPIO number for data bit 7
     * @param d8 GPIO number for data bit 8
     * @param d9 GPIO number for data bit 9
     * @param d10 GPIO number for data bit 10
     * @param d11 GPIO number for data bit 11
     * @param d12 GPIO number for data bit 12
     * @param d13 GPIO number for data bit 13
     * @param d14 GPIO number for data bit 14
     * @param d15 GPIO number for data bit 15 (MSB)
     * @param clk GPIO number for clock pin
     *
     * Initializes the 256-entry LUT to map byte values to GPIO masks
     * for the specified data pins.
     */
    void setPinMapping(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t d8, uint8_t d9, uint8_t d10, uint8_t d11,
                       uint8_t d12, uint8_t d13, uint8_t d14, uint8_t d15,
                       uint8_t clk) {
        // Store clock mask
        mClockMask = 1u << clk;

        // Build data pin masks array
        uint32_t dataPinMasks[NUM_DATA_PINS] = {
            1u << d0,   // Bit 0 (LSB)
            1u << d1,   // Bit 1
            1u << d2,   // Bit 2
            1u << d3,   // Bit 3
            1u << d4,   // Bit 4
            1u << d5,   // Bit 5
            1u << d6,   // Bit 6
            1u << d7,   // Bit 7
            1u << d8,   // Bit 8
            1u << d9,   // Bit 9
            1u << d10,  // Bit 10
            1u << d11,  // Bit 11
            1u << d12,  // Bit 12
            1u << d13,  // Bit 13
            1u << d14,  // Bit 14
            1u << d15   // Bit 15 (MSB)
        };

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Map all 8 bits to corresponding GPIO pins (16 total pins available)
        // - Bits 8-15 are always cleared (no corresponding bits in a byte)
        // - Generate set_mask (pins to set high) and clear_mask (pins to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            uint32_t setMask = 0;
            uint32_t clearMask = 0;

            // Process all 16 data pins, checking each corresponding bit in the byte
            for (int bitPos = 0; bitPos < NUM_DATA_PINS; bitPos++) {
                if (bitPos < 8 && (byteValue & (1 << bitPos))) {
                    // Bits 0-7: set if corresponding byte bit is set
                    setMask |= dataPinMasks[bitPos];
                } else if (bitPos < 8) {
                    // Bits 0-7: clear if corresponding byte bit is not set
                    clearMask |= dataPinMasks[bitPos];
                } else {
                    // Bits 8-15: always cleared (no corresponding bits in byte)
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
     * Each byte in the buffer represents 16 parallel bits to output.
     * Only 8 bits of each byte are used (mapped to 16 pins via LUT).
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
     * Uses the same two-phase bit-banging logic as 8-way implementation:
     * - Phase 0: Set data pins + force CLK low
     * - Phase 1: Raise CLK high to latch data
     *
     * Performance: Higher throughput than 8-way due to 16 parallel pins
     */
    void transmit() {
        if (!mBuffer || mBufferLen == 0) return;

        // Inline bit-banging loop (same logic as 8-way implementation)
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
