#pragma once

/**
 * FastPins implementation for Teensy 3.x (Kinetis K20/K64/K66/KL26)
 *
 * Teensy 3.x GPIO Architecture:
 * - Freescale/NXP Kinetis MCUs (ARM Cortex-M4 for K20/K64/K66, ARM Cortex-M0+ for KL26)
 * - 5 GPIO ports: GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
 * - Atomic set/clear registers: PSOR (set), PCOR (clear)
 * - Direct output register: PDOR (read-modify-write)
 *
 * Register Map (Kinetis K20/K64/K66):
 * GPIOx (x = A/B/C/D/E):
 *   ->PDOR   - Port Data Output Register (read/write current state)
 *   ->PSOR   - Port Set Output Register (write 1 to set HIGH, write 0 has no effect)
 *   ->PCOR   - Port Clear Output Register (write 1 to clear LOW, write 0 has no effect)
 *   ->PTOR   - Port Toggle Output Register (write 1 to toggle)
 *   ->PDIR   - Port Data Input Register (read input state)
 *   ->PDDR   - Port Data Direction Register (1 = output, 0 = input)
 *
 * Pin Encoding:
 * - Teensy uses direct Arduino pin numbers (0-33 for Teensy 3.0/3.1/3.2, 0-63 for Teensy 3.6)
 * - Pin-to-port mapping is board-specific (defined in fastpin_arm_k20.h / fastpin_arm_k66.h)
 * - Runtime detection uses existing FastPin<> infrastructure (compile-time pin mapping)
 *
 * Performance:
 * - FastPinsSamePort<>::write(): ~25-30ns (PSOR + PCOR atomic writes)
 * - FastPinsWithClock<>::writeWithClockStrobe(): ~40ns (data + clock strobe)
 * - FastPins<> (multi-port): ~60-120ns (sequential port writes)
 *
 * Platform Support:
 * - Teensy 3.0 (MK20DX128) - Cortex-M4 @ 48 MHz, 34 pins
 * - Teensy 3.1/3.2 (MK20DX256) - Cortex-M4 @ 72 MHz, 34 pins
 * - Teensy 3.5 (MK64FX512) - Cortex-M4F @ 120 MHz, 62 pins
 * - Teensy 3.6 (MK66FX1M0) - Cortex-M4F @ 180 MHz, 64 pins
 * - Teensy LC (MKL26Z64) - Cortex-M0+ @ 48 MHz, 27 pins
 */

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__MKL26Z64__) || defined(FASTLED_TEENSY3)

#include "fl/stdint.h"
#include "fl/warn.h"

// Include existing FastPin definitions to access compile-time pin info
#if defined(__MK20DX128__) || defined(__MK20DX256__)
    #include "teensy31_32/fastpin_arm_k20.h"
#elif defined(__MK66FX1M0__)
    #include "teensy36/fastpin_arm_k66.h"
#elif defined(__MK64FX512__)
    // Teensy 3.5 uses similar pin definitions to K66
    #include "teensy36/fastpin_arm_k66.h"
#elif defined(__MKL26Z64__)
    #include "teensy_lc/fastpin_arm_kl26.h"
#endif

