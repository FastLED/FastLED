#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/i2s_ws2812_codec.h
/// @brief WS2812-over-I2S protocol codec for the ESP8266 (host-testable).
///
/// The ESP8266 I2S peripheral is used as a precise bit-pulse generator: the
/// driver streams 16-bit samples at a 3.2 MHz slot rate, so one WS2812 bit
/// (1.25 us) is four I2S bit-slots (312.5 ns each). Each LED bit becomes a
/// four-slot nibble, MSB first:
///
///   '0' -> 0b1000  (T0H ~= 312 ns, inside the 200-500 ns window)
///   '1' -> 0b1100  (T1H ~= 625 ns, inside the 550-850 ns window)
///
/// A 16-bit sample therefore carries four LED bits, so one LED byte is two
/// samples. After the last sample the line idles low (the WS2812 reset/latch).
///
/// `decodeSamples` reverses `encodeByte`, which is how the I2S **RX** loopback
/// self-test validates a transmitted frame (#4943): the RX channel captures the
/// very samples the driver emitted and the decoder reconstructs the LED bytes.
///
/// This header is deliberately free of SDK headers so it is unit-tested on the
/// host (see tests/platforms/esp/8266/i2s_ws2812_codec.cpp), mirroring
/// `spi_hspi_clock.h`.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace esp8266 {

/// WS2812 slot rate: 4 slots per 800 kHz LED bit.
constexpr fl::u32 kI2sSlotHz = 3200000UL;

/// Slots per WS2812 bit.
constexpr fl::u8 kI2sSlotsPerBit = 4;

/// LED bits carried by one 16-bit I2S sample (MSB first).
constexpr fl::u8 kI2sBitsPerSample = static_cast<fl::u8>(16 / kI2sSlotsPerBit);

/// 16-bit samples needed per LED byte (two nibbles of four bits).
constexpr fl::u8 kI2sSamplesPerByte = 2;

/// Four-slot code for one WS2812 bit ('1' = two high slots, '0' = one).
constexpr fl::u8 i2sNibble(bool one) FL_NO_EXCEPT {
    return static_cast<fl::u8>(one ? 0xC : 0x8);
}

/// Number of leading high slots a nibble stands for (1 or 2).
constexpr fl::u8 i2sHighSlots(fl::u8 nibble) FL_NO_EXCEPT {
    return static_cast<fl::u8>(((nibble >> 3) & 1) + ((nibble >> 2) & 1) +
                               ((nibble >> 1) & 1) + (nibble & 1));
}

/// Encode four WS2812 bits (bit3 = first on the wire, MSB first) into a sample.
constexpr fl::u16 encodeFourBits(fl::u8 fourBits) FL_NO_EXCEPT {
    return static_cast<fl::u16>(
        (static_cast<fl::u16>(i2sNibble((fourBits >> 3) & 1)) << 12) |
        (static_cast<fl::u16>(i2sNibble((fourBits >> 2) & 1)) << 8) |
        (static_cast<fl::u16>(i2sNibble((fourBits >> 1) & 1)) << 4) |
        static_cast<fl::u16>(i2sNibble(fourBits & 1)));
}

/// Encode one LED byte into the two I2S samples that carry it.
inline void encodeByte(fl::u8 value, fl::u16* out2) FL_NO_EXCEPT {
    out2[0] = encodeFourBits(static_cast<fl::u8>((value >> 4) & 0x0F));
    out2[1] = encodeFourBits(static_cast<fl::u8>(value & 0x0F));
}

/// Decode four WS2812 bits out of one captured sample (inverse of
/// `encodeFourBits`): a nibble whose high-slot run is >= 2 is a '1' bit.
constexpr fl::u8 decodeFourBits(fl::u16 sample) FL_NO_EXCEPT {
    return static_cast<fl::u8>(
        ((i2sHighSlots(static_cast<fl::u8>((sample >> 12) & 0x0F)) >= 2 ? 1 : 0)
         << 3) |
        ((i2sHighSlots(static_cast<fl::u8>((sample >> 8) & 0x0F)) >= 2 ? 1 : 0)
         << 2) |
        ((i2sHighSlots(static_cast<fl::u8>((sample >> 4) & 0x0F)) >= 2 ? 1 : 0)
         << 1) |
        (i2sHighSlots(static_cast<fl::u8>(sample & 0x0F)) >= 2 ? 1 : 0));
}

/// Decode one LED byte from the two I2S samples that carry it (RX validation).
inline fl::u8 decodeByte(fl::u16 s0, fl::u16 s1) FL_NO_EXCEPT {
    return static_cast<fl::u8>((decodeFourBits(s0) << 4) | decodeFourBits(s1));
}

/// 16-bit I2S samples needed to carry @p ledBytes LED bytes of a frame.
constexpr fl::u32 i2sSamplesForBytes(fl::u32 ledBytes) FL_NO_EXCEPT {
    return ledBytes * kI2sSamplesPerByte;
}

}  // namespace esp8266
}  // namespace fl
