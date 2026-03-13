/// @file wave8_encoder_uart.h
/// @brief Wave10 encoding for UART LED transmission
///
/// This encoder uses a "wave10" model that treats the full 10-bit UART frame
/// (START + 8 data + STOP) as the fundamental encoding unit. With TX inversion,
/// START=HIGH and STOP=LOW, giving 5 controllable pulses per LED bit (2 LED
/// bits per frame).
///
/// ## Wave10 Concept
///
/// Unlike wave8 (8 pulses per bit for PARLIO/SPI), wave10 accounts for UART's
/// automatic start/stop bits as part of the waveform:
///
/// ```
/// LED bit A: [START=H] [D0] [D1] [D2] [D3]     ← 5 pulses, START fixed HIGH
/// LED bit B: [D4]      [D5] [D6] [D7] [STOP=L]  ← 5 pulses, STOP fixed LOW
/// ```
///
/// ## Dynamic LUT Generation
///
/// The LUT is computed from chipset timing (T1/T2/T3) rather than hardcoded,
/// enabling support for any clockless LED protocol (WS2812, SK6812, WS2811, etc.)
///
/// ## Encoding Ratio
/// - 2 LED bits → 1 UART byte (8 data bits)
/// - 1 LED byte (8 bits) → 4 UART bytes
/// - 1 RGB LED (3 bytes) → 12 UART bytes
///
/// ## Timing Feasibility
///
/// A chipset is compatible when:
/// 1. Required baud rate ≤ 5.0 Mbps (ESP32 UART limit)
/// 2. T0H and T1H map to different pulse counts in [1, 4]
/// 3. Quantized timing is within half a pulse width of nominal

#pragma once

// IWYU pragma: private

#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"

namespace fl {

/// @brief Maximum UART baud rate supported by ESP32 variants
constexpr u32 kMaxUartBaudRate = 5000000;

/// @brief Wave10 lookup table for UART LED encoding
///
/// Maps 2 LED bits to 1 UART data byte, computed from chipset timing.
/// The 10-bit UART frame (with TX inversion) provides 5 pulses per LED bit.
///
/// Valid HIGH pulse count per LED bit: [1, 4]
/// - Bit A: START=H provides 1 guaranteed HIGH; D3 must be LOW for H→L transition
/// - Bit B: D4 must be HIGH for leading edge; STOP=L provides trailing LOW
struct Wave10Lut {
    u8 lut[4]; ///< 2-bit input → UART data byte

