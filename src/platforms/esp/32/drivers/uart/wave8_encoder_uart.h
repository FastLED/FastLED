/// @file wave8_encoder_uart.h
/// @brief Wave8 encoding for UART LED transmission
///
/// This encoder adapts the wave8 encoding pattern for UART framing constraints.
/// Unlike PARLIO's full wave8 expansion (1 LED bit → 8 pulses → 1 byte), UART
/// encoding uses a 2-bit lookup table that encodes LED data in pairs of bits.
///
/// ## Encoding Strategy
///
/// **UART Frame Structure** (8N1):
/// - 1 start bit (LOW) - automatic
/// - 8 data bits
/// - 1 stop bit (HIGH) - automatic
/// - Total: 10 bits per UART byte
///
/// **Encoding Ratio**:
/// - 2 LED bits → 1 UART byte (8 data bits)
/// - 1 LED byte (8 bits) → 4 UART bytes
/// - 1 RGB LED (3 bytes) → 12 UART bytes
///
/// **Why 2-Bit Encoding?**
/// UART's automatic start (LOW) and stop (HIGH) bits provide natural framing,
/// allowing us to encode 2 LED bits per UART byte instead of 1 LED bit per byte.
/// This doubles encoding efficiency compared to simple bit-banging.
///
/// **LUT Design**:
/// ```
/// 2-bit input  → UART data byte → Transmitted waveform (10 bits)
/// 0b00 (0)     → 0x88 (10001000) → [S][0][0][0][1][0][0][0][1][P]
/// 0b01 (1)     → 0x8C (10001100) → [S][0][0][1][1][0][0][0][1][P]
/// 0b10 (2)     → 0xC8 (11001000) → [S][0][0][0][1][0][0][1][1][P]
/// 0b11 (3)     → 0xCC (11001100) → [S][0][0][1][1][0][0][1][1][P]
///                                   ^S=start(LOW)          ^P=stop(HIGH)
/// ```
///
/// These patterns were proven by ESP8266 UART LED implementation (3.2 Mbps).
/// Each pattern provides correct pulse width ratios for WS2812-style protocols.
///
/// ## Buffer Sizing
/// ```cpp
/// size_t output_bytes = input_bytes * 4;
/// size_t rgb_led_bytes = num_leds * 3 * 4;  // 12 bytes per RGB LED
/// ```
///
/// ## Performance
/// - 4 LUT lookups per LED byte (one per 2-bit pair)
/// - 12 LUT lookups per RGB LED
/// - LUT is 4 bytes (ultra cache-friendly)
/// - No transposition needed (UART is single-lane)

#pragma once

// IWYU pragma: private

#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/isr.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

/// @brief 2-bit to UART byte lookup table for LED encoding
///
/// Maps 2 LED bits to 1 UART data byte (8 bits). The UART hardware
/// automatically adds start bit (LOW) and stop bit (HIGH) during transmission.
///
/// **REQUIRES TX LINE INVERSION** (`UART_SIGNAL_TXD_INV` on ESP32).
/// With TX inversion: idle=LOW (correct for WS2812), start bit=HIGH,
/// stop bit=LOW. Each 10-bit UART frame at 4.0 Mbps = 2500ns = exactly
/// 2 WS2812 bit periods (1250ns each).
///
/// Pattern derivation (WS2812 timing at 4.0 Mbps, inverted TX):
/// - Each UART bit = 250ns
/// - Each 10-bit frame = 2500ns = 2 LED bit periods
/// - LED bit "0": 1 HIGH + 4 LOW = T0H=250ns, T0L=1000ns
/// - LED bit "1": 3 HIGH + 2 LOW = T1H=750ns, T1L=500ns
///
/// Inverted wire format: [START=H] [~D0..~D7 LSB-first] [STOP=L]
///
/// ```
/// 2-bit   → UART byte → Inverted wire (10 bits)      → LED timing
/// 0b00(0) → 0xEF      → H L L L L  H L L L L         → "0"(250/1000) "0"(250/1000)
/// 0b01(1) → 0x8F      → H L L L L  H H H L L         → "0"(250/1000) "1"(750/500)
/// 0b10(2) → 0xEC      → H H H L L  H L L L L         → "1"(750/500)  "0"(250/1000)
/// 0b11(3) → 0x8C      → H H H L L  H H H L L         → "1"(750/500)  "1"(750/500)
/// ```
constexpr u8 kUartEncode2BitLUT[4] = {
    0xEF,  // 0b00 → LED "0","0" → wire: H L L L L H L L L L
    0x8F,  // 0b01 → LED "0","1" → wire: H L L L L H H H L L
    0xEC,  // 0b10 → LED "1","0" → wire: H H H L L H L L L L
    0x8C   // 0b11 → LED "1","1" → wire: H H H L L H H H L L
};

