/// @file wave8_encoder_i2s.h
/// @brief Wave8 encoding for ESP32-S3 I2S LCD_CAM channel engine
///
/// This encoder converts LED byte data into I2S LCD_CAM waveforms using wave8 expansion,
/// enabling the I2S peripheral to generate WS2812-compatible timing via parallel GPIO.
///
/// ## Architecture
/// The I2S LCD_CAM peripheral outputs 16 bits per clock cycle. For single-lane operation,
/// only one bit (D0) is used for LED data, with the remaining bits set to 0.
///
/// ## Wave8 Encoding
/// Each LED data bit expands to 8 output bits (pulses):
/// - Bit '1': 0b11111100 (6 HIGH, 2 LOW) - ~750ns HIGH, ~250ns LOW at 8MHz
/// - Bit '0': 0b11000000 (2 HIGH, 6 LOW) - ~250ns HIGH, ~750ns LOW at 8MHz
///
/// ## Output Format
/// Each LED byte (8 bits) expands to 8 Wave8Bit structures (8 bytes total).
/// For I2S 16-bit mode, these are packed as uint16_t with data on D0.

#pragma once

#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"

namespace fl {

/// @brief Encode single-lane LED data for I2S LCD_CAM using wave8 expansion
///
/// Converts LED byte data to I2S waveforms. Each LED byte becomes 64 bits
/// (8 bytes × 8 bits/byte) of output, where only D0 carries the signal.
///
/// @param input LED byte data (RGB values)
/// @param output Output buffer for I2S 16-bit words
/// @param lut Wave8 expansion lookup table
/// @return Number of uint16_t words written
size_t wave8EncodeI2sSingleLane(
    fl::span<const uint8_t> input,
    fl::span<uint16_t> output,
    const Wave8BitExpansionLut& lut);

/// @brief Encode multi-lane LED data for I2S LCD_CAM using wave8 expansion
///
/// Converts multiple lanes of LED data to I2S waveforms with parallel output.
/// Each LED byte from each lane becomes 8 output bits, interleaved across lanes.
///
/// For 2 lanes: D0 = lane0, D1 = lane1
/// For 4 lanes: D0 = lane0, D1 = lane1, D2 = lane2, D3 = lane3
/// etc.
///
/// @param lanes Array of lane data spans
/// @param num_lanes Number of active lanes (1-16)
/// @param output Output buffer for I2S 16-bit words
/// @param lut Wave8 expansion lookup table
/// @return Number of uint16_t words written
size_t wave8EncodeI2sMultiLane(
    fl::span<const uint8_t>* lanes,
    int num_lanes,
    fl::span<uint16_t> output,
    const Wave8BitExpansionLut& lut);

/// @brief Calculate required output buffer size for I2S wave8 encoding
///
/// @param input_bytes Number of input LED bytes per lane
/// @return Required output buffer size in uint16_t words
constexpr size_t wave8CalculateI2sOutputSize(size_t input_bytes) {
    // Each input byte → 8 Wave8Bit → 64 bits → 4 uint16_t words
    return input_bytes * 4;
}

/// @brief Calculate I2S clock frequency for WS2812 timing
///
/// Given the LED timing and wave8 expansion (8 pulses per bit), calculates
/// the required I2S PCLK frequency.
///
/// WS2812 timing: T0H=350ns, T1H=700ns, T0L=800ns, T1L=600ns
/// Total period: ~1.25µs per bit
/// Wave8: 8 pulses per bit
/// Pulse period: 1.25µs / 8 = 156.25ns
/// Clock frequency: 1 / 156.25ns ≈ 6.4 MHz
///
/// @param timing ChipsetTiming configuration
/// @return Recommended I2S PCLK frequency in Hz
uint32_t calculateI2sClockHz(const ChipsetTiming& timing);

} // namespace fl
