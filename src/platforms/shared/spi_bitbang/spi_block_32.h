// spi_block_32.h — 32-way blocking soft-SPI driver (inline bit-banging, platform-agnostic)
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "platforms/shared/spi_bitbang/spi_isr_engine.h"
#include "platforms/shared/spi_bitbang/spi_platform.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/compiler_control.h"
#include "platforms/shared/spi_bitbang/spi_pin_mapping.h"
#include "fl/stl/noexcept.h"

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
 *   if (!spi.setPinMapping(gpio_d0, gpio_d1, ..., gpio_d31, gpio_clk)) return;  // false: mapping rejected
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
    static constexpr u16 MAX_BUFFER_SIZE = 256;

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
     * @return false, changing nothing, if a pin is outside 0-31, data lanes 0-7
     *         repeat or sit on the clock, or a lane 8+ sits on lanes 0-7.
     */
    FL_NO_DISCARD bool setPinMapping(u8 d0, u8 d1, u8 d2, u8 d3,
                       u8 d4, u8 d5, u8 d6, u8 d7,
                       u8 d8, u8 d9, u8 d10, u8 d11,
                       u8 d12, u8 d13, u8 d14, u8 d15,
                       u8 d16, u8 d17, u8 d18, u8 d19,
                       u8 d20, u8 d21, u8 d22, u8 d23,
                       u8 d24, u8 d25, u8 d26, u8 d27,
                       u8 d28, u8 d29, u8 d30, u8 d31,
                       u8 clk) FL_NO_EXCEPT {
        // Validate first, so a rejected mapping leaves the last good one in place.
        const u8 dataPins[] = {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31};
        if (!spiPinMappingValid(dataPins, clk)) {
            return false;
        }
        // Store clock mask
        mClockMask = u32(1) << clk;

        // Build data pin masks array
        u32 dataPinMasks[NUM_DATA_PINS] = {
            u32(1) << d0,   // Bit 0 (LSB)
            u32(1) << d1,   // Bit 1
            u32(1) << d2,   // Bit 2
            u32(1) << d3,   // Bit 3
            u32(1) << d4,   // Bit 4
            u32(1) << d5,   // Bit 5
            u32(1) << d6,   // Bit 6
            u32(1) << d7,   // Bit 7
            u32(1) << d8,   // Bit 8
            u32(1) << d9,   // Bit 9
            u32(1) << d10,  // Bit 10
            u32(1) << d11,  // Bit 11
            u32(1) << d12,  // Bit 12
            u32(1) << d13,  // Bit 13
            u32(1) << d14,  // Bit 14
            u32(1) << d15,  // Bit 15
            u32(1) << d16,  // Bit 16
            u32(1) << d17,  // Bit 17
            u32(1) << d18,  // Bit 18
            u32(1) << d19,  // Bit 19
            u32(1) << d20,  // Bit 20
            u32(1) << d21,  // Bit 21
            u32(1) << d22,  // Bit 22
            u32(1) << d23,  // Bit 23
            u32(1) << d24,  // Bit 24
            u32(1) << d25,  // Bit 25
            u32(1) << d26,  // Bit 26
            u32(1) << d27,  // Bit 27
            u32(1) << d28,  // Bit 28
            u32(1) << d29,  // Bit 29
            u32(1) << d30,  // Bit 30
            u32(1) << d31   // Bit 31 (MSB)
        };

        // Initialize 256-entry LUT
        // For each possible byte value (0-255):
        // - Map all 8 bits to corresponding GPIO pins (32 total pins available)
        // - Bits 8-31 are always cleared (no corresponding bits in a byte)
        // - Generate set_mask (pins to set high) and clear_mask (pins to clear low)
        for (int byteValue = 0; byteValue < 256; byteValue++) {
            u32 setMask = 0;
            u32 clearMask = 0;

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
        return true;
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
    void loadBuffer(const u8* data, u16 n) FL_NO_EXCEPT {
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
    template <bool GPIO_AVAILABLE = FL_SPI_GPIO_AVAILABLE>
    void transmit() FL_NO_EXCEPT {
        FL_STATIC_ASSERT(GPIO_AVAILABLE && FL_SPI_GPIO_AVAILABLE,
                      "Parallel software SPI has no GPIO registers for this target: define "
                      "FASTLED_GPIO_W1TS_ADDR and FASTLED_GPIO_W1TC_ADDR");
        if (!mBuffer || mBufferLen == 0 || mClockMask == 0) return;  // Unmapped: drive nothing.

        // Inline bit-banging loop (same logic as 16-way implementation)
        for (u16 i = 0; i < mBufferLen; i++) {
            u8 byte = mBuffer[i];

            // Phase 0: Present data + force CLK low
            u32 pins_to_set = mLUT[byte].set_mask;
            u32 pins_to_clear = mLUT[byte].clear_mask | mClockMask;

            FL_GPIO_WRITE_SET(pins_to_set);      // data-high bits
            FL_GPIO_WRITE_CLEAR(pins_to_clear);  // data-low bits + CLK low

            // Phase 1: Raise CLK high to latch data
            FL_GPIO_WRITE_SET(mClockMask);
        }
    }

    /**
     * Get buffer pointer (for inspection)
     */
    const u8* getBuffer() const FL_NO_EXCEPT {
        return mBuffer;
    }

    /**
     * Get buffer length (for inspection)
     */
    u16 getBufferLength() const FL_NO_EXCEPT {
        return mBufferLen;
    }

    /**
     * Get LUT array (for advanced users who want direct LUT control)
     */
    PinMaskEntry* getLUTArray() FL_NO_EXCEPT {
        return mLUT;
    }

private:
    u32 mClockMask = 0;           ///< Clock pin mask
    PinMaskEntry mLUT[256];            ///< 256-entry lookup table
    const u8* mBuffer = nullptr;  ///< Data buffer pointer
    u16 mBufferLen = 0;           ///< Buffer length
};

// C++11 requires out-of-class definitions for static constexpr members that are ODR-used
// These definitions are in src/fl/static_constexpr_defs.cpp to avoid duplicate symbols

}  // namespace fl
