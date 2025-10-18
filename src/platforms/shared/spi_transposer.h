#pragma once

/// @file spi_transposer.h
/// @brief Unified bit-interleaving transpose logic for multi-lane SPI parallel LED control
///
/// This provides a unified functional approach to bit-interleaving for multi-lane SPI transmission.
/// All state is managed by the caller - the transposer only performs the conversion.
///
/// # Supported Widths
///
/// - **2-way SPI**: `transpose2()` - 2 parallel data lanes
/// - **4-way SPI**: `transpose4()` - 4 parallel data lanes
/// - **8-way SPI**: `transpose8()` - 8 parallel data lanes
///
/// # How Bit-Interleaving Works
///
/// Traditional SPI sends one byte at a time on a single data line (MOSI).
/// Multi-lane SPI uses N data lines (D0-DN) to send N bits in parallel per clock cycle.
///
/// The transposer converts per-lane data into interleaved format:
///
/// **Example: 2-way SPI**
/// ```
/// Input (2 separate lanes):
///   Lane 0: [0xAB, ...] → Strip 1 (D0 pin)
///   Lane 1: [0x12, ...] → Strip 2 (D1 pin)
///
/// Output (interleaved): Each input byte becomes 2 output bytes
///   Input: Lane0=0xAB (10101011), Lane1=0x12 (00010010)
///   Out[0] = 0x2A (bits 7:4 from each lane, 4 bits per lane)
///   Out[1] = 0x2B (bits 3:0 from each lane, 4 bits per lane)
/// ```
///
/// **Example: 4-way SPI**
/// ```
/// Input (4 separate lanes):
///   Lane 0-3: [0xAB, 0x12, 0xEF, 0x78, ...] → Strips 1-4
///
/// Output (interleaved): Each input byte becomes 4 output bytes
///   Out[0-3] = bit pairs interleaved (2 bits per lane per output byte)
/// ```
///
/// **Example: 8-way SPI**
/// ```
/// Input (8 separate lanes):
///   Lane 0-7: [...] → Strips 1-8
///
/// Output (interleaved): Each input byte becomes 8 output bytes
///   Out[0-7] = individual bits interleaved (1 bit per lane per output byte)
/// ```
///
/// # Synchronized Latching with Black LED Padding
///
/// LED strips often have different lengths. To ensure all strips latch simultaneously
/// (updating LEDs at the same time), shorter strips are padded with black LED frames
/// at the BEGINNING of the data stream.
///
/// Common padding patterns:
/// - **APA102/SK9822**: {0xE0, 0x00, 0x00, 0x00} (brightness=0, RGB=0)
/// - **LPD8806**: {0x80, 0x80, 0x80} (7-bit GRB with MSB=1, all colors 0)
/// - **WS2801**: {0x00, 0x00, 0x00} (RGB all zero)
/// - **P9813**: {0xFF, 0x00, 0x00, 0x00} (flag byte + BGR all zero)
///
/// These invisible black LEDs are prepended so all strips finish transmitting
/// simultaneously, providing synchronized visual updates across all parallel strips.
///
/// # Usage Example
///
/// @code
/// using namespace fl;
///
/// // Prepare lane data
/// vector<uint8_t> lane0_data = {0xAB, 0xCD, ...};
/// vector<uint8_t> lane1_data = {0x12, 0x34, ...};
/// span<const uint8_t> apa102_padding = APA102Controller<...>::getPaddingLEDFrame();
///
/// // Setup lanes (optional for unused lanes)
/// optional<SPITransposer::LaneData> lane0 = SPITransposer::LaneData{lane0_data, apa102_padding};
/// optional<SPITransposer::LaneData> lane1 = SPITransposer::LaneData{lane1_data, apa102_padding};
///
/// // Allocate output buffer (caller manages memory)
/// size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
/// vector<uint8_t> output(max_size * 2);  // 2× for 2-way, 4× for 4-way, 8× for 8-way
///
/// // Perform transpose
/// const char* error = nullptr;
/// bool success = SPITransposer::transpose2(lane0, lane1, output, &error);
/// if (!success) {
///     FL_WARN("Transpose failed: " << error);
/// }
/// @endcode
///
/// # Performance
///
/// - **CPU overhead**: Minimal - just the transpose operation (runs once per frame)
/// - **Transpose time**: ~25-100µs depending on lane count and data size
/// - **Transmission time**: Hardware DMA, zero CPU usage during transfer
/// - **Optimization**: Direct bit extraction provides optimal performance