namespace fl {

namespace detail {

/// Helper to get GPIO port SET register address for a Teensy 3.x pin
/// Uses FastPin<>::sport() to get the correct PSOR register
template<uint8_t PIN>
inline volatile uint32_t* getTeensy3SetReg() {
    return (volatile uint32_t*)FastPin<PIN>::sport();
}

/// Helper to get GPIO port CLEAR register address for a Teensy 3.x pin
/// Uses FastPin<>::cport() to get the correct PCOR register
template<uint8_t PIN>
inline volatile uint32_t* getTeensy3ClearReg() {
    return (volatile uint32_t*)FastPin<PIN>::cport();
}

/// Helper to get pin bit mask for a Teensy 3.x pin
/// Uses FastPin<>::mask() to get the correct bit mask within the GPIO port
template<uint8_t PIN>
inline uint32_t getTeensy3Mask() {
    return FastPin<PIN>::mask();
}

/// Runtime pin info lookup using switch statement
/// Returns GPIO port SET/CLEAR register addresses and bit mask for given pin number
struct PinInfo {
    volatile uint32_t* set_reg;
    volatile uint32_t* clear_reg;
    uint32_t mask;
};

inline PinInfo getPinInfo(uint8_t pin) {
    PinInfo info = {nullptr, nullptr, 0};

    // Use switch to map pin number to FastPin template instantiation
    // This bridges runtime pin numbers to compile-time FastPin<PIN> templates
    switch (pin) {
        case 0: info.set_reg = getTeensy3SetReg<0>(); info.clear_reg = getTeensy3ClearReg<0>(); info.mask = getTeensy3Mask<0>(); break;
        case 1: info.set_reg = getTeensy3SetReg<1>(); info.clear_reg = getTeensy3ClearReg<1>(); info.mask = getTeensy3Mask<1>(); break;
        case 2: info.set_reg = getTeensy3SetReg<2>(); info.clear_reg = getTeensy3ClearReg<2>(); info.mask = getTeensy3Mask<2>(); break;
        case 3: info.set_reg = getTeensy3SetReg<3>(); info.clear_reg = getTeensy3ClearReg<3>(); info.mask = getTeensy3Mask<3>(); break;
        case 4: info.set_reg = getTeensy3SetReg<4>(); info.clear_reg = getTeensy3ClearReg<4>(); info.mask = getTeensy3Mask<4>(); break;
        case 5: info.set_reg = getTeensy3SetReg<5>(); info.clear_reg = getTeensy3ClearReg<5>(); info.mask = getTeensy3Mask<5>(); break;
        case 6: info.set_reg = getTeensy3SetReg<6>(); info.clear_reg = getTeensy3ClearReg<6>(); info.mask = getTeensy3Mask<6>(); break;
        case 7: info.set_reg = getTeensy3SetReg<7>(); info.clear_reg = getTeensy3ClearReg<7>(); info.mask = getTeensy3Mask<7>(); break;
        case 8: info.set_reg = getTeensy3SetReg<8>(); info.clear_reg = getTeensy3ClearReg<8>(); info.mask = getTeensy3Mask<8>(); break;
        case 9: info.set_reg = getTeensy3SetReg<9>(); info.clear_reg = getTeensy3ClearReg<9>(); info.mask = getTeensy3Mask<9>(); break;
        case 10: info.set_reg = getTeensy3SetReg<10>(); info.clear_reg = getTeensy3ClearReg<10>(); info.mask = getTeensy3Mask<10>(); break;
        case 11: info.set_reg = getTeensy3SetReg<11>(); info.clear_reg = getTeensy3ClearReg<11>(); info.mask = getTeensy3Mask<11>(); break;
        case 12: info.set_reg = getTeensy3SetReg<12>(); info.clear_reg = getTeensy3ClearReg<12>(); info.mask = getTeensy3Mask<12>(); break;
        case 13: info.set_reg = getTeensy3SetReg<13>(); info.clear_reg = getTeensy3ClearReg<13>(); info.mask = getTeensy3Mask<13>(); break;
        case 14: info.set_reg = getTeensy3SetReg<14>(); info.clear_reg = getTeensy3ClearReg<14>(); info.mask = getTeensy3Mask<14>(); break;
        case 15: info.set_reg = getTeensy3SetReg<15>(); info.clear_reg = getTeensy3ClearReg<15>(); info.mask = getTeensy3Mask<15>(); break;
        case 16: info.set_reg = getTeensy3SetReg<16>(); info.clear_reg = getTeensy3ClearReg<16>(); info.mask = getTeensy3Mask<16>(); break;
        case 17: info.set_reg = getTeensy3SetReg<17>(); info.clear_reg = getTeensy3ClearReg<17>(); info.mask = getTeensy3Mask<17>(); break;
        case 18: info.set_reg = getTeensy3SetReg<18>(); info.clear_reg = getTeensy3ClearReg<18>(); info.mask = getTeensy3Mask<18>(); break;
        case 19: info.set_reg = getTeensy3SetReg<19>(); info.clear_reg = getTeensy3ClearReg<19>(); info.mask = getTeensy3Mask<19>(); break;
        case 20: info.set_reg = getTeensy3SetReg<20>(); info.clear_reg = getTeensy3ClearReg<20>(); info.mask = getTeensy3Mask<20>(); break;
        case 21: info.set_reg = getTeensy3SetReg<21>(); info.clear_reg = getTeensy3ClearReg<21>(); info.mask = getTeensy3Mask<21>(); break;
        case 22: info.set_reg = getTeensy3SetReg<22>(); info.clear_reg = getTeensy3ClearReg<22>(); info.mask = getTeensy3Mask<22>(); break;
        case 23: info.set_reg = getTeensy3SetReg<23>(); info.clear_reg = getTeensy3ClearReg<23>(); info.mask = getTeensy3Mask<23>(); break;
        case 24: info.set_reg = getTeensy3SetReg<24>(); info.clear_reg = getTeensy3ClearReg<24>(); info.mask = getTeensy3Mask<24>(); break;
        case 25: info.set_reg = getTeensy3SetReg<25>(); info.clear_reg = getTeensy3ClearReg<25>(); info.mask = getTeensy3Mask<25>(); break;
        case 26: info.set_reg = getTeensy3SetReg<26>(); info.clear_reg = getTeensy3ClearReg<26>(); info.mask = getTeensy3Mask<26>(); break;
        case 27: info.set_reg = getTeensy3SetReg<27>(); info.clear_reg = getTeensy3ClearReg<27>(); info.mask = getTeensy3Mask<27>(); break;
        case 28: info.set_reg = getTeensy3SetReg<28>(); info.clear_reg = getTeensy3ClearReg<28>(); info.mask = getTeensy3Mask<28>(); break;
        case 29: info.set_reg = getTeensy3SetReg<29>(); info.clear_reg = getTeensy3ClearReg<29>(); info.mask = getTeensy3Mask<29>(); break;
        case 30: info.set_reg = getTeensy3SetReg<30>(); info.clear_reg = getTeensy3ClearReg<30>(); info.mask = getTeensy3Mask<30>(); break;
        case 31: info.set_reg = getTeensy3SetReg<31>(); info.clear_reg = getTeensy3ClearReg<31>(); info.mask = getTeensy3Mask<31>(); break;
        case 32: info.set_reg = getTeensy3SetReg<32>(); info.clear_reg = getTeensy3ClearReg<32>(); info.mask = getTeensy3Mask<32>(); break;
        case 33: info.set_reg = getTeensy3SetReg<33>(); info.clear_reg = getTeensy3ClearReg<33>(); info.mask = getTeensy3Mask<33>(); break;
#if defined(__MK66FX1M0__) || defined(__MK64FX512__)
        // Teensy 3.5/3.6 have additional pins (34-63)
        case 34: info.set_reg = getTeensy3SetReg<34>(); info.clear_reg = getTeensy3ClearReg<34>(); info.mask = getTeensy3Mask<34>(); break;
        case 35: info.set_reg = getTeensy3SetReg<35>(); info.clear_reg = getTeensy3ClearReg<35>(); info.mask = getTeensy3Mask<35>(); break;
        case 36: info.set_reg = getTeensy3SetReg<36>(); info.clear_reg = getTeensy3ClearReg<36>(); info.mask = getTeensy3Mask<36>(); break;
        case 37: info.set_reg = getTeensy3SetReg<37>(); info.clear_reg = getTeensy3ClearReg<37>(); info.mask = getTeensy3Mask<37>(); break;
        case 38: info.set_reg = getTeensy3SetReg<38>(); info.clear_reg = getTeensy3ClearReg<38>(); info.mask = getTeensy3Mask<38>(); break;
        case 39: info.set_reg = getTeensy3SetReg<39>(); info.clear_reg = getTeensy3ClearReg<39>(); info.mask = getTeensy3Mask<39>(); break;
        case 40: info.set_reg = getTeensy3SetReg<40>(); info.clear_reg = getTeensy3ClearReg<40>(); info.mask = getTeensy3Mask<40>(); break;
        case 41: info.set_reg = getTeensy3SetReg<41>(); info.clear_reg = getTeensy3ClearReg<41>(); info.mask = getTeensy3Mask<41>(); break;
        case 42: info.set_reg = getTeensy3SetReg<42>(); info.clear_reg = getTeensy3ClearReg<42>(); info.mask = getTeensy3Mask<42>(); break;
        case 43: info.set_reg = getTeensy3SetReg<43>(); info.clear_reg = getTeensy3ClearReg<43>(); info.mask = getTeensy3Mask<43>(); break;
        case 44: info.set_reg = getTeensy3SetReg<44>(); info.clear_reg = getTeensy3ClearReg<44>(); info.mask = getTeensy3Mask<44>(); break;
        case 45: info.set_reg = getTeensy3SetReg<45>(); info.clear_reg = getTeensy3ClearReg<45>(); info.mask = getTeensy3Mask<45>(); break;
        case 46: info.set_reg = getTeensy3SetReg<46>(); info.clear_reg = getTeensy3ClearReg<46>(); info.mask = getTeensy3Mask<46>(); break;
        case 47: info.set_reg = getTeensy3SetReg<47>(); info.clear_reg = getTeensy3ClearReg<47>(); info.mask = getTeensy3Mask<47>(); break;
        case 48: info.set_reg = getTeensy3SetReg<48>(); info.clear_reg = getTeensy3ClearReg<48>(); info.mask = getTeensy3Mask<48>(); break;
        case 49: info.set_reg = getTeensy3SetReg<49>(); info.clear_reg = getTeensy3ClearReg<49>(); info.mask = getTeensy3Mask<49>(); break;
        case 50: info.set_reg = getTeensy3SetReg<50>(); info.clear_reg = getTeensy3ClearReg<50>(); info.mask = getTeensy3Mask<50>(); break;
        case 51: info.set_reg = getTeensy3SetReg<51>(); info.clear_reg = getTeensy3ClearReg<51>(); info.mask = getTeensy3Mask<51>(); break;
        case 52: info.set_reg = getTeensy3SetReg<52>(); info.clear_reg = getTeensy3ClearReg<52>(); info.mask = getTeensy3Mask<52>(); break;
        case 53: info.set_reg = getTeensy3SetReg<53>(); info.clear_reg = getTeensy3ClearReg<53>(); info.mask = getTeensy3Mask<53>(); break;
        case 54: info.set_reg = getTeensy3SetReg<54>(); info.clear_reg = getTeensy3ClearReg<54>(); info.mask = getTeensy3Mask<54>(); break;
        case 55: info.set_reg = getTeensy3SetReg<55>(); info.clear_reg = getTeensy3ClearReg<55>(); info.mask = getTeensy3Mask<55>(); break;
        case 56: info.set_reg = getTeensy3SetReg<56>(); info.clear_reg = getTeensy3ClearReg<56>(); info.mask = getTeensy3Mask<56>(); break;
        case 57: info.set_reg = getTeensy3SetReg<57>(); info.clear_reg = getTeensy3ClearReg<57>(); info.mask = getTeensy3Mask<57>(); break;
        case 58: info.set_reg = getTeensy3SetReg<58>(); info.clear_reg = getTeensy3ClearReg<58>(); info.mask = getTeensy3Mask<58>(); break;
        case 59: info.set_reg = getTeensy3SetReg<59>(); info.clear_reg = getTeensy3ClearReg<59>(); info.mask = getTeensy3Mask<59>(); break;
        case 60: info.set_reg = getTeensy3SetReg<60>(); info.clear_reg = getTeensy3ClearReg<60>(); info.mask = getTeensy3Mask<60>(); break;
        case 61: info.set_reg = getTeensy3SetReg<61>(); info.clear_reg = getTeensy3ClearReg<61>(); info.mask = getTeensy3Mask<61>(); break;
        case 62: info.set_reg = getTeensy3SetReg<62>(); info.clear_reg = getTeensy3ClearReg<62>(); info.mask = getTeensy3Mask<62>(); break;
        case 63: info.set_reg = getTeensy3SetReg<63>(); info.clear_reg = getTeensy3ClearReg<63>(); info.mask = getTeensy3Mask<63>(); break;
#endif
        default: break;  // Invalid pin
    }

    return info;
}

} // namespace detail

//
// FastPinsSamePort<> Implementation for Teensy 3.x
//

template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Teensy 3.x: Validate all pins on same GPIO port (A/B/C/D/E)
    // Get SET register address for first pin (each port has unique PSOR address)
    auto first_info = detail::getPinInfo(pins[0]);
    volatile uint32_t* first_port = first_info.set_reg;