/// @brief Encode 2 LED bits to 1 UART byte using LUT
/// @param two_bits 2-bit value (0-3)
/// @return UART data byte (8 bits)
///
/// This function is force-inlined for performance in encoding loops.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
u8 encodeUart2Bits(u8 two_bits) {
    return kUartEncode2BitLUT[two_bits & 0x03];
}

/// @brief Encode 1 LED byte to 4 UART bytes
/// @param led_byte LED data byte (8 bits)
/// @param output Output buffer (must have space for 4 bytes)
///
/// Encodes LED byte in 4 UART bytes by processing 2-bit pairs:
/// - Bits 7-6 → output[0]
/// - Bits 5-4 → output[1]
/// - Bits 3-2 → output[2]
/// - Bits 1-0 → output[3]
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void encodeUartByte(u8 led_byte, u8* output) {
    output[0] = encodeUart2Bits((led_byte >> 6) & 0x03);  // Bits 7-6
    output[1] = encodeUart2Bits((led_byte >> 4) & 0x03);  // Bits 5-4
    output[2] = encodeUart2Bits((led_byte >> 2) & 0x03);  // Bits 3-2
    output[3] = encodeUart2Bits((led_byte >> 0) & 0x03);  // Bits 1-0
}

} // namespace detail

/// @brief Encode LED pixel data to UART bytes using wave8-adapted encoding
///
/// Converts LED pixel data (RGB bytes) into UART transmission bytes using
/// a 2-bit lookup table optimized for UART framing (start/stop bits).
///
/// ## Usage Example
/// ```cpp
/// CRGB leds[100];
/// uint8_t uart_buffer[100 * 3 * 4];  // 12 bytes per LED
///
/// size_t encoded = encodeLedsToUart(
///     reinterpret_cast<const uint8_t*>(leds),
///     100 * 3,
///     uart_buffer,
///     sizeof(uart_buffer)
/// );
/// ```
///
/// ## Encoding Process
/// 1. Each LED byte (8 bits) is split into 4 pairs of 2 bits
/// 2. Each 2-bit pair is encoded to 1 UART byte via LUT
/// 3. Result: 1 LED byte → 4 UART bytes (4:1 expansion)
///
/// ## UART Transmission
/// Each UART byte is transmitted with automatic start/stop bits:
/// - Start bit (LOW): prepended by hardware
/// - 8 data bits: from LUT lookup
/// - Stop bit (HIGH): appended by hardware
///
/// Total bits per UART byte: 10 (1+8+1)
/// Total bits per LED byte: 40 (4 UART bytes × 10 bits)
///
/// @param input LED pixel data (RGB bytes)
/// @param input_size Input size in bytes
/// @param output Output buffer for UART bytes
/// @param output_capacity Output buffer size in bytes
/// @return Number of encoded bytes written, or 0 if insufficient capacity
///
/// @note This function is ISR-safe and can be called from interrupt context
/// @note Output buffer must have capacity for at least (input_size * 4) bytes
FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity);

/// @brief Calculate required output buffer size for LED encoding
/// @param input_size Input size in bytes (LED pixel data)
/// @return Required output buffer size in bytes
///
/// Formula: output_bytes = input_bytes × 4
FASTLED_FORCE_INLINE
constexpr size_t calculateUartBufferSize(size_t input_size) {
    return input_size * 4;
}

/// @brief Calculate required output buffer size for RGB LED encoding
/// @param num_leds Number of RGB LEDs
/// @return Required output buffer size in bytes
///
/// Formula: output_bytes = num_leds × 3 bytes/LED × 4 expansion
FASTLED_FORCE_INLINE
constexpr size_t calculateUartBufferSizeForLeds(size_t num_leds) {
    return num_leds * 3 * 4;  // 12 bytes per RGB LED
}

} // namespace fl
