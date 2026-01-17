/// @file wave8_encoder_spi.cpp
/// @brief Implementation of wave8 encoding for ESP32 SPI

#include "fl/compiler_control.h"
#include "wave8_encoder_spi.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/stl/algorithm.h"
#include "fl/dbg.h"
#include "fl/warn.h"

// The encoding functions (wave8Encode*) are platform-agnostic and work on all platforms.
// Only convertSpiTimingToChipsetTiming() requires ESP32-specific headers.

// ESP32-specific: Include SpiTimingConfig definition for conversion function
// Requires FASTLED_ESP32_HAS_CLOCKLESS_SPI since SpiTimingConfig is defined in
// channel_engine_spi.h which is guarded by that flag
#ifdef ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "channel_engine_spi.h"  // For SpiTimingConfig definition
#endif
#endif

namespace fl {

// ============================================================================
// ESP32-specific: SpiTimingConfig conversion (requires channel_engine_spi.h)
// ============================================================================

#if defined(ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI

ChipsetTiming convertSpiTimingToChipsetTiming(const SpiTimingConfig& spiTiming) {
    // SPI timing structure:
    // - clock_hz: SPI clock frequency (e.g., 2.5 MHz for WS2812)
    // - bits_per_led_bit: Number of SPI bits per LED bit (e.g., 3 for WS2812)
    // - bit0_pattern: SPI bit pattern for LED bit '0' (e.g., 0b100 for WS2812)
    // - bit1_pattern: SPI bit pattern for LED bit '1' (e.g., 0b110 for WS2812)
    //
    // ChipsetTiming structure (3-phase LED protocol):
    // - T1: High time for bit '1' (nanoseconds)
    // - T2: Low time for bit '1' (nanoseconds)
    // - T3: High time for bit '0' (nanoseconds)
    //
    // Conversion strategy:
    // 1. Calculate SPI bit period: T_spi = 1/clock_hz (nanoseconds)
    // 2. Count HIGH bits in bit1_pattern → T1 = count * T_spi
    // 3. Count LOW bits in bit1_pattern → T2 = (bits_per_led_bit - count) * T_spi
    // 4. Count HIGH bits in bit0_pattern → T3 = count * T_spi
    // 5. Total period for bit '1': T1 + T2
    // 6. Total period for bit '0': T3 + (bits_per_led_bit - count) * T_spi
    //
    // Note: LED protocols typically have T1+T2 ≈ T3+T4 (same total period)

    // Calculate SPI bit period in nanoseconds
    const uint32_t spi_bit_period_ns = 1000000000U / spiTiming.clock_hz;

    // Count HIGH bits in bit1_pattern (these become T1)
    uint8_t bit1_high_count = 0;
    for (uint8_t i = 0; i < spiTiming.bits_per_led_bit; i++) {
        if (spiTiming.bit1_pattern & (1U << (spiTiming.bits_per_led_bit - 1 - i))) {
            bit1_high_count++;
        }
    }

    // Count HIGH bits in bit0_pattern (these become T3)
    uint8_t bit0_high_count = 0;
    for (uint8_t i = 0; i < spiTiming.bits_per_led_bit; i++) {
        if (spiTiming.bit0_pattern & (1U << (spiTiming.bits_per_led_bit - 1 - i))) {
            bit0_high_count++;
        }
    }

    // Build ChipsetTiming
    ChipsetTiming chipsetTiming;
    chipsetTiming.T1 = bit1_high_count * spi_bit_period_ns;           // High time for '1'
    chipsetTiming.T2 = (spiTiming.bits_per_led_bit - bit1_high_count) * spi_bit_period_ns;  // Low time for '1'
    chipsetTiming.T3 = bit0_high_count * spi_bit_period_ns;           // High time for '0'

    // Debug output temporarily commented due to nested namespace issues with FL_DBG macro
    // TODO(iteration 12): Fix FL_DBG macro usage inside nested namespace
    // FL_DBG("convertSpiTimingToChipsetTiming: clock=" << spiTiming.clock_hz
    //        << "Hz, bits_per_led_bit=" << static_cast<int>(spiTiming.bits_per_led_bit)
    //        << ", bit0_pattern=0x" << ::fl::hex(spiTiming.bit0_pattern)
    //        << ", bit1_pattern=0x" << ::fl::hex(spiTiming.bit1_pattern)
    //        << " → T1=" << chipsetTiming.T1 << "ns"
    //        << ", T2=" << chipsetTiming.T2 << "ns"
    //        << ", T3=" << chipsetTiming.T3 << "ns");

    return chipsetTiming;
}

#endif // defined(ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI

} // namespace fl

// ============================================================================
// Platform-agnostic: Wave8 encoding functions (work on all platforms)
// These are defined in fl namespace using explicit fl:: qualification
// to avoid issues with nested namespace lookups in ESP32 GCC
// ============================================================================

