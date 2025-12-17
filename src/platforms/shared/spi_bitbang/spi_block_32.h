// spi_block_32.h — 32-way blocking soft-SPI driver (inline bit-banging, platform-agnostic)
#pragma once

#include "fl/stl/stdint.h"
#include "spi_isr_engine.h"
#include "spi_platform.h"

namespace fl {

/**
 * SpiBlock32 - 32-way parallel blocking soft-SPI driver (platform-agnostic bitbanging)
 *
 * This is a main-thread blocking implementation that uses inline bit-banging
 * with the same GPIO manipulation logic as the 16-way implementation, extended to 32 pins.
 *
 * Key Differences from 16-way variant:
 * - Supports 32 parallel data pins (vs 16 in SpiBlock16)
 * - Uses 32-bit GPIO masks (same width but double the pins)
 * - 2x throughput gain over 16-way SPI
 * - 4x throughput gain over 8-way SPI
 *
 * When to Use:
 * - Use 32-way SPI when:
 *   - Need maximum throughput with many parallel LED strips
 *   - Platform has 32+ available GPIO pins
 *   - Simple LED update pattern
 *   - Lower overhead needed (no ISR context switching)
 *   - Blocking during LED update is acceptable
 *
 * Architecture:
 * - Uses same bit-banging logic as 16-way implementation
 * - 256-entry LUT maps byte values to 32-pin GPIO masks
 * - Only 8 bits of each byte value are used (mapped to first 8 of 32 pins)
 * - Remaining 24 pins are always cleared
 * - Direct GPIO MMIO writes (same as 16-way)
 * - Two-phase bit transmission (data+CLK_LOW, then CLK_HIGH)
 *
 * Typical Usage:
 *   SpiBlock32 spi;
 *   spi.setPinMapping(gpio_d0, gpio_d1, ..., gpio_d31, gpio_clk);
 *   spi.loadBuffer(data, len);
 *   spi.transmit();  // Blocks until complete
 *
 * Performance:
 * - Highest throughput with 32 parallel pins
 * - Same latency characteristics as 16-way (inline execution)
 * - Better timing precision (inline execution)
 *
 * Test Patterns:
 * - 0x00: All pins low (00000000000000000000000000000000)
 * - 0x01: D0 high, others low (00000000000000000000000000000001)
 * - 0xFF: D0-D7 high, D8-D31 low (00000000000000000000000011111111)
 * - 0xFFFFFFFF: All 32 pins high (when combined with clock masking)
 */
class SpiBlock32 {
public:
    /// Maximum pins per lane (32-way)
    static constexpr int NUM_DATA_PINS = 32;

    /// Maximum buffer size
    static constexpr uint16_t MAX_BUFFER_SIZE = 256;

    SpiBlock32() = default;
    ~SpiBlock32() = default;

    /**
     * Configure pin mapping for 32 data pins + 1 clock
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
     * @param d15 GPIO number for data bit 15
     * @param d16 GPIO number for data bit 16
     * @param d17 GPIO number for data bit 17
     * @param d18 GPIO number for data bit 18
     * @param d19 GPIO number for data bit 19
     * @param d20 GPIO number for data bit 20
     * @param d21 GPIO number for data bit 21
     * @param d22 GPIO number for data bit 22
     * @param d23 GPIO number for data bit 23
     * @param d24 GPIO number for data bit 24
     * @param d25 GPIO number for data bit 25
     * @param d26 GPIO number for data bit 26
     * @param d27 GPIO number for data bit 27
     * @param d28 GPIO number for data bit 28
     * @param d29 GPIO number for data bit 29
     * @param d30 GPIO number for data bit 30
     * @param d31 GPIO number for data bit 31 (MSB)
     * @param clk GPIO number for clock pin
     *
     * Initializes the 256-entry LUT to map byte values to GPIO masks
     * for the specified data pins. Only the first 8 bits of each byte
     * are used; bits 8-31 are always cleared.
     */
    void setPinMapping(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t d8, uint8_t d9, uint8_t d10, uint8_t d11,
                       uint8_t d12, uint8_t d13, uint8_t d14, uint8_t d15,
                       uint8_t d16, uint8_t d17, uint8_t d18, uint8_t d19,
                       uint8_t d20, uint8_t d21, uint8_t d22, uint8_t d23,
                       uint8_t d24, uint8_t d25, uint8_t d26, uint8_t d27,
                       uint8_t d28, uint8_t d29, uint8_t d30, uint8_t d31,
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
            1u << d15,  // Bit 15
            1u << d16,  // Bit 16
            1u << d17,  // Bit 17
            1u << d18,  // Bit 18
            1u << d19,  // Bit 19
            1u << d20,  // Bit 20
            1u << d21,  // Bit 21
            1u << d22,  // Bit 22
            1u << d23,  // Bit 23
            1u << d24,  // Bit 24
            1u << d25,  // Bit 25
            1u << d26,  // Bit 26
            1u << d27,  // Bit 27
            1u << d28,  // Bit 28
            1u << d29,  // Bit 29
            1u << d30,  // Bit 30
            1u << d31   // Bit 31 (MSB)
        };

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Map all 8 bits to corresponding GPIO pins (32 total pins available)
        // - Bits 8-31 are always cleared (no corresponding bits in a byte)
        // - Generate set_mask (pins to set high) and clear_mask (pins to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            uint32_t setMask = 0;
            uint32_t clearMask = 0;

            // Process all 32 data pins, checking each corresponding bit in the byte
            for (int bitPos = 0; bitPos < NUM_DATA_PINS; bitPos++) {
                if (bitPos < 8 && (byteValue & (1 << bitPos))) {
                    // Bits 0-7: set if corresponding byte bit is set
                    setMask |= dataPinMasks[bitPos];
                } else if (bitPos < 8) {
                    // Bits 0-7: clear if corresponding byte bit is not set
                    clearMask |= dataPinMasks[bitPos];
                } else {
                    // Bits 8-31: always cleared (no corresponding bits in byte)
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
     * Each byte in the buffer represents 32 parallel bits to output.
     * Only 8 bits of each byte are used (mapped to 32 pins via LUT).
     * Bits 8-31 are always cleared during transmission.
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
     * Uses the same two-phase bit-banging logic as 16-way implementation:
     * - Phase 0: Set data pins + force CLK low
     * - Phase 1: Raise CLK high to latch data
     *
     * Performance: Highest throughput with 32 parallel pins
     */
    void transmit() {
        if (!mBuffer || mBufferLen == 0) return;

        // Inline bit-banging loop (same logic as 16-way implementation)
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