    /// @brief Compute the baud rate required for this timing
    /// @param timing Chipset timing configuration
    /// @return Baud rate in bits per second
    static u32 computeBaudRate(const ChipsetTimingConfig& timing) {
        // 10 UART bits encode 2 LED bits
        // baud = 10 / (2 * bit_period) * 1e9
        // = 10e9 / (2 * period_ns)
        // = 5e9 / period_ns
        const u32 period_ns = timing.total_period_ns();
        if (period_ns == 0) return 0;
        return static_cast<u32>(5000000000ULL / period_ns);
    }
};

/// @brief Build a Wave10 LUT from chipset timing parameters
///
/// Computes UART byte patterns that produce correct LED waveforms when
/// transmitted with TX inversion at the derived baud rate.
///
/// The 10-bit UART frame is modeled as two 5-pulse LED bit slots:
/// - Bit A: [START=H] [D0..D3] — START provides leading HIGH edge
/// - Bit B: [D4..D7] [STOP=L] — STOP provides trailing LOW
///
/// For each LED bit value (0 or 1), the HIGH pulse count within its
/// 5-pulse slot is computed from the chipset's T0H/T1H timing.
///
/// @param timing Chipset timing configuration
/// @return Populated Wave10Lut, or all-zero LUT if timing is infeasible
Wave10Lut buildWave10Lut(const ChipsetTimingConfig& timing);

/// @brief Check if a chipset timing can be accurately represented by UART
///
/// Validates that:
/// 1. Required baud rate ≤ 5.0 Mbps
/// 2. T0H pulse count is in [1, 4]
/// 3. T1H pulse count is in [1, 4]
/// 4. T0H and T1H pulse counts are distinguishable (different)
/// 5. Quantized T0H is within half a pulse width of nominal
/// 6. Quantized T1H is within half a pulse width of nominal
///
/// @param timing Chipset timing configuration
/// @return true if timing is representable, false otherwise
bool canRepresentTiming(const ChipsetTimingConfig& timing);

namespace detail {

/// @brief Encode 2 LED bits to 1 UART byte using a Wave10 LUT
/// @param two_bits 2-bit value (0-3)
/// @param lut Wave10 lookup table
/// @return UART data byte (8 bits)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
u8 encodeUart2Bits(u8 two_bits, const Wave10Lut& lut) {
    return lut.lut[two_bits & 0x03];
}

/// @brief Encode 1 LED byte to 4 UART bytes using a Wave10 LUT
/// @param led_byte LED data byte (8 bits)
/// @param output Output buffer (must have space for 4 bytes)
/// @param lut Wave10 lookup table
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void encodeUartByte(u8 led_byte, u8* output, const Wave10Lut& lut) {
    output[0] = encodeUart2Bits((led_byte >> 6) & 0x03, lut);  // Bits 7-6
    output[1] = encodeUart2Bits((led_byte >> 4) & 0x03, lut);  // Bits 5-4
    output[2] = encodeUart2Bits((led_byte >> 2) & 0x03, lut);  // Bits 3-2
    output[3] = encodeUart2Bits((led_byte >> 0) & 0x03, lut);  // Bits 1-0
}

/// @brief Legacy 2-bit encoding using hardcoded WS2812 LUT (backward compat)
/// @param two_bits 2-bit value (0-3)
/// @return UART data byte (8 bits)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
u8 encodeUart2Bits(u8 two_bits) {
    // Hardcoded WS2812 LUT for backward compatibility with existing tests
    static constexpr u8 kWs2812Lut[4] = {0xEF, 0x8F, 0xEC, 0x8C};
    return kWs2812Lut[two_bits & 0x03];
}

/// @brief Legacy byte encoding using hardcoded WS2812 LUT (backward compat)
/// @param led_byte LED data byte (8 bits)
/// @param output Output buffer (must have space for 4 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void encodeUartByte(u8 led_byte, u8* output) {
    output[0] = encodeUart2Bits((led_byte >> 6) & 0x03);  // Bits 7-6
    output[1] = encodeUart2Bits((led_byte >> 4) & 0x03);  // Bits 5-4
    output[2] = encodeUart2Bits((led_byte >> 2) & 0x03);  // Bits 3-2
    output[3] = encodeUart2Bits((led_byte >> 0) & 0x03);  // Bits 1-0
}

} // namespace detail

/// @brief Encode LED pixel data to UART bytes using a Wave10 LUT
///
/// @param input LED pixel data (RGB bytes)
/// @param input_size Input size in bytes
/// @param output Output buffer for UART bytes
/// @param output_capacity Output buffer size in bytes
/// @param lut Wave10 lookup table for encoding
/// @return Number of encoded bytes written, or 0 if insufficient capacity
FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity,
                        const Wave10Lut& lut);

/// @brief Legacy encoding using hardcoded WS2812 LUT (backward compat)
FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity);

/// @brief Calculate required output buffer size for LED encoding
/// @param input_size Input size in bytes (LED pixel data)
/// @return Required output buffer size in bytes
FASTLED_FORCE_INLINE
constexpr size_t calculateUartBufferSize(size_t input_size) {
    return input_size * 4;
}

/// @brief Calculate required output buffer size for RGB LED encoding
/// @param num_leds Number of RGB LEDs
/// @return Required output buffer size in bytes
FASTLED_FORCE_INLINE
constexpr size_t calculateUartBufferSizeForLeds(size_t num_leds) {
    return num_leds * 3 * 4;  // 12 bytes per RGB LED
}

} // namespace fl
