/// @file fl/pins.cpp.hpp
/// Implementation of DigitalMultiWrite8 (nibble LUT bulk pin writes).

#include "fl/pins.h"
#include "fl/fastpin.h"
#include "fl/log.h"

namespace fl {

// ============================================================================
// Pin-to-port mapping using FastPin<N>::port()
// ============================================================================
// Strategy:
// - On platforms with FASTLED_ALL_PINS_VALID (stub, WASM): build a static
//   table of FastPin<0..31>::port() pointers and compare them. Since stub
//   returns nullptr for all, we fall back to pin / 32 grouping.
// - On real platforms (AVR, ESP32, ARM): FastPin<N> has a static_assert for
//   invalid pins, so we can't instantiate all 32 entries. Instead, use the
//   platform's digitalPinToPort() macro which is already available.

#ifdef FASTLED_ALL_PINS_VALID

namespace pin_port_table {

constexpr int TABLE_SIZE = 32;

using port_ptr_t = typename FastPin<0>::port_ptr_t;

template <int N> struct PortEntry {
    static port_ptr_t get() { return FastPin<static_cast<u8>(N)>::port(); }
};

inline const port_ptr_t* portTable() {
    static port_ptr_t table[TABLE_SIZE] = {
        PortEntry<0>::get(),  PortEntry<1>::get(),  PortEntry<2>::get(),  PortEntry<3>::get(),
        PortEntry<4>::get(),  PortEntry<5>::get(),  PortEntry<6>::get(),  PortEntry<7>::get(),
        PortEntry<8>::get(),  PortEntry<9>::get(),  PortEntry<10>::get(), PortEntry<11>::get(),
        PortEntry<12>::get(), PortEntry<13>::get(), PortEntry<14>::get(), PortEntry<15>::get(),
        PortEntry<16>::get(), PortEntry<17>::get(), PortEntry<18>::get(), PortEntry<19>::get(),
        PortEntry<20>::get(), PortEntry<21>::get(), PortEntry<22>::get(), PortEntry<23>::get(),
        PortEntry<24>::get(), PortEntry<25>::get(), PortEntry<26>::get(), PortEntry<27>::get(),
        PortEntry<28>::get(), PortEntry<29>::get(), PortEntry<30>::get(), PortEntry<31>::get(),
    };
    return table;
}

inline port_ptr_t getPortPtr(int pin) {
    if (pin < 0 || pin >= TABLE_SIZE) return nullptr;
    return portTable()[pin];
}

inline bool allPortsNull() {
    const port_ptr_t* table = portTable();
    for (int i = 0; i < TABLE_SIZE; ++i) {
        if (table[i] != nullptr) return false;
    }
    return true;
}

} // namespace pin_port_table

int pinToPort(int pin) {
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

#else // !FASTLED_ALL_PINS_VALID — real platforms (AVR, ESP32, ARM)

int pinToPort(int pin) {
    if (pin < 0) return -1;
    return digitalPinToPort(pin);
}

#endif // FASTLED_ALL_PINS_VALID

void DigitalMultiWrite8::init(const Pins8& pins) {
    for (u8 i = 0; i < 8; ++i) {
        mPins[i] = pins.pins[i];
    }

    // Find the majority port and disable pins on other ports.
    int port_counts[32] = {};
    int port_ids[8] = {};
    int num_active = 0;
    for (u8 i = 0; i < 8; ++i) {
        if (mPins[i] < 0) {
            port_ids[i] = -1;
            continue;
        }
        int p = fl::pinToPort(mPins[i]);
        port_ids[i] = p;
        if (p >= 0 && p < 32) {
            port_counts[p]++;
        }
        num_active++;
    }

    // Find the port with the highest pin count.
    int best_port = -1;
    int best_count = 0;
    for (int p = 0; p < 32; ++p) {
        if (port_counts[p] > best_count) {
            best_count = port_counts[p];
            best_port = p;
        }
    }

    // Disable pins that aren't on the majority port and warn.
    if (best_port >= 0 && best_count < num_active) {
        for (u8 i = 0; i < 8; ++i) {
            if (mPins[i] < 0) {
                continue;
            }
            if (port_ids[i] != best_port) {
                FL_WARN("digitalMultiWrite8: pin "
                        << mPins[i] << " (port " << port_ids[i]
                        << ") disabled — not on majority port "
                        << best_port);
                mPins[i] = -1;
            }
        }
    }

    buildNibbleLut(0, mSetLo, mClrLo); // bits 0-3
    buildNibbleLut(4, mSetHi, mClrHi); // bits 4-7
}

void DigitalMultiWrite8::write(fl::span<const u8> pin_data) const {
    for (fl::size i = 0; i < pin_data.size(); ++i) {
        const u8 byte = pin_data[i];
        const u8 lo_nib = byte & 0x0F;
        const u8 hi_nib = (byte >> 4) & 0x0F;
        applyNibble(mSetLo[lo_nib], mClrLo[lo_nib]);
        applyNibble(mSetHi[hi_nib], mClrHi[hi_nib]);
    }
}

bool DigitalMultiWrite8::allSamePort() const {
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
                                        PinList (&clr_lut)[16]) {
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

void DigitalMultiWrite8::applyNibble(const PinList &set, const PinList &clr) {
    for (u8 i = 0; i < set.count; ++i) {
        fl::digitalWrite(set.pins[i], PinValue::High);
    }
    for (u8 i = 0; i < clr.count; ++i) {
        fl::digitalWrite(clr.pins[i], PinValue::Low);
    }
}

void digitalMultiWrite8(const Pins8& pins, fl::span<const u8> pin_data) {
    DigitalMultiWrite8 writer;
    writer.init(pins);
    writer.write(pin_data);
}

} // namespace fl
