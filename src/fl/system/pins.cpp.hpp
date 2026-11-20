/// @file fl/pins.cpp.hpp
/// Implementation of DigitalMultiWrite8 (nibble LUT bulk pin writes).

#include "fl/system/pins.h"
#include "fl/system/fastpin.h"
#include "fl/log/log.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// Pin-to-port mapping using FastPin<N>::port()
// ============================================================================
// Strategy:
// - On platforms with FASTLED_ALL_PINS_VALID (stub, WASM): build a static
//   table of FastPin<0..31>::port() pointers and compare them. Since stub
//   returns nullptr for all, we fall back to pin / 32 grouping.
// - On platforms with FASTLED_NO_PINMAP (Apollo3, Renesas, Giga, NRF51,
//   STM32, RP2040): digitalPinToPort() is unavailable, so fall back to
//   grouping pins by blocks of 32.
// - On real platforms with pin maps (AVR, SAM): use the platform's
//   digitalPinToPort() macro which is already available.

#ifdef FASTLED_ALL_PINS_VALID

namespace pin_port_table {

constexpr int TABLE_SIZE = 64;

using port_ptr_t = typename FastPin<0>::port_ptr_t;

template <int N> struct PortEntry {
    static port_ptr_t get() FL_NO_EXCEPT { return FastPin<static_cast<u8>(N)>::port(); }
};

inline const port_ptr_t* portTable() FL_NO_EXCEPT {
    static port_ptr_t table[TABLE_SIZE] = {
        PortEntry<0>::get(),  PortEntry<1>::get(),  PortEntry<2>::get(),  PortEntry<3>::get(),
        PortEntry<4>::get(),  PortEntry<5>::get(),  PortEntry<6>::get(),  PortEntry<7>::get(),
        PortEntry<8>::get(),  PortEntry<9>::get(),  PortEntry<10>::get(), PortEntry<11>::get(),
        PortEntry<12>::get(), PortEntry<13>::get(), PortEntry<14>::get(), PortEntry<15>::get(),
        PortEntry<16>::get(), PortEntry<17>::get(), PortEntry<18>::get(), PortEntry<19>::get(),
        PortEntry<20>::get(), PortEntry<21>::get(), PortEntry<22>::get(), PortEntry<23>::get(),
        PortEntry<24>::get(), PortEntry<25>::get(), PortEntry<26>::get(), PortEntry<27>::get(),
        PortEntry<28>::get(), PortEntry<29>::get(), PortEntry<30>::get(), PortEntry<31>::get(),
        PortEntry<32>::get(), PortEntry<33>::get(), PortEntry<34>::get(), PortEntry<35>::get(),
        PortEntry<36>::get(), PortEntry<37>::get(), PortEntry<38>::get(), PortEntry<39>::get(),
        PortEntry<40>::get(), PortEntry<41>::get(), PortEntry<42>::get(), PortEntry<43>::get(),
        PortEntry<44>::get(), PortEntry<45>::get(), PortEntry<46>::get(), PortEntry<47>::get(),
        PortEntry<48>::get(), PortEntry<49>::get(), PortEntry<50>::get(), PortEntry<51>::get(),
        PortEntry<52>::get(), PortEntry<53>::get(), PortEntry<54>::get(), PortEntry<55>::get(),
        PortEntry<56>::get(), PortEntry<57>::get(), PortEntry<58>::get(), PortEntry<59>::get(),
        PortEntry<60>::get(), PortEntry<61>::get(), PortEntry<62>::get(), PortEntry<63>::get(),
    };
    return table;
}

inline port_ptr_t getPortPtr(int pin) FL_NO_EXCEPT {
    if (pin < 0 || pin >= TABLE_SIZE) return nullptr;
    return portTable()[pin];
}

inline bool allPortsNull() FL_NO_EXCEPT {
    const port_ptr_t* table = portTable();
    for (int i = 0; i < TABLE_SIZE; ++i) {
        if (table[i] != nullptr) return false;
    }
    return true;
}

} // namespace pin_port_table

