#ifndef __INC_FAST_PINS_ARM_RENESAS_H
#define __INC_FAST_PINS_ARM_RENESAS_H

// FastPins implementation for Renesas (Arduino UNO R4, RA4M1, RA6M5, etc.)
//
// Platform: Renesas RA4M1/RA6M5 (ARM Cortex-M4/M33)
// Boards: Arduino UNO R4 Minima, UNO R4 WiFi, Portenta C33, Thingplus RA6M5
//
// Architecture:
// - Renesas RA series uses PORT peripheral with POSR/PORR registers
// - POSR (Port Output Set Register): Write 1 to set pin HIGH (atomic)
// - PORR (Port Output Reset Register): Write 1 to set pin LOW (atomic)
// - PODR (Port Output Data Register): Read/write current pin state
// - Multiple PORTs (PORT0-PORT9), each with 16 pins
// - Pin mapping is board-specific (defined in fastpin_arm_renesas.h)
//
// Register Map (Renesas RA series):
//   R_PORTx (x = 0-9) - Up to 10 GPIO ports
//     ->PODR   - Port Output Data Register (read/write current state)
//     ->POSR   - Port Output Set Register (write 1 to set HIGH, write 0 has no effect)
//     ->PORR   - Port Output Reset Register (write 1 to clear LOW, write 0 has no effect)
//     ->PIDR   - Port Input Data Register (read pin state)
//     ->PDR    - Port Direction Register (1 = output mode)
//
// Pin Encoding:
// - Arduino pin numbers (0-22) map to BSP_IO_PORT_XX_PIN_YY via compile-time templates
// - Each pin belongs to one PORT (0-9)
// - Runtime detection uses existing FastPin<> compile-time infrastructure
// - Switch statement bridges runtime pin numbers to compile-time templates
//
// Performance:
// - Same-port mode: ~25-30ns (atomic POSR/PORR writes)
// - Multi-port mode: ~60-120ns (sequential writes to multiple PORTs)
// - Clock speed: 48 MHz (UNO R4) to 200 MHz (RA6M5)
//
// Supported Boards:
// - Arduino UNO R4 Minima (RA4M1, 20 pins)
// - Arduino UNO R4 WiFi (RA4M1, 20 pins)
// - Arduino Portenta C33 (RA6M5, 22 pins)
// - Arduino Thingplus RA6M5 (RA6M5, 24 pins)

#if defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)

#include "fastpin_arm_renesas.h"