    for (uint8_t i = 1; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        if (pin_info.set_reg != first_port) {
            return false;  // Cross-port not allowed in same-port mode
        }
    }

    return true;
}

template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Teensy 3.x: Use PSOR and PCOR registers for atomic writes
    volatile uint32_t* psor = (volatile uint32_t*)mPsorReg;
    volatile uint32_t* pcor = (volatile uint32_t*)mPcorReg;
    *psor = set_mask;
    *pcor = clear_mask;
}

template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine GPIO port (use first pin's SET/CLEAR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mPsorReg = first_info.set_reg;
    mPcorReg = first_info.clear_reg;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        pin_masks[i] = pin_info.mask;
    }

    // Build 256-entry LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mLUT[pattern].set_mask = set_mask;
        mLUT[pattern].clear_mask = clear_mask;
    }
}

//
// FastPinsWithClock<> Implementation for Teensy 3.x
//

template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(
    uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {

    // Teensy 3.x: Clock and all data pins must be on same GPIO port (A/B/C/D/E)
    auto clock_info = detail::getPinInfo(clockPin);
    volatile uint32_t* clock_port = clock_info.set_reg;

    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(dataPins[i]);
        if (pin_info.set_reg != clock_port) {
            return false;  // Different ports detected
        }
    }

    return true;
}

