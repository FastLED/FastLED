/// @file fl/pins.h
/// Bulk pin write API for setting multiple pins simultaneously.
///
/// DigitalMultiWrite8 groups up to 8 pins and DigitalMultiWrite16 groups
/// up to 16 pins. Both use nibble LUT tables to convert data into set/clear
/// masks without per-bit branching in the hot loop.

#pragma once

#include "fl/pin.h"  // IWYU pragma: keep
#include "fl/stl/span.h"

namespace fl {

/// POD struct holding 8 pin numbers for bulk pin writes.
/// Use -1 for any pin position you want to skip.
struct Pins8 {
    int pins[8];
};

/// Pre-computed nibble LUT for fast 8-pin digital writes.
///
/// Call init() once with your 8 pin numbers (-1 to skip a position), then
/// call write() in your hot loop. Each byte in the data stream is split
/// into two 4-bit nibbles; each nibble indexes into a 16-entry LUT that
/// yields the pin indices to set HIGH or LOW — no per-bit branching.
class DigitalMultiWrite8 {
  public:
    DigitalMultiWrite8() = default;

    /// Initialize from 8 pin numbers. -1 means "skip this bit position".
    void init(const Pins8& pins);

    /// Write pre-transposed byte data to the 8 pins.
    ///
    /// Each byte in @p pin_data is a bitmask where bit N → pin N.
    /// 1 = HIGH, 0 = LOW. Pins set to -1 during init() are skipped.
    void write(fl::span<const u8> pin_data) const;

    /// Write a single bitmask byte to the 8 pins.
    ///
    /// Same as write() but optimized for the single-byte case in
    /// timing-critical hot loops (clockless bit-banging). Avoids span
    /// iteration overhead — inlines directly to two nibble LUT lookups.
    void writeByte(u8 byte) const {
        const u8 lo_nib = byte & 0x0F;
        const u8 hi_nib = (byte >> 4) & 0x0F;
        applyNibble(mSetLo[lo_nib], mClrLo[lo_nib]);
        applyNibble(mSetHi[hi_nib], mClrHi[hi_nib]);
    }

    /// Check whether all active pins (non -1) share the same GPIO port.
    /// Returns true if fewer than 2 active pins exist (trivially same port).
    bool allSamePort() const;

  private:
    // Each LUT entry stores up to 4 pin indices to set or clear.
    struct PinList {
        int pins[4];
        u8 count;
    };

    void buildNibbleLut(u8 bit_offset, PinList (&set_lut)[16],
                        PinList (&clr_lut)[16]);

    static void applyNibble(const PinList &set, const PinList &clr);

    int mPins[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    PinList mSetLo[16] = {};
    PinList mClrLo[16] = {};
    PinList mSetHi[16] = {};
    PinList mClrHi[16] = {};
};

/// Convenience free function — creates a temporary DigitalMultiWrite8,
/// initializes it, and writes. For repeated writes, prefer the class
/// directly to amortize the LUT build cost.
void digitalMultiWrite8(const Pins8& pins, fl::span<const u8> pin_data);

/// POD struct holding 16 pin numbers for bulk pin writes.
/// Use -1 for any pin position you want to skip.
struct Pins16 {
    int pins[16];
};

/// Pre-computed nibble LUT for fast 16-pin digital writes.
///
/// Call init() once with your 16 pin numbers (-1 to skip a position), then
/// call write() in your hot loop. Each u16 in the data stream is split
/// into four 4-bit nibbles; each nibble indexes into a 16-entry LUT that
/// yields the pin indices to set HIGH or LOW — no per-bit branching.
class DigitalMultiWrite16 {
  public:
    DigitalMultiWrite16() = default;

    /// Initialize from 16 pin numbers. -1 means "skip this bit position".
    void init(const Pins16& pins);

    /// Write pre-transposed 16-bit data to the 16 pins.
    ///
    /// Each u16 in @p pin_data is a bitmask where bit N → pin N.
    /// 1 = HIGH, 0 = LOW. Pins set to -1 during init() are skipped.
    void write(fl::span<const u16> pin_data) const;

    /// Check whether all active pins (non -1) share the same GPIO port.
    /// Returns true if fewer than 2 active pins exist (trivially same port).
    bool allSamePort() const;

  private:
    // Each LUT entry stores up to 4 pin indices to set or clear.
    struct PinList {
        int pins[4];
        u8 count;
    };

    void buildNibbleLut(u8 bit_offset, PinList (&set_lut)[16],
                        PinList (&clr_lut)[16]);

    static void applyNibble(const PinList &set, const PinList &clr);

    int mPins[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    PinList mSetNib[4][16] = {};  // nibbles 0-3 (bits 0-3, 4-7, 8-11, 12-15)
    PinList mClrNib[4][16] = {};
};

/// Convenience free function — creates a temporary DigitalMultiWrite16,
/// initializes it, and writes. For repeated writes, prefer the class
/// directly to amortize the LUT build cost.
void digitalMultiWrite16(const Pins16& pins, fl::span<const u16> pin_data);

/// Pin number with its resolved port ID.
struct PinInfo {
    int pin = -1;
    int port = -1;
};

/// Map a runtime pin number to an integer port ID using FastPin<N>::port().
/// Pins that share the same GPIO port register return the same ID.
/// @param pin Pin number (0-63)
/// @return Integer port ID (0-based), or -1 for out-of-range pins
int pinToPort(int pin);

/// Resolve port IDs for an array of PinInfo in-place.
/// For each entry, sets port = pinToPort(pin). Skips entries where pin < 0.
void pinMap(fl::span<PinInfo> pins);

} // namespace fl
