/// @file fl/pins.h
/// Bulk pin write API for setting multiple pins simultaneously.
///
/// DigitalMultiWrite8 groups up to 8 pins and DigitalMultiWrite16 groups
/// up to 16 pins. Both keep only the pin numbers and an active-pin mask, and
/// turn each data value into HIGH/LOW masks with two AND operations.

#pragma once

#include "fl/system/pin.h"  // IWYU pragma: keep
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// POD struct holding 8 pin numbers for bulk pin writes.
/// Use -1 for any pin position you want to skip.
struct Pins8 {
    int pins[8];
};

namespace pins_detail {
/// Write 8 pins: `pins[b]` goes HIGH when bit b of @p high is set, LOW when
/// bit b of @p low is set, and is untouched otherwise. Per 4-pin group in
/// ascending order, every HIGH goes first, then every LOW, each in ascending
/// bit order. Takes an array reference rather than `fl::span<const int, 8>`:
/// span has a user-provided copy constructor, so the ABI would pass it through
/// memory on this timing-critical call.
void writeEight(const int (&pins)[8], u8 high, u8 low) FL_NO_EXCEPT;
} // namespace pins_detail

/// Fast 8-pin digital writes.
///
/// Call init() once with your 8 pin numbers (-1 to skip a position), then
/// call write() or writeByte() in your hot loop. Each data byte is ANDed with
/// the active-pin mask to get the pins to drive HIGH and LOW, which are then
/// written in a fixed, unrolled order. The object holds only the pins and the
/// mask (17 B on AVR, 36 B on 32-bit targets); the per-nibble LUTs it used to
/// carry were 592 B / ~1.3 KB and made writeByte slower on AVR (FastLED#5314).
class DigitalMultiWrite8 {
  public:
    DigitalMultiWrite8() FL_NO_EXCEPT = default;

    /// Initialize from 8 pin numbers. -1 means "skip this bit position".
    void init(const Pins8& pins) FL_NO_EXCEPT;

    /// Write pre-transposed byte data to the 8 pins.
    ///
    /// Each byte in @p pin_data is a bitmask where bit N → pin N.
    /// 1 = HIGH, 0 = LOW. Pins set to -1 during init() are skipped.
    void write(fl::span<const u8> pin_data) const FL_NO_EXCEPT;

    /// Write a single bitmask byte to the 8 pins.
    ///
    /// Same as write() but optimized for the single-byte case in
    /// timing-critical hot loops (clockless bit-banging). Avoids span
    /// iteration overhead: two ANDs inline, then one out-of-line call.
    void writeByte(u8 byte) const FL_NO_EXCEPT {
        pins_detail::writeEight(mPins, static_cast<u8>(byte & mActive),
                                static_cast<u8>(~byte & mActive));
    }

    /// Check whether all active pins (non -1) share the same GPIO port.
    /// Returns true if fewer than 2 active pins exist (trivially same port).
    bool allSamePort() const FL_NO_EXCEPT;

  private:
    int mPins[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    u8 mActive = 0;  // bit N set when mPins[N] is driven
};

/// Convenience free function — creates a temporary DigitalMultiWrite8,
/// initializes it, and writes. For repeated writes, prefer the class
/// directly so the majority-port check in init() runs once.
void digitalMultiWrite8(const Pins8& pins, fl::span<const u8> pin_data) FL_NO_EXCEPT;

/// POD struct holding 16 pin numbers for bulk pin writes.
/// Use -1 for any pin position you want to skip.
struct Pins16 {
    int pins[16];
};

/// Fast 16-pin digital writes.
///
/// Call init() once with your 16 pin numbers (-1 to skip a position), then
/// call write() in your hot loop. Each u16 is written as two bytes, low byte
/// first, the same way DigitalMultiWrite8 writes one. The object holds only
/// the pins and the active-pin mask (34 B on AVR, 68 B on 32-bit targets).
class DigitalMultiWrite16 {
  public:
    DigitalMultiWrite16() FL_NO_EXCEPT = default;

    /// Initialize from 16 pin numbers. -1 means "skip this bit position".
    void init(const Pins16& pins) FL_NO_EXCEPT;

    /// Write pre-transposed 16-bit data to the 16 pins.
    ///
    /// Each u16 in @p pin_data is a bitmask where bit N → pin N.
    /// 1 = HIGH, 0 = LOW. Pins set to -1 during init() are skipped.
    void write(fl::span<const u16> pin_data) const FL_NO_EXCEPT;

    /// Check whether all active pins (non -1) share the same GPIO port.
    /// Returns true if fewer than 2 active pins exist (trivially same port).
    bool allSamePort() const FL_NO_EXCEPT;

  private:
    // Pin N is mPins[N / 8][N % 8]: two 8-pin halves for writeEight().
    int mPins[2][8] = {{-1, -1, -1, -1, -1, -1, -1, -1},
                       {-1, -1, -1, -1, -1, -1, -1, -1}};
    u16 mActive = 0;  // bit N set when pin N is driven
};

/// Convenience free function — creates a temporary DigitalMultiWrite16,
/// initializes it, and writes. For repeated writes, prefer the class
/// directly so the majority-port check in init() runs once.
void digitalMultiWrite16(const Pins16& pins, fl::span<const u16> pin_data) FL_NO_EXCEPT;

/// Pin number with its resolved port ID.
struct PinInfo {
    int pin = -1;
    int port = -1;
};

/// Map a runtime pin number to an integer port ID using FastPin<N>::port().
/// Pins that share the same GPIO port register return the same ID.
/// @param pin Pin number (0-63)
/// @return Integer port ID (0-based), or -1 for out-of-range pins
int pinToPort(int pin) FL_NO_EXCEPT;

/// Resolve port IDs for an array of PinInfo in-place.
/// For each entry, sets port = pinToPort(pin). Skips entries where pin < 0.
void pinMap(fl::span<PinInfo> pins) FL_NO_EXCEPT;

} // namespace fl