namespace fl {

size_t wave8EncodeSingleLane(
    ::fl::span<const uint8_t> input,
    ::fl::span<uint8_t> output,
    const ::fl::Wave8BitExpansionLut& lut) {

    // Validate output buffer size
    const size_t required_size = input.size() * 8;
    if (output.size() < required_size) {
        using ::fl::StrStream; using ::fl::println; using ::fl::fastled_file_offset;
        println((StrStream() << (fastled_file_offset(__FILE__)) << "(" << int(__LINE__) << "): WARN: "
                << "wave8EncodeSingleLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)").c_str());
        return 0;
    }

    // Encode each input byte to Wave8Byte (8 output bytes)
    size_t output_idx = 0;
    for (size_t i = 0; i < input.size(); i++) {
        ::fl::Wave8Byte wave8_output;
        ::fl::detail::wave8_convert_byte_to_wave8byte(input[i], lut, &wave8_output);

        // Copy Wave8Byte to output buffer (8 bytes)
        ::fl::memcpy(output.data() + output_idx, &wave8_output, sizeof(::fl::Wave8Byte));
        output_idx += sizeof(::fl::Wave8Byte);
    }

    return output_idx;
}

size_t wave8EncodeDualLane(
    ::fl::span<const uint8_t> lane0,
    ::fl::span<const uint8_t> lane1,
    ::fl::span<uint8_t> output,
    const ::fl::Wave8BitExpansionLut& lut) {

    // Validate lane sizes match
    if (lane0.size() != lane1.size()) {
        using ::fl::StrStream; using ::fl::println; using ::fl::fastled_file_offset;
        println((StrStream() << (fastled_file_offset(__FILE__)) << "(" << int(__LINE__) << "): WARN: "
                << "wave8EncodeDualLane: Lane sizes mismatch (lane0="
                << lane0.size() << " bytes, lane1=" << lane1.size() << " bytes)").c_str());
        return 0;
    }

    // Validate output buffer size
    const size_t required_size = lane0.size() * 16;  // 2 Wave8Byte structures per byte
    if (output.size() < required_size) {
        using ::fl::StrStream; using ::fl::println; using ::fl::fastled_file_offset;
        println((StrStream() << (fastled_file_offset(__FILE__)) << "(" << int(__LINE__) << "): WARN: "
                << "wave8EncodeDualLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)").c_str());
        return 0;
    }

    // Encode each byte pair with transposition
    size_t output_idx = 0;
    for (size_t i = 0; i < lane0.size(); i++) {
        // Convert both lane bytes to Wave8Byte
        ::fl::Wave8Byte wave8_lane0, wave8_lane1;
        ::fl::detail::wave8_convert_byte_to_wave8byte(lane0[i], lut, &wave8_lane0);
        ::fl::detail::wave8_convert_byte_to_wave8byte(lane1[i], lut, &wave8_lane1);

        // Transpose 2 lanes into interleaved format
        ::fl::Wave8Byte lane_array[2] = {wave8_lane0, wave8_lane1};
        uint8_t transposed[2 * sizeof(::fl::Wave8Byte)];
        ::fl::detail::wave8_transpose_2(lane_array, transposed);

        // Copy transposed data to output
        ::fl::memcpy(output.data() + output_idx, transposed, sizeof(transposed));
        output_idx += sizeof(transposed);
    }

    return output_idx;
}

size_t wave8EncodeQuadLane(
    ::fl::span<const uint8_t> lanes[4],
    ::fl::span<uint8_t> output,
    const ::fl::Wave8BitExpansionLut& lut) {

    // Validate all lane sizes match
    const size_t lane_size = lanes[0].size();
    for (int i = 1; i < 4; i++) {
        if (lanes[i].size() != lane_size) {
            using ::fl::StrStream; using ::fl::println; using ::fl::fastled_file_offset;
            println((StrStream() << (fastled_file_offset(__FILE__)) << "(" << int(__LINE__) << "): WARN: "
                    << "wave8EncodeQuadLane: Lane size mismatch (lane0="
                    << lane_size << " bytes, lane" << i << "=" << lanes[i].size() << " bytes)").c_str());
            return 0;
        }
    }

    // Validate output buffer size
    const size_t required_size = lane_size * 32;  // 4 Wave8Byte structures per byte set
    if (output.size() < required_size) {
        using ::fl::StrStream; using ::fl::println; using ::fl::fastled_file_offset;
        println((StrStream() << (fastled_file_offset(__FILE__)) << "(" << int(__LINE__) << "): WARN: "
                << "wave8EncodeQuadLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)").c_str());
        return 0;
    }

    // Encode each byte set with transposition
    size_t output_idx = 0;
    for (size_t i = 0; i < lane_size; i++) {
        // Convert all 4 lane bytes to Wave8Byte
        ::fl::Wave8Byte wave8_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
            ::fl::detail::wave8_convert_byte_to_wave8byte(lanes[lane][i], lut, &wave8_lanes[lane]);
        }

        // Transpose 4 lanes into interleaved format
        uint8_t transposed[4 * sizeof(::fl::Wave8Byte)];
        ::fl::detail::wave8_transpose_4(wave8_lanes, transposed);

        // Copy transposed data to output
        ::fl::memcpy(output.data() + output_idx, transposed, sizeof(transposed));
        output_idx += sizeof(transposed);
    }

    return output_idx;
}

} // namespace fl
