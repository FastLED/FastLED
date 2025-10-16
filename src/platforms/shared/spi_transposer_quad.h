#pragma once

/// @file spi_transposer_quad.h
/// @brief Stateless bit-interleaving transpose logic for Quad-SPI parallel LED control
///
/// This provides a pure functional approach to bit-interleaving for Quad-SPI transmission.
/// All state is managed by the caller - the transposer only performs the conversion.
///
/// # How Bit-Interleaving Works
///
/// Traditional SPI sends one byte at a time on a single data line (MOSI).
/// Quad-SPI uses 4 data lines (D0-D3) to send 4 bits in parallel per clock cycle.
///
/// The transposer converts per-lane data into interleaved format:
///
/// **Input (4 separate lanes):**
/// ```
/// Lane 0: [0xAB, 0xCD, ...]  → Strip 1 (D0 pin)
/// Lane 1: [0x12, 0x34, ...]  → Strip 2 (D1 pin)
/// Lane 2: [0xEF, 0x56, ...]  → Strip 3 (D2 pin)
/// Lane 3: [0x78, 0x90, ...]  → Strip 4 (D3 pin)
/// ```
///
/// **Output (interleaved for quad-SPI):**
/// ```
/// Each input byte becomes 4 output bytes (2 bits per lane per output byte):
///
/// Input byte index 0:
///   Lane0[0]=0xAB (10101011), Lane1[0]=0x12 (00010010),
///   Lane2[0]=0xEF (11101111), Lane3[0]=0x78 (01111000)
///
/// Output bytes (bit order: [D3b7 D2b7 D1b7 D0b7 D3b6 D2b6 D1b6 D0b6]):
///   Out[0] = 0b01110010 (bits 7:6 from each lane)
///   Out[1] = 0b11110110 (bits 5:4 from each lane)
///   Out[2] = 0b10010011 (bits 3:2 from each lane)
///   Out[3] = 0b00111011 (bits 1:0 from each lane)
/// ```
///
/// When transmitted via quad-SPI, each output byte sends 2 bits to each of the
/// 4 data lines simultaneously, reconstructing the original byte streams in parallel.
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
/// optional<SPITransposerQuad::LaneData> lanes[4];
/// lanes[0] = SPITransposerQuad::LaneData{lane0_data, apa102_padding};
/// lanes[1] = SPITransposerQuad::LaneData{lane1_data, apa102_padding};
/// // lanes[2] and lanes[3] remain empty (no value)
///
/// // Allocate output buffer (caller manages memory)
/// size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
/// vector<uint8_t> output(max_size * 4);
///
/// // Perform transpose
/// const char* error = nullptr;
/// bool success = SPITransposerQuad::transpose(lanes, max_size, output, &error);
/// if (!success) {
///     // Handle error
///     FL_WARN("Transpose failed: " << error);
/// }
/// @endcode
///
/// # Performance
///
/// - **CPU overhead**: Minimal - just the transpose operation (runs once per frame)
/// - **Transpose time**: ~50-100µs for 4×100 LED strips with optimized algorithm
/// - **Transmission time**: ~0.08ms at 40 MHz (hardware DMA, zero CPU)
/// - **Total speedup**: 27× faster than serial transmission with 0% CPU usage
/// - **Optimization**: Direct bit extraction provides 2-3× speedup over nested loops
///
/// @see QuadSPIController for the full ESP32 hardware integration
/// @see fl/quad_spi_platform.h for platform detection macros

#include "spi_transposer.h"

namespace fl {

/// @deprecated Use SPITransposer::transpose4() or SPITransposer::transpose8() instead
///
/// Backward-compatible wrapper for Quad-SPI bit-interleaving.
/// This class now delegates to the unified SPITransposer implementation.
///
/// Pure functional design: no instance state, all data provided by caller.
/// Memory management is the caller's responsibility.
class SPITransposerQuad {
public:
    /// Lane data structure: payload + padding frame
    using LaneData = SPITransposer::LaneData;

    /// @deprecated Use SPITransposer::transpose4() instead
    ///
    /// Transpose up to 4 lanes of data into interleaved quad-SPI format
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
    [[deprecated("Use SPITransposer::transpose4() instead")]]
    static bool transpose(const fl::optional<LaneData>& lane0,
                         const fl::optional<LaneData>& lane1,
                         const fl::optional<LaneData>& lane2,
                         const fl::optional<LaneData>& lane3,
                         fl::span<uint8_t> output,
                         const char** error = nullptr);

    /// @deprecated Use SPITransposer::transpose8() instead
    ///
    /// Transpose up to 8 lanes of data into interleaved octal-SPI format
    ///
    /// @param lanes Array of 8 lane data (use fl::nullopt for unused lanes)
    /// @param output Output buffer to write interleaved data (size must be divisible by 8)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 8
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    [[deprecated("Use SPITransposer::transpose8() instead")]]
    static bool transpose8(const fl::optional<LaneData> lanes[8],
                          fl::span<uint8_t> output,
                          const char** error = nullptr);
};

}  // namespace fl