namespace fl {

namespace detail {

/// Helper to get GPIO port SET register address for a Renesas pin
template<uint8_t PIN>
inline volatile uint16_t* getRenesasSetReg() {
    return (volatile uint16_t*)FastPin<PIN>::sport();
}

/// Helper to get GPIO port RESET register address for a Renesas pin
template<uint8_t PIN>
inline volatile uint16_t* getRenesasClearReg() {
    return (volatile uint16_t*)FastPin<PIN>::cport();
}

/// Helper to get pin bit mask for a Renesas pin
template<uint8_t PIN>
inline uint16_t getRenesasMask() {
    return FastPin<PIN>::mask();
}

/// Runtime pin info lookup using switch statement
/// Bridges runtime pin numbers to compile-time FastPin<> templates
struct PinInfo {
    volatile uint16_t* set_reg;    // POSR register address
    volatile uint16_t* clear_reg;  // PORR register address
    uint16_t mask;                 // Pin bit mask
};

inline PinInfo getPinInfo(uint8_t pin) {
    PinInfo info = {nullptr, nullptr, 0};

    // Use switch to map pin number to FastPin template instantiation
    // Cases vary by board:
    // - UNO R4 Minima/WiFi: 20 pins (0-19)
    // - Portenta C33: 22 pins (0-22)
    // - Thingplus RA6M5: 24 pins (0-21, 30-31)

#if defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_UNOR4_MINIMA)
    switch (pin) {
        case 0: info.set_reg = getRenesasSetReg<0>(); info.clear_reg = getRenesasClearReg<0>(); info.mask = getRenesasMask<0>(); break;
        case 1: info.set_reg = getRenesasSetReg<1>(); info.clear_reg = getRenesasClearReg<1>(); info.mask = getRenesasMask<1>(); break;
        case 2: info.set_reg = getRenesasSetReg<2>(); info.clear_reg = getRenesasClearReg<2>(); info.mask = getRenesasMask<2>(); break;
        case 3: info.set_reg = getRenesasSetReg<3>(); info.clear_reg = getRenesasClearReg<3>(); info.mask = getRenesasMask<3>(); break;
        case 4: info.set_reg = getRenesasSetReg<4>(); info.clear_reg = getRenesasClearReg<4>(); info.mask = getRenesasMask<4>(); break;
        case 5: info.set_reg = getRenesasSetReg<5>(); info.clear_reg = getRenesasClearReg<5>(); info.mask = getRenesasMask<5>(); break;
        case 6: info.set_reg = getRenesasSetReg<6>(); info.clear_reg = getRenesasClearReg<6>(); info.mask = getRenesasMask<6>(); break;
        case 7: info.set_reg = getRenesasSetReg<7>(); info.clear_reg = getRenesasClearReg<7>(); info.mask = getRenesasMask<7>(); break;
        case 8: info.set_reg = getRenesasSetReg<8>(); info.clear_reg = getRenesasClearReg<8>(); info.mask = getRenesasMask<8>(); break;
        case 9: info.set_reg = getRenesasSetReg<9>(); info.clear_reg = getRenesasClearReg<9>(); info.mask = getRenesasMask<9>(); break;
        case 10: info.set_reg = getRenesasSetReg<10>(); info.clear_reg = getRenesasClearReg<10>(); info.mask = getRenesasMask<10>(); break;
        case 11: info.set_reg = getRenesasSetReg<11>(); info.clear_reg = getRenesasClearReg<11>(); info.mask = getRenesasMask<11>(); break;
        case 12: info.set_reg = getRenesasSetReg<12>(); info.clear_reg = getRenesasClearReg<12>(); info.mask = getRenesasMask<12>(); break;
        case 13: info.set_reg = getRenesasSetReg<13>(); info.clear_reg = getRenesasClearReg<13>(); info.mask = getRenesasMask<13>(); break;
        case 14: info.set_reg = getRenesasSetReg<14>(); info.clear_reg = getRenesasClearReg<14>(); info.mask = getRenesasMask<14>(); break;
        case 15: info.set_reg = getRenesasSetReg<15>(); info.clear_reg = getRenesasClearReg<15>(); info.mask = getRenesasMask<15>(); break;
        case 16: info.set_reg = getRenesasSetReg<16>(); info.clear_reg = getRenesasClearReg<16>(); info.mask = getRenesasMask<16>(); break;
        case 17: info.set_reg = getRenesasSetReg<17>(); info.clear_reg = getRenesasClearReg<17>(); info.mask = getRenesasMask<17>(); break;
        case 18: info.set_reg = getRenesasSetReg<18>(); info.clear_reg = getRenesasClearReg<18>(); info.mask = getRenesasMask<18>(); break;
        case 19: info.set_reg = getRenesasSetReg<19>(); info.clear_reg = getRenesasClearReg<19>(); info.mask = getRenesasMask<19>(); break;
        default: break;  // Invalid pin
    }
#elif defined(ARDUINO_THINGPLUS_RA6M5)
    switch (pin) {
        case 0: info.set_reg = getRenesasSetReg<0>(); info.clear_reg = getRenesasClearReg<0>(); info.mask = getRenesasMask<0>(); break;
        case 1: info.set_reg = getRenesasSetReg<1>(); info.clear_reg = getRenesasClearReg<1>(); info.mask = getRenesasMask<1>(); break;
        case 2: info.set_reg = getRenesasSetReg<2>(); info.clear_reg = getRenesasClearReg<2>(); info.mask = getRenesasMask<2>(); break;
        case 3: info.set_reg = getRenesasSetReg<3>(); info.clear_reg = getRenesasClearReg<3>(); info.mask = getRenesasMask<3>(); break;
        case 4: info.set_reg = getRenesasSetReg<4>(); info.clear_reg = getRenesasClearReg<4>(); info.mask = getRenesasMask<4>(); break;
        case 5: info.set_reg = getRenesasSetReg<5>(); info.clear_reg = getRenesasClearReg<5>(); info.mask = getRenesasMask<5>(); break;
        case 6: info.set_reg = getRenesasSetReg<6>(); info.clear_reg = getRenesasClearReg<6>(); info.mask = getRenesasMask<6>(); break;
        case 7: info.set_reg = getRenesasSetReg<7>(); info.clear_reg = getRenesasClearReg<7>(); info.mask = getRenesasMask<7>(); break;
        case 8: info.set_reg = getRenesasSetReg<8>(); info.clear_reg = getRenesasClearReg<8>(); info.mask = getRenesasMask<8>(); break;
        case 9: info.set_reg = getRenesasSetReg<9>(); info.clear_reg = getRenesasClearReg<9>(); info.mask = getRenesasMask<9>(); break;
        case 10: info.set_reg = getRenesasSetReg<10>(); info.clear_reg = getRenesasClearReg<10>(); info.mask = getRenesasMask<10>(); break;
        case 11: info.set_reg = getRenesasSetReg<11>(); info.clear_reg = getRenesasClearReg<11>(); info.mask = getRenesasMask<11>(); break;
        case 12: info.set_reg = getRenesasSetReg<12>(); info.clear_reg = getRenesasClearReg<12>(); info.mask = getRenesasMask<12>(); break;
        case 13: info.set_reg = getRenesasSetReg<13>(); info.clear_reg = getRenesasClearReg<13>(); info.mask = getRenesasMask<13>(); break;
        case 14: info.set_reg = getRenesasSetReg<14>(); info.clear_reg = getRenesasClearReg<14>(); info.mask = getRenesasMask<14>(); break;
        case 15: info.set_reg = getRenesasSetReg<15>(); info.clear_reg = getRenesasClearReg<15>(); info.mask = getRenesasMask<15>(); break;
        case 16: info.set_reg = getRenesasSetReg<16>(); info.clear_reg = getRenesasClearReg<16>(); info.mask = getRenesasMask<16>(); break;
        case 17: info.set_reg = getRenesasSetReg<17>(); info.clear_reg = getRenesasClearReg<17>(); info.mask = getRenesasMask<17>(); break;
        case 18: info.set_reg = getRenesasSetReg<18>(); info.clear_reg = getRenesasClearReg<18>(); info.mask = getRenesasMask<18>(); break;
        case 19: info.set_reg = getRenesasSetReg<19>(); info.clear_reg = getRenesasClearReg<19>(); info.mask = getRenesasMask<19>(); break;
        case 20: info.set_reg = getRenesasSetReg<20>(); info.clear_reg = getRenesasClearReg<20>(); info.mask = getRenesasMask<20>(); break;
        case 21: info.set_reg = getRenesasSetReg<21>(); info.clear_reg = getRenesasClearReg<21>(); info.mask = getRenesasMask<21>(); break;
        case 30: info.set_reg = getRenesasSetReg<30>(); info.clear_reg = getRenesasClearReg<30>(); info.mask = getRenesasMask<30>(); break;
        case 31: info.set_reg = getRenesasSetReg<31>(); info.clear_reg = getRenesasClearReg<31>(); info.mask = getRenesasMask<31>(); break;
        default: break;  // Invalid pin
    }
#elif defined(ARDUINO_ARCH_RENESAS_PORTENTA)
    switch (pin) {
        case 0: info.set_reg = getRenesasSetReg<0>(); info.clear_reg = getRenesasClearReg<0>(); info.mask = getRenesasMask<0>(); break;
        case 1: info.set_reg = getRenesasSetReg<1>(); info.clear_reg = getRenesasClearReg<1>(); info.mask = getRenesasMask<1>(); break;
        case 2: info.set_reg = getRenesasSetReg<2>(); info.clear_reg = getRenesasClearReg<2>(); info.mask = getRenesasMask<2>(); break;
        case 3: info.set_reg = getRenesasSetReg<3>(); info.clear_reg = getRenesasClearReg<3>(); info.mask = getRenesasMask<3>(); break;
        case 4: info.set_reg = getRenesasSetReg<4>(); info.clear_reg = getRenesasClearReg<4>(); info.mask = getRenesasMask<4>(); break;
        case 5: info.set_reg = getRenesasSetReg<5>(); info.clear_reg = getRenesasClearReg<5>(); info.mask = getRenesasMask<5>(); break;
        case 6: info.set_reg = getRenesasSetReg<6>(); info.clear_reg = getRenesasClearReg<6>(); info.mask = getRenesasMask<6>(); break;
        case 7: info.set_reg = getRenesasSetReg<7>(); info.clear_reg = getRenesasClearReg<7>(); info.mask = getRenesasMask<7>(); break;
        case 8: info.set_reg = getRenesasSetReg<8>(); info.clear_reg = getRenesasClearReg<8>(); info.mask = getRenesasMask<8>(); break;
        case 9: info.set_reg = getRenesasSetReg<9>(); info.clear_reg = getRenesasClearReg<9>(); info.mask = getRenesasMask<9>(); break;
        case 10: info.set_reg = getRenesasSetReg<10>(); info.clear_reg = getRenesasClearReg<10>(); info.mask = getRenesasMask<10>(); break;
        case 11: info.set_reg = getRenesasSetReg<11>(); info.clear_reg = getRenesasClearReg<11>(); info.mask = getRenesasMask<11>(); break;
        case 12: info.set_reg = getRenesasSetReg<12>(); info.clear_reg = getRenesasClearReg<12>(); info.mask = getRenesasMask<12>(); break;
        case 13: info.set_reg = getRenesasSetReg<13>(); info.clear_reg = getRenesasClearReg<13>(); info.mask = getRenesasMask<13>(); break;
        case 14: info.set_reg = getRenesasSetReg<14>(); info.clear_reg = getRenesasClearReg<14>(); info.mask = getRenesasMask<14>(); break;
        case 15: info.set_reg = getRenesasSetReg<15>(); info.clear_reg = getRenesasClearReg<15>(); info.mask = getRenesasMask<15>(); break;
        case 16: info.set_reg = getRenesasSetReg<16>(); info.clear_reg = getRenesasClearReg<16>(); info.mask = getRenesasMask<16>(); break;
        case 17: info.set_reg = getRenesasSetReg<17>(); info.clear_reg = getRenesasClearReg<17>(); info.mask = getRenesasMask<17>(); break;
        case 18: info.set_reg = getRenesasSetReg<18>(); info.clear_reg = getRenesasClearReg<18>(); info.mask = getRenesasMask<18>(); break;
        case 19: info.set_reg = getRenesasSetReg<19>(); info.clear_reg = getRenesasClearReg<19>(); info.mask = getRenesasMask<19>(); break;
        case 20: info.set_reg = getRenesasSetReg<20>(); info.clear_reg = getRenesasClearReg<20>(); info.mask = getRenesasMask<20>(); break;
        case 21: info.set_reg = getRenesasSetReg<21>(); info.clear_reg = getRenesasClearReg<21>(); info.mask = getRenesasMask<21>(); break;
        case 22: info.set_reg = getRenesasSetReg<22>(); info.clear_reg = getRenesasClearReg<22>(); info.mask = getRenesasMask<22>(); break;
        default: break;  // Invalid pin
    }
#endif

    return info;
}

} // namespace detail

// ============================================================================
// FastPinsSamePort<> Implementation
// ============================================================================

/// Renesas same-port validation
/// Check if all pins are on same PORT (0-9)
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Renesas: All pins must be on same PORT (PORT0-PORT9)
    // Use POSR register address to identify port (each port has unique POSR address)
    auto first_info = detail::getPinInfo(pins[0]);
    volatile uint16_t* first_port = first_info.set_reg;