int pinToPort(int pin) FL_NO_EXCEPT {
    if (pin < 0) return -1;

    if (!pin_port_table::allPortsNull()) {
        auto ptr = pin_port_table::getPortPtr(pin);
        if (ptr == nullptr) return -1;

        // Assign sequential port IDs by counting unique pointers before this one
        const auto* table = pin_port_table::portTable();
        for (int i = 0; i < pin_port_table::TABLE_SIZE; ++i) {
            if (table[i] == ptr) {
                int id = 0;
                for (int j = 0; j < i; ++j) {
                    bool unique = true;
                    for (int k = 0; k < j; ++k) {
                        if (table[k] == table[j]) { unique = false; break; }
                    }
                    if (unique && table[j] != nullptr) ++id;
                }
                return id;
            }
        }
        return -1;
    }

    // Stub/WASM fallback: all FastPin ports are null, group by pin / 32
    return pin / 32;
}

#elif defined(FASTLED_NO_PINMAP) || defined(FL_IS_ESP32)

// Platforms that define FASTLED_NO_PINMAP (Apollo3, Renesas, Giga, NRF51,
// STM32, RP2040) do not provide digitalPinToPort(). ESP32 uses native GPIO
// banks without Arduino: group pins by blocks of 32 on these platforms.
int pinToPort(int pin) FL_NO_EXCEPT {
    if (pin < 0) return -1;
    return pin / 32;
}

#else // Real platforms with pin maps (AVR, SAM, etc.)

namespace {
// Convert digitalPinToPort() result to int. Most platforms return integers;
// SAM3X8E returns Pio* which needs mapping to sequential port IDs.
inline int portValueToId(unsigned char v) FL_NO_EXCEPT { return v; }
inline int portValueToId(unsigned short v) FL_NO_EXCEPT { return static_cast<int>(v); }
inline int portValueToId(int v) FL_NO_EXCEPT { return v; }
inline int portValueToId(long v) FL_NO_EXCEPT { return static_cast<int>(v); }

// Template overload for unsigned multi-byte integers (handles both u32 and unsigned long)
// Uses enable_if to avoid duplicate overloads on platforms where they're the same type
template<typename T>
typename fl::enable_if<fl::is_multi_byte_integer<T>::value &&
                      !fl::is_signed<T>::value,
                      int>::type
portValueToId(T v) FL_NO_EXCEPT {
    return static_cast<int>(v);
}

// Pointer overload for SAM and similar platforms where digitalPinToPort
// returns a peripheral struct pointer (e.g. Pio*).
template <typename T>
int portValueToId(T* ptr) FL_NO_EXCEPT {
    if (!ptr) return -1;
    static T* seen[8] = {};
    static int n = 0;
    for (int i = 0; i < n; ++i) {
        if (seen[i] == ptr) return i;
    }
    if (n < 8) {
        seen[n] = ptr;
        return n++;
    }
    return -1;
}
} // namespace

int pinToPort(int pin) FL_NO_EXCEPT {
    if (pin < 0) return -1;
    return portValueToId(digitalPinToPort(pin));
}

#endif // FASTLED_ALL_PINS_VALID

