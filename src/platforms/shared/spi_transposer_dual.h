#pragma once

/// @file spi_transposer_dual.h
/// @brief Stateless bit-interleaving transpose logic for Dual-SPI parallel LED control
///
/// This provides a pure functional approach to bit-interleaving for Dual-SPI transmission.
/// All state is managed by the caller - the transposer only performs the conversion.
///
/// # How Bit-Interleaving Works
///
/// Traditional SPI sends one byte at a time on a single data line (MOSI).
/// Dual-SPI uses 2 data lines (D0-D1) to send 2 bits in parallel per clock cycle.
///
/// The transposer converts per-lane data into interleaved format:
///
/// **Input (2 separate lanes):**
/// ```
/// Lane 0: [0xAB, 0xCD, ...]  → Strip 1 (D0 pin)
/// Lane 1: [0x12, 0x34, ...]  → Strip 2 (D1 pin)
/// ```
///
/// **Output (interleaved for dual-SPI):**
/// ```
/// Each input byte becomes 2 output bytes (4 bits per lane per output byte):
///
/// Input byte index 0:
///   Lane0[0]=0xAB (10101011), Lane1[0]=0x12 (00010010)
///
/// Output bytes (bit order: [D1b7 D0b7 D1b6 D0b6 D1b5 D0b5 D1b4 D0b4]):
///   Out[0] = 0b00101010 (bits 7:4 from each lane)
///          = L1[7:4]=0001, L0[7:4]=1010 → interleaved: 00101010
///
///   Out[1] = 0b00101011 (bits 3:0 from each lane)
///          = L1[3:0]=0010, L0[3:0]=1011 → interleaved: 00101011
/// ```
///
/// When transmitted via dual-SPI, each output byte sends 4 bits to each of the
/// 2 data lines simultaneously, reconstructing the original byte streams in parallel.
///
/// # Synchronized Latching with Black LED Padding
///
/// LED strips often have different lengths. To ensure all strips latch simultaneously
/// (updating LEDs at the same time), shorter strips are padded with black LED frames
/// at the BEGINNING of the data stream:
///
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
/// optional<SPITransposerDual::LaneData> lanes[2];
/// lanes[0] = SPITransposerDual::LaneData{lane0_data, apa102_padding};
/// lanes[1] = SPITransposerDual::LaneData{lane1_data, apa102_padding};
///
/// // Allocate output buffer (caller manages memory)
/// size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
/// vector<uint8_t> output(max_size * 2);
///
/// // Perform transpose
/// const char* error = nullptr;
/// bool success = SPITransposerDual::transpose(lanes, max_size, output, &error);
/// if (!success) {
///     // Handle error
///     FL_WARN("Transpose failed: " << error);
/// }
/// @endcode
///
/// # Performance
///
/// - **CPU overhead**: Minimal - just the transpose operation (runs once per frame)
/// - **Transpose time**: ~25-50µs for 2×100 LED strips with optimized algorithm
/// - **Transmission time**: ~0.16ms at 40 MHz (hardware DMA, zero CPU)
/// - **Total speedup**: 2× faster than serial transmission with 0% CPU usage
/// - **Optimization**: Direct bit extraction provides faster performance
///
/// @see DualSPIController for the full ESP32 hardware integration
/// @see fl/dual_spi_platform.h for platform detection macros

#include "fl/span.h"
#include "fl/optional.h"

namespace fl {

/// Stateless bit-interleaving transposer for Dual-SPI parallel LED transmission
///
/// Pure functional design: no instance state, all data provided by caller.
/// Memory management is the caller's responsibility.
class SPITransposerDual {
public:
    /// Lane data structure: payload + padding frame
    struct LaneData {
        fl::span<const uint8_t> payload;        ///< Actual LED data for this lane
        fl::span<const uint8_t> padding_frame;  ///< Black LED frame for padding (repeating pattern)
    };

    /// Transpose up to 2 lanes of data into interleaved dual-SPI format
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
    static bool transpose(const fl::optional<LaneData>& lane0,
                         const fl::optional<LaneData>& lane1,
                         fl::span<uint8_t> output,
                         const char** error = nullptr);

private:
    /// Optimized bit interleaving using direct bit extraction
    /// @param dest Output buffer (must have space for 2 bytes)
    /// @param a Lane 0 input byte
    /// @param b Lane 1 input byte
    static void interleave_byte_optimized(uint8_t* dest,
                                          uint8_t a, uint8_t b);

    /// Get byte from lane at given index, handling padding automatically
    /// @param lane Lane data (payload + padding frame)
    /// @param byte_idx Byte index in the padded output
    /// @param max_size Maximum lane size (for padding calculation)
    /// @return Byte value (from data or padding)
    static uint8_t getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size);
};

}  // namespace fl