    for (uint8_t i = 1; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        if (pin_info.set_reg != first_port) {
            return false;  // Cross-port not allowed in same-port mode
        }
    }

    return true;
}

/// Renesas same-port write implementation
/// Performance: ~25-30ns (two sequential MMIO writes to POSR and PORR)
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Renesas: Use POSR and PORR registers for atomic writes
    // Note: Registers are 16-bit, but we use 32-bit masks for API consistency
    volatile uint16_t* posr = (volatile uint16_t*)mPosrReg;
    volatile uint16_t* porr = (volatile uint16_t*)mPorrReg;
    *posr = static_cast<uint16_t>(set_mask);
    *porr = static_cast<uint16_t>(clear_mask);
}

/// Build LUT for Renesas
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine PORT (use first pin's POSR/PORR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mPosrReg = first_info.set_reg;
    mPorrReg = first_info.clear_reg;

    // Extract pin masks via runtime lookup
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        pin_masks[i] = pin_info.mask;
    }

    // Build 256-entry LUT (standard algorithm)
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

// ============================================================================
// FastPinsWithClock<> Implementation
// ============================================================================

/// Renesas clock+data validation
/// Check if clock pin and all data pins are on same PORT
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(
    uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {

    // Renesas: Clock and all data pins must be on same PORT (PORT0-PORT9)
    auto clock_info = detail::getPinInfo(clockPin);
    volatile uint16_t* clock_port = clock_info.set_reg;

    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(dataPins[i]);
        if (pin_info.set_reg != clock_port) {
            return false;  // Different ports detected
        }
    }

    return true;
}