namespace pins_detail {

// Disable (set to -1) every active pin that is not on the port most active
// pins share, warning for each. Ports outside [0, kPortLimit) never win, and
// ties go to the lowest port id. Counting pairwise over the N <= 16 pins
// gives the same answer as a per-port histogram without the kPortLimit-entry
// counter array in the frame (FastLED#5307).
template <u8 N, int kPortLimit>
void keepMajorityPort(int (&pins)[N], const char* who) FL_NO_EXCEPT {
    int port_ids[N];
    int num_active = 0;
    for (u8 i = 0; i < N; ++i) {
        if (pins[i] < 0) {
            port_ids[i] = -1;
            continue;
        }
        port_ids[i] = fl::pinToPort(pins[i]);
        num_active++;
    }

    int best_port = -1;
    int best_count = 0;
    for (u8 i = 0; i < N; ++i) {
        const int p = port_ids[i];
        if (pins[i] < 0 || p < 0 || p >= kPortLimit) {
            continue;
        }
        int count = 0;
        for (u8 j = 0; j < N; ++j) {
            if (pins[j] >= 0 && port_ids[j] == p) {
                count++;
            }
        }
        if (count > best_count || (count == best_count && p < best_port)) {
            best_count = count;
            best_port = p;
        }
    }

    if (best_port >= 0 && best_count < num_active) {
        for (u8 i = 0; i < N; ++i) {
            if (pins[i] < 0) {
                continue;
            }
            if (port_ids[i] != best_port) {
                FL_WARN(who << ": pin " << pins[i] << " (port " << port_ids[i] << ") disabled - not on majority port " << best_port);
                pins[i] = -1;
            }
        }
    }
}

// Drive one 4-pin group from a nibble without a LUT: every HIGH first, then
// every LOW, each in ascending bit order. That is exactly the digitalWrite
// sequence the classes' nibble LUTs replay.
inline void writeNibbleDirect(fl::span<const int, 4> pins4, u8 nib) FL_NO_EXCEPT {
    for (u8 b = 0; b < 4; ++b) {
        if (pins4[b] >= 0 && (nib & (1 << b))) {
            (fl::digitalWrite)(pins4[b], PinValue::High);
        }
    }
    for (u8 b = 0; b < 4; ++b) {
        if (pins4[b] >= 0 && !(nib & (1 << b))) {
            (fl::digitalWrite)(pins4[b], PinValue::Low);
        }
    }
}

} // namespace pins_detail