template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store clock mask and register pointers
    auto clock_info = detail::getPinInfo(clockPin);
    mClockMask = clock_info.mask;
    mClockSet = clock_info.set_reg;
    mClockClear = clock_info.clear_reg;
}

//
// FastPins<> Auto-Detect Implementation for Teensy 3.x
//

template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Teensy 3.x: Check if all pins are on same GPIO port (A/B/C/D/E)
    auto first_info = detail::getPinInfo(pins[0]);
    volatile uint32_t* first_port = first_info.set_reg;

    for (uint8_t i = 1; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        if (pin_info.set_reg != first_port) {
            return false;  // Different ports detected
        }
    }

    return true;  // All pins on same port
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Teensy 3.x: Same as FastPinsSamePort<>::writeImpl()
    volatile uint32_t* psor = (volatile uint32_t*)mPsorReg;
    volatile uint32_t* pcor = (volatile uint32_t*)mPcorReg;
    *psor = set_mask;
    *pcor = clear_mask;
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // Teensy 3.x: Sequential writes to multiple GPIO ports (A/B/C/D/E)
    for (uint8_t i = 0; i < entry.port_count; i++) {
        volatile uint32_t* set_reg = (volatile uint32_t*)entry.ports[i].port_set;
        volatile uint32_t* clear_reg = (volatile uint32_t*)entry.ports[i].port_clear;

        *set_reg = entry.ports[i].set_mask;
        *clear_reg = entry.ports[i].clear_mask;
    }
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine GPIO port (use first pin's SET/CLEAR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mPsorReg = first_info.set_reg;
    mPcorReg = first_info.clear_reg;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        pin_masks[i] = pin_info.mask;
    }

    // Build 256-entry LUT (same algorithm as FastPinsSamePort)
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mSamePortLUT[pattern].set_mask = set_mask;
        mSamePortLUT[pattern].clear_mask = clear_mask;
    }
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Group pins by GPIO port
    struct PinInfoRuntime {
        volatile uint32_t* set_reg;
        volatile uint32_t* clear_reg;
        uint32_t mask;
    };
    PinInfoRuntime pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        auto info = detail::getPinInfo(pins[i]);
        pin_info[i].set_reg = info.set_reg;
        pin_info[i].clear_reg = info.clear_reg;
        pin_info[i].mask = info.mask;
    }

    // Build 256-entry multi-port LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        // Count unique ports for this pattern
        volatile uint32_t* ports[5] = {nullptr};  // Max 5 ports (A, B, C, D, E)
        uint8_t port_count = 0;

        // Identify unique ports
        for (uint8_t bit = 0; bit < count; bit++) {
            volatile uint32_t* port = pin_info[bit].set_reg;
            bool found = false;
            for (uint8_t p = 0; p < port_count; p++) {
                if (ports[p] == port) {
                    found = true;
                    break;
                }
            }
            if (!found && port_count < 5) {
                ports[port_count++] = port;
            }
        }

        // Build masks for each port
        for (uint8_t p = 0; p < port_count; p++) {
            volatile uint32_t* port_set = ports[p];
            uint32_t set_mask = 0;
            uint32_t clear_mask = 0;

            for (uint8_t bit = 0; bit < count; bit++) {
                if (pin_info[bit].set_reg == port_set) {
                    if (pattern & (1 << bit)) {
                        set_mask |= pin_info[bit].mask;
                    } else {
                        clear_mask |= pin_info[bit].mask;
                    }
                }
            }

            // Store register pointers and masks
            mMultiPortLUT[pattern].ports[p].port_set = port_set;
            mMultiPortLUT[pattern].ports[p].port_clear = pin_info[0].clear_reg;  // Same offset for all ports
            mMultiPortLUT[pattern].ports[p].set_mask = set_mask;
            mMultiPortLUT[pattern].ports[p].clear_mask = clear_mask;

            // Find PCOR register (PCOR = PSOR + 4 bytes)
            mMultiPortLUT[pattern].ports[p].port_clear = (volatile uint32_t*)((uintptr_t)port_set + 4);
        }

        mMultiPortLUT[pattern].port_count = port_count;
    }
}

} // namespace fl

//
// Platform-Specific Storage for Teensy 3.x
//

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__MKL26Z64__) || defined(FASTLED_TEENSY3)

namespace fl {

// FastPinsSamePort<> platform-specific members
template<uint8_t MAX_PINS>
class FastPinsSamePort {
private:
    volatile uint32_t* mPsorReg = nullptr;  // Teensy 3.x: PSOR register pointer
    volatile uint32_t* mPcorReg = nullptr;  // Teensy 3.x: PCOR register pointer
};

// FastPins<> platform-specific members
template<uint8_t MAX_PINS>
class FastPins {
private:
    volatile uint32_t* mPsorReg = nullptr;  // Teensy 3.x: PSOR register for same-port mode
    volatile uint32_t* mPcorReg = nullptr;  // Teensy 3.x: PCOR register for same-port mode
};

} // namespace fl

#endif // Teensy 3.x platform detection

#endif // Teensy 3.x guard