/// Build clock mask for Renesas
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store clock mask and register pointers
    auto clock_info = detail::getPinInfo(clockPin);
    mClockMask = clock_info.mask;
    mClockSet = clock_info.set_reg;
    mClockClear = clock_info.clear_reg;
}

// ============================================================================
// FastPins<> Auto-Detect Implementation
// ============================================================================

/// Renesas cross-port detection
/// Returns true if all pins are on same PORT
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Renesas: Check if all pins are on same PORT (PORT0-PORT9)
    auto first_info = detail::getPinInfo(pins[0]);
    volatile uint16_t* first_port = first_info.set_reg;

    for (uint8_t i = 1; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        if (pin_info.set_reg != first_port) {
            return false;  // Different ports detected
        }
    }

    return true;  // All pins on same port
}

/// Renesas same-port write implementation (for FastPins<>)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Renesas: Same as FastPinsSamePort<>::writeImpl()
    volatile uint16_t* posr = (volatile uint16_t*)mPosrReg;
    volatile uint16_t* porr = (volatile uint16_t*)mPorrReg;
    *posr = static_cast<uint16_t>(set_mask);
    *porr = static_cast<uint16_t>(clear_mask);
}

/// Renesas multi-port write implementation
/// Performance: 60-120ns for cross-port (sequential writes to multiple PORTs)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // Renesas: Sequential writes to multiple PORTs (PORT0-PORT9)
    for (uint8_t i = 0; i < entry.port_count; i++) {
        volatile uint16_t* set_reg = (volatile uint16_t*)entry.ports[i].port_set;
        volatile uint16_t* clear_reg = (volatile uint16_t*)entry.ports[i].port_clear;

        *set_reg = static_cast<uint16_t>(entry.ports[i].set_mask);
        *clear_reg = static_cast<uint16_t>(entry.ports[i].clear_mask);
    }
}