#include "fl/span.h"
#include "fl/optional.h"

namespace fl {

/// Unified stateless bit-interleaving transposer for multi-lane SPI parallel LED transmission
///
/// Pure functional design: no instance state, all data provided by caller.
/// Memory management is the caller's responsibility.
class SPITransposer {
public:
    /// Lane data structure: payload + padding frame
    struct LaneData {
        fl::span<const uint8_t> payload;        ///< Actual LED data for this lane
        fl::span<const uint8_t> padding_frame;  ///< Black LED frame for padding (repeating pattern)
    };

    /// Transpose 2 lanes of data into interleaved dual-SPI format
    ///
    /// @param lane0 Lane 0 data (use fl::nullopt for unused lane)
    /// @param lane1 Lane 1 data (use fl::nullopt for unused lane)
    /// @param output Output buffer to write interleaved data (size must be divisible by 2)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 2
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose2(const fl::optional<LaneData>& lane0,
                          const fl::optional<LaneData>& lane1,
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 4 lanes of data into interleaved quad-SPI format
    ///
    /// @param lane0 Lane 0 data (use fl::nullopt for unused lane)
    /// @param lane1 Lane 1 data (use fl::nullopt for unused lane)
    /// @param lane2 Lane 2 data (use fl::nullopt for unused lane)
    /// @param lane3 Lane 3 data (use fl::nullopt for unused lane)
    /// @param output Output buffer to write interleaved data (size must be divisible by 4)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 4
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose4(const fl::optional<LaneData>& lane0,
                          const fl::optional<LaneData>& lane1,
                          const fl::optional<LaneData>& lane2,
                          const fl::optional<LaneData>& lane3,
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 8 lanes of data into interleaved octal-SPI format
    ///
    /// @param lanes Array of 8 lane data (use fl::nullopt for unused lanes)
    /// @param output Output buffer to write interleaved data (size must be divisible by 8)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 8
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose8(const fl::optional<LaneData> lanes[8],
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 16 lanes of data into interleaved hex-SPI format
    ///
    /// @param lanes Array of 16 lane data (use fl::nullopt for unused lanes)
    /// @param output Output buffer to write interleaved data (size must be divisible by 16)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 16
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose16(const fl::optional<LaneData> lanes[16],
                           fl::span<uint8_t> output,
                           const char** error = nullptr);

private:
    /// Optimized bit interleaving for 2 lanes (dual-SPI)
    /// @param dest Output buffer (must have space for 2 bytes)
    /// @param a Lane 0 input byte
    /// @param b Lane 1 input byte
    static void interleave_byte_2way(uint8_t* dest, uint8_t a, uint8_t b);

    /// Optimized bit interleaving for 4 lanes (quad-SPI)
    /// @param dest Output buffer (must have space for 4 bytes)
    /// @param a Lane 0 input byte
    /// @param b Lane 1 input byte
    /// @param c Lane 2 input byte
    /// @param d Lane 3 input byte
    static void interleave_byte_4way(uint8_t* dest, uint8_t a, uint8_t b, uint8_t c, uint8_t d);

    /// Optimized bit interleaving for 8 lanes (octal-SPI)
    /// @param dest Output buffer (must have space for 8 bytes)
    /// @param lane_bytes Array of 8 input bytes (one per lane)
    static void interleave_byte_8way(uint8_t* dest, const uint8_t lane_bytes[8]);

    /// Optimized bit interleaving for 16 lanes (hex-SPI)
    /// @param dest Output buffer (must have space for 16 bytes)
    /// @param lane_bytes Array of 16 input bytes (one per lane)
    static void interleave_byte_16way(uint8_t* dest, const uint8_t lane_bytes[16]);

    /// Get byte from lane at given index, handling padding automatically
    /// @param lane Lane data (payload + padding frame)
    /// @param byte_idx Byte index in the padded output
    /// @param max_size Maximum lane size (for padding calculation)
    /// @return Byte value (from data or padding)
    static uint8_t getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size);
};

}  // namespace fl
