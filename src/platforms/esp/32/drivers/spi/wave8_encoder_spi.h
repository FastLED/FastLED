/// @file wave8_encoder_spi.h
/// @brief Wave8 encoding for ESP32 SPI channel engine
///
/// This encoder converts LED byte data into SPI waveforms using wave8 expansion,
/// similar to the PARLIO driver but optimized for SPI hardware characteristics.
///
/// Key differences from PARLIO:
/// - Single-lane mode (ESP32-C3): No transposition needed (direct wave8 encoding)
/// - Multi-lane mode (dual/quad): Requires transposition for parallel transmission
/// - SPI-specific timing: Uses SPI clock divider and bit patterns
///
/// Architecture:
/// 1. Build Wave8BitExpansionLut from SpiTimingConfig (one-time setup)
/// 2. Single-lane: wave8_convert_byte_to_wave8byte() → output (8 bytes per LED byte)
/// 3. Multi-lane: wave8_convert + wave8Transpose_N() → interleaved output
///
/// Performance:
/// - Single-lane: 1 LUT lookup per byte (2 nibbles), 8 bytes output
/// - Multi-lane: N LUT lookups + transpose (16-128 bytes output depending on lane count)

#pragma once

#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"

// ESP32-specific: check feature flag for clockless SPI support
#if defined(ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

namespace fl {

// ESP32-specific conversion function (requires SpiTimingConfig from channel_engine_spi.h)
// Only available when FASTLED_ESP32_HAS_CLOCKLESS_SPI is defined (ESP-IDF 5.0+)
#if defined(ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI

// Forward declaration of SpiTimingConfig (defined in channel_engine_spi.h)
struct SpiTimingConfig;

/// @brief Convert SpiTimingConfig to ChipsetTiming for wave8 LUT generation
///
/// SpiTimingConfig uses SPI-specific parameters (clock_hz, bits_per_led_bit, bit patterns).
/// ChipsetTiming uses standard LED protocol timing (T1, T2, T3 in nanoseconds).
///
/// This function converts SPI bit patterns into equivalent 3-phase timing suitable
/// for wave8 LUT generation.
///
/// @param spiTiming SPI timing configuration with clock and bit patterns
/// @return ChipsetTiming with T1, T2, T3 derived from SPI patterns
/// @note This function is ESP32-specific and only available when FASTLED_ESP32_HAS_CLOCKLESS_SPI is defined
ChipsetTiming convertSpiTimingToChipsetTiming(const SpiTimingConfig& spiTiming);

#endif // defined(ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI

/// @brief Encode single-lane LED data using wave8 expansion (no transposition)
///
/// Optimized for ESP32-C3 single-lane mode. Each LED byte expands to 8 bytes
/// (1 Wave8Byte) without interleaving.
///
/// Output format: Sequential Wave8Byte structures (8 bytes each)
///   Input:  [byte0, byte1, byte2, ...]
///   Output: [wave8(byte0), wave8(byte1), wave8(byte2), ...]
///           |<--8 bytes-->| |<--8 bytes-->| |<--8 bytes-->|
///
/// @param input LED byte data (typically RGB values)
/// @param output Output buffer for wave8-encoded SPI bits
/// @param lut Wave8 expansion lookup table (built from timing)
/// @return Number of bytes written to output (input.size() * 8)
size_t wave8EncodeSingleLane(
    fl::span<const uint8_t> input,
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut);

/// @brief Encode dual-lane LED data using wave8 + 2-lane transposition
///
/// For ESP32 dual-SPI mode (2 parallel data lines). Each pair of LED bytes
/// expands to 16 bytes (2 Wave8Byte structures transposed).
///
/// Output format: Interleaved 2-lane Wave8Byte structures
///   Input:  lane0=[byte0_L0, byte1_L0, ...], lane1=[byte0_L1, byte1_L1, ...]
///   Output: [transpose_2(wave8(byte0_L0), wave8(byte0_L1)), ...]
///           |<------------16 bytes-------------->|
///
/// @param lane0 LED byte data for lane 0
/// @param lane1 LED byte data for lane 1
/// @param output Output buffer for transposed wave8 data
/// @param lut Wave8 expansion lookup table
/// @return Number of bytes written to output (lane0.size() * 16)
size_t wave8EncodeDualLane(
    fl::span<const uint8_t> lane0,
    fl::span<const uint8_t> lane1,
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut);

/// @brief Encode quad-lane LED data using wave8 + 4-lane transposition
///
/// For ESP32-S3 quad-SPI mode (4 parallel data lines). Each set of 4 LED bytes
/// expands to 32 bytes (4 Wave8Byte structures transposed).
///
/// Output format: Interleaved 4-lane Wave8Byte structures
///   Input:  lane0-3 = [byte0_LN, byte1_LN, ...]
///   Output: [transpose_4(wave8(byte0_L0..L3)), ...]
///           |<------------32 bytes-------------->|
///
/// @param lanes Array of 4 lane spans (must all have same size)
/// @param output Output buffer for transposed wave8 data
/// @param lut Wave8 expansion lookup table
/// @return Number of bytes written to output (lanes[0].size() * 32)
size_t wave8EncodeQuadLane(
    fl::span<const uint8_t> lanes[4],
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut);

/// @brief Calculate required output buffer size for wave8 encoding
///
/// Helper function to determine output buffer size before encoding.
///
/// Formula:
/// - Single-lane: input_bytes * 8 (1 Wave8Byte per byte)
/// - Dual-lane:   input_bytes * 16 (2 Wave8Byte transposed per byte pair)
/// - Quad-lane:   input_bytes * 32 (4 Wave8Byte transposed per byte set)
///
/// @param input_bytes Number of input LED bytes
/// @param num_lanes Number of SPI lanes (1, 2, or 4)
/// @return Required output buffer size in bytes
constexpr size_t wave8CalculateOutputSize(size_t input_bytes, uint8_t num_lanes) {
    return input_bytes * 8 * num_lanes;
}

} // namespace fl