/// Build same-port LUT for Renesas
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine PORT (use first pin's POSR/PORR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mPosrReg = first_info.set_reg;
    mPorrReg = first_info.clear_reg;

    // Extract pin masks via runtime lookup
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        auto pin_info = detail::getPinInfo(pins[i]);
        pin_masks[i] = pin_info.mask;
    }

    // Build 256-entry LUT (standard algorithm)
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

/// Build multi-port LUT for Renesas
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Group pins by PORT
    struct PinInfoRuntime {
        volatile uint16_t* set_reg;
        volatile uint16_t* clear_reg;
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
        volatile uint16_t* ports[10] = {nullptr};  // Max 10 ports (PORT0-PORT9)
        uint8_t port_count = 0;

        // Identify unique ports (by register address)
        for (uint8_t bit = 0; bit < count; bit++) {
            volatile uint16_t* port = pin_info[bit].set_reg;
            bool found = false;
            for (uint8_t p = 0; p < port_count; p++) {
                if (ports[p] == port) {
                    found = true;
                    break;
                }
            }
            if (!found && port_count < 10) {
                ports[port_count++] = port;
            }
        }

        // Build masks for each port
        for (uint8_t p = 0; p < port_count; p++) {
            volatile uint16_t* port_set = ports[p];
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
            mMultiPortLUT[pattern].ports[p].port_clear = pin_info[bit].clear_reg;
            mMultiPortLUT[pattern].ports[p].set_mask = set_mask;
            mMultiPortLUT[pattern].ports[p].clear_mask = clear_mask;
        }

        mMultiPortLUT[pattern].port_count = port_count;
    }
}

} // namespace fl

// ============================================================================
// Platform-Specific Storage
// ============================================================================

#if defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)

namespace fl {

// FastPinsSamePort<> platform-specific members
template<uint8_t MAX_PINS>
class FastPinsSamePort {
private:
    volatile uint16_t* mPosrReg = nullptr;  // Renesas: POSR register pointer
    volatile uint16_t* mPorrReg = nullptr;  // Renesas: PORR register pointer
};

// FastPinsWithClock<> platform-specific members
template<uint8_t DATA_PINS>
class FastPinsWithClock {
private:
    volatile uint16_t* mClockSet = nullptr;    // Renesas: Clock POSR register
    volatile uint16_t* mClockClear = nullptr;  // Renesas: Clock PORR register
    uint16_t mClockMask = 0;                   // Renesas: Clock pin mask
};

// FastPins<> platform-specific members
template<uint8_t MAX_PINS>
class FastPins {
private:
    volatile uint16_t* mPosrReg = nullptr;  // Renesas: POSR register for same-port mode
    volatile uint16_t* mPorrReg = nullptr;  // Renesas: PORR register for same-port mode
};

} // namespace fl

#endif // Renesas platform detection

#endif // defined(ARDUINO_ARCH_RENESAS)

#endif // __INC_FAST_PINS_ARM_RENESAS_H
