/// @file lpuart_encoder.h
/// @brief Wave8 encoder for WS2812 via LPUART (TXINV semantics).
///
/// Pure-function header — no Teensy hardware dependency, so it links
/// cleanly into both the Teensy build (where lpuart_driver.cpp.hpp
/// calls it) and the host unit-test build (where the encoder test in
/// `tests/platforms/.../lpuart_encoder.cpp` exercises the math).
///
/// See `lpuart_driver.h` for the full derivation of the wave8 table.
/// Summary:
///
///   At 4 Mbps with LPUART CTRL.TXINV=1, each UART byte takes 10 wire
///   bits (start + 8 data + stop) = 2.5 us. We pack 2 WS2812 bits per
///   UART byte, 5 wire bits per WS2812 bit.
///
///   '0' WS bit on the wire = H L L L L (250 ns HIGH + 1000 ns LOW)
///   '1' WS bit on the wire = H H H L L (750 ns HIGH + 500 ns LOW)
///
///   Mapping pairs to UART bytes (low nibble = first WS bit, high
///   nibble = second WS bit):
///
///     (WS0, WS1) = (0,0) -> 0xEF
///     (WS0, WS1) = (0,1) -> 0x8F
///     (WS0, WS1) = (1,0) -> 0xEC
///     (WS0, WS1) = (1,1) -> 0x8C
///
///   WS2812 streams MSB first within each byte, so a source byte's
///   bits 7,6 form the first emitted pair, bits 5,4 the second, etc.

#pragma once

#include "fl/stl/stdint.h"

namespace fl {

/// @brief Look up the UART byte for one WS2812 bit pair.
/// @param pair Two-bit field (WS0 in bit 1, WS1 in bit 0).
inline u8 lpuart_pair_to_byte(u8 pair) {
    static constexpr u8 kTable[4] = { 0xEF, 0x8F, 0xEC, 0x8C };
    return kTable[pair & 0x3];
}

/// @brief Encode one raw WS2812 byte into 4 UART bytes (MSB-first).
inline void lpuart_encode_byte(u8 b, u8* out4) {
    out4[0] = lpuart_pair_to_byte((b >> 6) & 0x3);  // bits 7,6
    out4[1] = lpuart_pair_to_byte((b >> 4) & 0x3);  // bits 5,4
    out4[2] = lpuart_pair_to_byte((b >> 2) & 0x3);  // bits 3,2
    out4[3] = lpuart_pair_to_byte((b >> 0) & 0x3);  // bits 1,0
}

} // namespace fl
