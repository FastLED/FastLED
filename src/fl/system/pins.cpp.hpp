/// @file fl/pins.cpp.hpp
/// Implementation of DigitalMultiWrite8/16 (bulk pin writes).

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

// Unrolled on purpose. On AVR (avr-gcc 7.3 -Os in simavr, digitalWrite
// replaced by a minimal out-of-line stand-in) writeByte takes 298 cycles with
// all 8 pins active and 58 with none, against 476-484 and 124 for the nibble
// LUTs this replaced, and the cost no longer depends on the data
// (FastLED#5314). Each test is one skip-if-bit instruction on a register.
void writeEight(const int (&pins)[8], u8 high, u8 low) FL_NO_EXCEPT {
    if (high & 0x01) (fl::digitalWrite)(pins[0], PinValue::High);
    if (high & 0x02) (fl::digitalWrite)(pins[1], PinValue::High);
    if (high & 0x04) (fl::digitalWrite)(pins[2], PinValue::High);
    if (high & 0x08) (fl::digitalWrite)(pins[3], PinValue::High);
    if (low & 0x01) (fl::digitalWrite)(pins[0], PinValue::Low);
    if (low & 0x02) (fl::digitalWrite)(pins[1], PinValue::Low);
    if (low & 0x04) (fl::digitalWrite)(pins[2], PinValue::Low);
    if (low & 0x08) (fl::digitalWrite)(pins[3], PinValue::Low);
    if (high & 0x10) (fl::digitalWrite)(pins[4], PinValue::High);
    if (high & 0x20) (fl::digitalWrite)(pins[5], PinValue::High);
    if (high & 0x40) (fl::digitalWrite)(pins[6], PinValue::High);
    if (high & 0x80) (fl::digitalWrite)(pins[7], PinValue::High);
    if (low & 0x10) (fl::digitalWrite)(pins[4], PinValue::Low);
    if (low & 0x20) (fl::digitalWrite)(pins[5], PinValue::Low);
    if (low & 0x40) (fl::digitalWrite)(pins[6], PinValue::Low);
    if (low & 0x80) (fl::digitalWrite)(pins[7], PinValue::Low);
}

} // namespace pins_detail

void DigitalMultiWrite8::init(const Pins8& pins) FL_NO_EXCEPT {
    for (u8 i = 0; i < 8; ++i) {
        mPins[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<8, 32>(mPins, "digitalMultiWrite8");

    mActive = 0;
    for (u8 i = 0; i < 8; ++i) {
        if (mPins[i] >= 0) {
            mActive = static_cast<u8>(mActive | (1u << i));
        }
    }
}

void DigitalMultiWrite8::write(fl::span<const u8> pin_data) const FL_NO_EXCEPT {
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        writeByte(pin_data[i]);
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

// Without LUTs the writer is 17 B on AVR (36 B on 32-bit), so the one-shot
// function can build one on the stack again (FastLED#5307, #5314).
void digitalMultiWrite8(const Pins8& pins, fl::span<const u8> pin_data) FL_NO_EXCEPT {
    DigitalMultiWrite8 writer;
    writer.init(pins);
    writer.write(pin_data);
}

// ============================================================================
// DigitalMultiWrite16
// ============================================================================

void DigitalMultiWrite16::init(const Pins16& pins) FL_NO_EXCEPT {
    int all[16];
    for (u8 i = 0; i < 16; ++i) {
        all[i] = pins.pins[i];
    }
    pins_detail::keepMajorityPort<16, 64>(all, "digitalMultiWrite16");

    mActive = 0;
    for (u8 i = 0; i < 16; ++i) {
        mPins[i / 8][i % 8] = all[i];
        if (all[i] >= 0) {
            mActive = static_cast<u16>(mActive | (1u << i));
        }
    }
}

void DigitalMultiWrite16::write(fl::span<const u16> pin_data) const FL_NO_EXCEPT {
    const u8 active_lo = static_cast<u8>(mActive & 0xFF);
    const u8 active_hi = static_cast<u8>(mActive >> 8);
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u16 word = pin_data[i];
        const u8 lo = static_cast<u8>(word & 0xFF);
        const u8 hi = static_cast<u8>(word >> 8);
        pins_detail::writeEight(mPins[0], static_cast<u8>(lo & active_lo),
                                static_cast<u8>(~lo & active_lo));
        pins_detail::writeEight(mPins[1], static_cast<u8>(hi & active_hi),
                                static_cast<u8>(~hi & active_hi));
    }
}

bool DigitalMultiWrite16::allSamePort() const FL_NO_EXCEPT {
    int first_port = -1;
    for (u8 i = 0; i < 16; ++i) {
        const int pin = mPins[i / 8][i % 8];
        if (pin < 0) {
            continue;
        }
        int port = fl::pinToPort(pin);
        if (first_port < 0) {
            first_port = port;
        } else if (port != first_port) {
            return false;
        }
    }
    return true;
}

void digitalMultiWrite16(const Pins16& pins, fl::span<const u16> pin_data) FL_NO_EXCEPT {
    DigitalMultiWrite16 writer;
    writer.init(pins);
    writer.write(pin_data);
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