void DigitalMultiWrite8::init(const Pins8& pins) FL_NO_EXCEPT {
    for (u8 i = 0; i < 8; ++i) {
        mPins[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<8, 32>(mPins, "digitalMultiWrite8");

    buildNibbleLut(0, mSetLo, mClrLo); // bits 0-3
    buildNibbleLut(4, mSetHi, mClrHi); // bits 4-7
}

void DigitalMultiWrite8::write(fl::span<const u8> pin_data) const FL_NO_EXCEPT {
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u8 byte = pin_data[i];
        const u8 lo_nib = byte & 0x0F;
        const u8 hi_nib = (byte >> 4) & 0x0F;
        applyNibble(mSetLo[lo_nib], mClrLo[lo_nib]);
        applyNibble(mSetHi[hi_nib], mClrHi[hi_nib]);
    }
}

bool DigitalMultiWrite8::allSamePort() const FL_NO_EXCEPT {
    int first_port = -1;
    for (u8 i = 0; i < 8; ++i) {
        if (mPins[i] < 0) {
            continue;
        }
        int port = fl::pinToPort(mPins[i]);
        if (first_port < 0) {
            first_port = port;
        } else if (port != first_port) {
            return false;
        }
    }
    return true;
}

void DigitalMultiWrite8::buildNibbleLut(u8 bit_offset, PinList (&set_lut)[16],
                                        PinList (&clr_lut)[16]) FL_NO_EXCEPT {
    for (u16 nib = 0; nib < 16; ++nib) {
        PinList &s = set_lut[nib];
        PinList &c = clr_lut[nib];
        s.count = 0;
        c.count = 0;
        for (u8 b = 0; b < 4; ++b) {
            int pin = mPins[bit_offset + b];
            if (pin < 0) {
                continue;
            }
            if (nib & (1 << b)) {
                s.pins[s.count++] = pin;
            } else {
                c.pins[c.count++] = pin;
            }
        }
    }
}

void DigitalMultiWrite8::applyNibble(const PinList &set, const PinList &clr) FL_NO_EXCEPT {
    for (u8 i = 0; i < set.count; ++i) {
        (fl::digitalWrite)(set.pins[i], PinValue::High);
    }
    for (u8 i = 0; i < clr.count; ++i) {
        (fl::digitalWrite)(clr.pins[i], PinValue::Low);
    }
}

// The one-shot free functions drive the pins directly instead of building a
// DigitalMultiWrite8/16 on the stack: the classes' nibble LUTs (592 / 1184 B
// on AVR) only pay off across repeated writes. Same pins disabled, same
// digitalWrite sequence (FastLED#5307).
void digitalMultiWrite8(const Pins8& pins, fl::span<const u8> pin_data) FL_NO_EXCEPT {
    int active[8];
    for (u8 i = 0; i < 8; ++i) {
        active[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<8, 32>(active, "digitalMultiWrite8");
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u8 byte = pin_data[i];
        pins_detail::writeNibbleDirect(fl::span<const int, 4>(active, 4), byte & 0x0F);
        pins_detail::writeNibbleDirect(fl::span<const int, 4>(active + 4, 4), (byte >> 4) & 0x0F);
    }
}

// ============================================================================
// DigitalMultiWrite16
// ============================================================================

void DigitalMultiWrite16::init(const Pins16& pins) FL_NO_EXCEPT {
    for (u8 i = 0; i < 16; ++i) {
        mPins[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<16, 64>(mPins, "digitalMultiWrite16");

    for (u8 n = 0; n < 4; ++n) {
        buildNibbleLut(n * 4, mSetNib[n], mClrNib[n]);
    }
}

void DigitalMultiWrite16::write(fl::span<const u16> pin_data) const FL_NO_EXCEPT {
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u16 word = pin_data[i];
        for (u8 n = 0; n < 4; ++n) {
            const u8 nib = (word >> (n * 4)) & 0x0F;
            applyNibble(mSetNib[n][nib], mClrNib[n][nib]);
        }
    }
}

bool DigitalMultiWrite16::allSamePort() const FL_NO_EXCEPT {
    int first_port = -1;
    for (u8 i = 0; i < 16; ++i) {
        if (mPins[i] < 0) {
            continue;
        }
        int port = fl::pinToPort(mPins[i]);
        if (first_port < 0) {
            first_port = port;
        } else if (port != first_port) {
            return false;
        }
    }
    return true;
}

void DigitalMultiWrite16::buildNibbleLut(u8 bit_offset,
                                          PinList (&set_lut)[16],
                                          PinList (&clr_lut)[16]) FL_NO_EXCEPT {
    for (u16 nib = 0; nib < 16; ++nib) {
        PinList &s = set_lut[nib];
        PinList &c = clr_lut[nib];
        s.count = 0;
        c.count = 0;
        for (u8 b = 0; b < 4; ++b) {
            int pin = mPins[bit_offset + b];
            if (pin < 0) {
                continue;
            }
            if (nib & (1 << b)) {
                s.pins[s.count++] = pin;
            } else {
                c.pins[c.count++] = pin;
            }
        }
    }
}

void DigitalMultiWrite16::applyNibble(const PinList &set, const PinList &clr) FL_NO_EXCEPT {
    for (u8 i = 0; i < set.count; ++i) {
        (fl::digitalWrite)(set.pins[i], PinValue::High);
    }
    for (u8 i = 0; i < clr.count; ++i) {
        (fl::digitalWrite)(clr.pins[i], PinValue::Low);
    }
}

void digitalMultiWrite16(const Pins16& pins, fl::span<const u16> pin_data) FL_NO_EXCEPT {
    int active[16];
    for (u8 i = 0; i < 16; ++i) {
        active[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<16, 64>(active, "digitalMultiWrite16");
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u16 word = pin_data[i];
        for (u8 n = 0; n < 4; ++n) {
            pins_detail::writeNibbleDirect(fl::span<const int, 4>(active + n * 4, 4),
                                           (word >> (n * 4)) & 0x0F);
        }
    }
}

// ============================================================================
// pinMap
// ============================================================================

void pinMap(fl::span<PinInfo> pins) FL_NO_EXCEPT {
    for (fl::size i = 0; i < pins.size(); ++i) {
        if (pins[i].pin >= 0) {
            pins[i].port = pinToPort(pins[i].pin);
        }
    }
}

} // namespace fl
