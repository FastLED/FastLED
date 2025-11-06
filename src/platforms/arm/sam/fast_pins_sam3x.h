/**
 * @file fast_pins_sam3x.h
 * @brief FastPins implementation for SAM3X (Arduino Due)
 *
 * Platform: Atmel SAM3X8E ARM Cortex-M3 (Arduino Due)
 * GPIO Architecture: PIO (Parallel I/O Controller) with SODR/CODR atomic registers
 *
 * Register Map (SAM3X8E):
 *   PIOx (x = A/B/C/D) - 4 PIO controllers (ports)
 *     ->PIO_ODSR   - Output Data Status Register (read/write current state)
 *     ->PIO_SODR   - Set Output Data Register (write 1 to set HIGH, write 0 has no effect)
 *     ->PIO_CODR   - Clear Output Data Register (write 1 to clear LOW, write 0 has no effect)
 *     ->PIO_OER    - Output Enable Register (1 = output mode)
 *     ->PIO_PDR    - PIO Disable Register (disable peripheral control)
 *     ->PIO_PER    - PIO Enable Register (enable GPIO control)
 *
 * Pin Encoding:
 *   - Arduino Due uses pin numbers 0-78 (standard pins) + 90-113 (Digix extension)
 *   - Pin-to-port mapping is board-specific (defined in fastpin_arm_sam.h)
 *   - Runtime detection uses existing FastPin<> compile-time infrastructure
 *   - Uses switch statement to bridge runtime pin numbers to compile-time templates
 *
 * GPIO Ports:
 *   - PIOA: Port A (pins mapped to various Arduino pin numbers)
 *   - PIOB: Port B (pins mapped to various Arduino pin numbers)
 *   - PIOC: Port C (pins mapped to various Arduino pin numbers)
 *   - PIOD: Port D (pins mapped to various Arduino pin numbers)
 *
 * Performance:
 *   - Same-port writes: ~25-30ns (SODR + CODR sequential writes)
 *   - Multi-port writes: 60-120ns (sequential port writes)
 *   - Uses atomic SODR/CODR registers (no read-modify-write)
 *
 * Supported Boards:
 *   - Arduino Due (SAM3X8E, 84 MHz, 78 standard pins)
 *   - Digix (SAM3X8E with extended pins 90-113)
 */

#ifndef __INC_FAST_PINS_SAM3X_H
#define __INC_FAST_PINS_SAM3X_H

#include "platforms/fast_pins.h"

// Platform detection: SAM3X
#if defined(__SAM3X8E__) || defined(ARDUINO_SAM_DUE) || defined(ARDUINO_ARCH_SAM)

// Include existing SAM3X FastPin infrastructure
#include "fastpin_arm_sam.h"

namespace fl {
namespace detail {

/**
 * Helper functions to access SAM3X GPIO registers via existing FastPin<> infrastructure.
 *
 * SAM3X uses compile-time pin mapping via FastPin<> templates.
 * We need runtime-to-compile-time bridge using switch statement.
 */

/// Helper to get GPIO port SET register address for a SAM3X pin
template<uint8_t PIN>
inline volatile uint32_t* getSAM3XSetReg() {
    return (volatile uint32_t*)FastPin<PIN>::sport();
}

/// Helper to get GPIO port CLEAR register address for a SAM3X pin
template<uint8_t PIN>
inline volatile uint32_t* getSAM3XClearReg() {
    return (volatile uint32_t*)FastPin<PIN>::cport();
}

/// Helper to get pin bit mask for a SAM3X pin
template<uint8_t PIN>
inline uint32_t getSAM3XMask() {
    return FastPin<PIN>::mask();
}

/// Runtime pin info structure
struct PinInfo {
    volatile uint32_t* set_reg;    // PIOX_SODR register address
    volatile uint32_t* clear_reg;  // PIOX_CODR register address
    uint32_t mask;                 // Pin bit mask
};

/**
 * Runtime pin info lookup using switch statement.
 *
 * Bridges runtime pin numbers to compile-time FastPin<> templates.
 *
 * SAM3X8E (Arduino Due) has 78 standard pins (0-78) + Digix pins (90-113).
 * We support all pins that have FastPin<> definitions in fastpin_arm_sam.h.
 */
inline PinInfo getPinInfo(uint8_t pin) {
    PinInfo info = {nullptr, nullptr, 0};

    // Use switch to map pin number to FastPin template instantiation
    switch (pin) {
        // Standard Arduino Due pins (0-78)
        case 0: info.set_reg = getSAM3XSetReg<0>(); info.clear_reg = getSAM3XClearReg<0>(); info.mask = getSAM3XMask<0>(); break;
        case 1: info.set_reg = getSAM3XSetReg<1>(); info.clear_reg = getSAM3XClearReg<1>(); info.mask = getSAM3XMask<1>(); break;
        case 2: info.set_reg = getSAM3XSetReg<2>(); info.clear_reg = getSAM3XClearReg<2>(); info.mask = getSAM3XMask<2>(); break;
        case 3: info.set_reg = getSAM3XSetReg<3>(); info.clear_reg = getSAM3XClearReg<3>(); info.mask = getSAM3XMask<3>(); break;
        case 4: info.set_reg = getSAM3XSetReg<4>(); info.clear_reg = getSAM3XClearReg<4>(); info.mask = getSAM3XMask<4>(); break;
        case 5: info.set_reg = getSAM3XSetReg<5>(); info.clear_reg = getSAM3XClearReg<5>(); info.mask = getSAM3XMask<5>(); break;
        case 6: info.set_reg = getSAM3XSetReg<6>(); info.clear_reg = getSAM3XClearReg<6>(); info.mask = getSAM3XMask<6>(); break;
        case 7: info.set_reg = getSAM3XSetReg<7>(); info.clear_reg = getSAM3XClearReg<7>(); info.mask = getSAM3XMask<7>(); break;
        case 8: info.set_reg = getSAM3XSetReg<8>(); info.clear_reg = getSAM3XClearReg<8>(); info.mask = getSAM3XMask<8>(); break;
        case 9: info.set_reg = getSAM3XSetReg<9>(); info.clear_reg = getSAM3XClearReg<9>(); info.mask = getSAM3XMask<9>(); break;
        case 10: info.set_reg = getSAM3XSetReg<10>(); info.clear_reg = getSAM3XClearReg<10>(); info.mask = getSAM3XMask<10>(); break;
        case 11: info.set_reg = getSAM3XSetReg<11>(); info.clear_reg = getSAM3XClearReg<11>(); info.mask = getSAM3XMask<11>(); break;
        case 12: info.set_reg = getSAM3XSetReg<12>(); info.clear_reg = getSAM3XClearReg<12>(); info.mask = getSAM3XMask<12>(); break;
        case 13: info.set_reg = getSAM3XSetReg<13>(); info.clear_reg = getSAM3XClearReg<13>(); info.mask = getSAM3XMask<13>(); break;
        case 14: info.set_reg = getSAM3XSetReg<14>(); info.clear_reg = getSAM3XClearReg<14>(); info.mask = getSAM3XMask<14>(); break;
        case 15: info.set_reg = getSAM3XSetReg<15>(); info.clear_reg = getSAM3XClearReg<15>(); info.mask = getSAM3XMask<15>(); break;
        case 16: info.set_reg = getSAM3XSetReg<16>(); info.clear_reg = getSAM3XClearReg<16>(); info.mask = getSAM3XMask<16>(); break;
        case 17: info.set_reg = getSAM3XSetReg<17>(); info.clear_reg = getSAM3XClearReg<17>(); info.mask = getSAM3XMask<17>(); break;
        case 18: info.set_reg = getSAM3XSetReg<18>(); info.clear_reg = getSAM3XClearReg<18>(); info.mask = getSAM3XMask<18>(); break;
        case 19: info.set_reg = getSAM3XSetReg<19>(); info.clear_reg = getSAM3XClearReg<19>(); info.mask = getSAM3XMask<19>(); break;
        case 20: info.set_reg = getSAM3XSetReg<20>(); info.clear_reg = getSAM3XClearReg<20>(); info.mask = getSAM3XMask<20>(); break;
        case 21: info.set_reg = getSAM3XSetReg<21>(); info.clear_reg = getSAM3XClearReg<21>(); info.mask = getSAM3XMask<21>(); break;
        case 22: info.set_reg = getSAM3XSetReg<22>(); info.clear_reg = getSAM3XClearReg<22>(); info.mask = getSAM3XMask<22>(); break;
        case 23: info.set_reg = getSAM3XSetReg<23>(); info.clear_reg = getSAM3XClearReg<23>(); info.mask = getSAM3XMask<23>(); break;
        case 24: info.set_reg = getSAM3XSetReg<24>(); info.clear_reg = getSAM3XClearReg<24>(); info.mask = getSAM3XMask<24>(); break;
        case 25: info.set_reg = getSAM3XSetReg<25>(); info.clear_reg = getSAM3XClearReg<25>(); info.mask = getSAM3XMask<25>(); break;
        case 26: info.set_reg = getSAM3XSetReg<26>(); info.clear_reg = getSAM3XClearReg<26>(); info.mask = getSAM3XMask<26>(); break;
        case 27: info.set_reg = getSAM3XSetReg<27>(); info.clear_reg = getSAM3XClearReg<27>(); info.mask = getSAM3XMask<27>(); break;
        case 28: info.set_reg = getSAM3XSetReg<28>(); info.clear_reg = getSAM3XClearReg<28>(); info.mask = getSAM3XMask<28>(); break;
        case 29: info.set_reg = getSAM3XSetReg<29>(); info.clear_reg = getSAM3XClearReg<29>(); info.mask = getSAM3XMask<29>(); break;
        case 30: info.set_reg = getSAM3XSetReg<30>(); info.clear_reg = getSAM3XClearReg<30>(); info.mask = getSAM3XMask<30>(); break;
        case 31: info.set_reg = getSAM3XSetReg<31>(); info.clear_reg = getSAM3XClearReg<31>(); info.mask = getSAM3XMask<31>(); break;
        case 32: info.set_reg = getSAM3XSetReg<32>(); info.clear_reg = getSAM3XClearReg<32>(); info.mask = getSAM3XMask<32>(); break;
        case 33: info.set_reg = getSAM3XSetReg<33>(); info.clear_reg = getSAM3XClearReg<33>(); info.mask = getSAM3XMask<33>(); break;
        case 34: info.set_reg = getSAM3XSetReg<34>(); info.clear_reg = getSAM3XClearReg<34>(); info.mask = getSAM3XMask<34>(); break;
        case 35: info.set_reg = getSAM3XSetReg<35>(); info.clear_reg = getSAM3XClearReg<35>(); info.mask = getSAM3XMask<35>(); break;
        case 36: info.set_reg = getSAM3XSetReg<36>(); info.clear_reg = getSAM3XClearReg<36>(); info.mask = getSAM3XMask<36>(); break;
        case 37: info.set_reg = getSAM3XSetReg<37>(); info.clear_reg = getSAM3XClearReg<37>(); info.mask = getSAM3XMask<37>(); break;
        case 38: info.set_reg = getSAM3XSetReg<38>(); info.clear_reg = getSAM3XClearReg<38>(); info.mask = getSAM3XMask<38>(); break;
        case 39: info.set_reg = getSAM3XSetReg<39>(); info.clear_reg = getSAM3XClearReg<39>(); info.mask = getSAM3XMask<39>(); break;
        case 40: info.set_reg = getSAM3XSetReg<40>(); info.clear_reg = getSAM3XClearReg<40>(); info.mask = getSAM3XMask<40>(); break;
        case 41: info.set_reg = getSAM3XSetReg<41>(); info.clear_reg = getSAM3XClearReg<41>(); info.mask = getSAM3XMask<41>(); break;
        case 42: info.set_reg = getSAM3XSetReg<42>(); info.clear_reg = getSAM3XClearReg<42>(); info.mask = getSAM3XMask<42>(); break;
        case 43: info.set_reg = getSAM3XSetReg<43>(); info.clear_reg = getSAM3XClearReg<43>(); info.mask = getSAM3XMask<43>(); break;
        case 44: info.set_reg = getSAM3XSetReg<44>(); info.clear_reg = getSAM3XClearReg<44>(); info.mask = getSAM3XMask<44>(); break;
        case 45: info.set_reg = getSAM3XSetReg<45>(); info.clear_reg = getSAM3XClearReg<45>(); info.mask = getSAM3XMask<45>(); break;
        case 46: info.set_reg = getSAM3XSetReg<46>(); info.clear_reg = getSAM3XClearReg<46>(); info.mask = getSAM3XMask<46>(); break;
        case 47: info.set_reg = getSAM3XSetReg<47>(); info.clear_reg = getSAM3XClearReg<47>(); info.mask = getSAM3XMask<47>(); break;
        case 48: info.set_reg = getSAM3XSetReg<48>(); info.clear_reg = getSAM3XClearReg<48>(); info.mask = getSAM3XMask<48>(); break;
        case 49: info.set_reg = getSAM3XSetReg<49>(); info.clear_reg = getSAM3XClearReg<49>(); info.mask = getSAM3XMask<49>(); break;
        case 50: info.set_reg = getSAM3XSetReg<50>(); info.clear_reg = getSAM3XClearReg<50>(); info.mask = getSAM3XMask<50>(); break;
        case 51: info.set_reg = getSAM3XSetReg<51>(); info.clear_reg = getSAM3XClearReg<51>(); info.mask = getSAM3XMask<51>(); break;
        case 52: info.set_reg = getSAM3XSetReg<52>(); info.clear_reg = getSAM3XClearReg<52>(); info.mask = getSAM3XMask<52>(); break;
        case 53: info.set_reg = getSAM3XSetReg<53>(); info.clear_reg = getSAM3XClearReg<53>(); info.mask = getSAM3XMask<53>(); break;
        case 54: info.set_reg = getSAM3XSetReg<54>(); info.clear_reg = getSAM3XClearReg<54>(); info.mask = getSAM3XMask<54>(); break;
        case 55: info.set_reg = getSAM3XSetReg<55>(); info.clear_reg = getSAM3XClearReg<55>(); info.mask = getSAM3XMask<55>(); break;
        case 56: info.set_reg = getSAM3XSetReg<56>(); info.clear_reg = getSAM3XClearReg<56>(); info.mask = getSAM3XMask<56>(); break;
        case 57: info.set_reg = getSAM3XSetReg<57>(); info.clear_reg = getSAM3XClearReg<57>(); info.mask = getSAM3XMask<57>(); break;
        case 58: info.set_reg = getSAM3XSetReg<58>(); info.clear_reg = getSAM3XClearReg<58>(); info.mask = getSAM3XMask<58>(); break;
        case 59: info.set_reg = getSAM3XSetReg<59>(); info.clear_reg = getSAM3XClearReg<59>(); info.mask = getSAM3XMask<59>(); break;
        case 60: info.set_reg = getSAM3XSetReg<60>(); info.clear_reg = getSAM3XClearReg<60>(); info.mask = getSAM3XMask<60>(); break;
        case 61: info.set_reg = getSAM3XSetReg<61>(); info.clear_reg = getSAM3XClearReg<61>(); info.mask = getSAM3XMask<61>(); break;
        case 62: info.set_reg = getSAM3XSetReg<62>(); info.clear_reg = getSAM3XClearReg<62>(); info.mask = getSAM3XMask<62>(); break;
        case 63: info.set_reg = getSAM3XSetReg<63>(); info.clear_reg = getSAM3XClearReg<63>(); info.mask = getSAM3XMask<63>(); break;
        case 64: info.set_reg = getSAM3XSetReg<64>(); info.clear_reg = getSAM3XClearReg<64>(); info.mask = getSAM3XMask<64>(); break;
        case 65: info.set_reg = getSAM3XSetReg<65>(); info.clear_reg = getSAM3XClearReg<65>(); info.mask = getSAM3XMask<65>(); break;
        case 66: info.set_reg = getSAM3XSetReg<66>(); info.clear_reg = getSAM3XClearReg<66>(); info.mask = getSAM3XMask<66>(); break;
        case 67: info.set_reg = getSAM3XSetReg<67>(); info.clear_reg = getSAM3XClearReg<67>(); info.mask = getSAM3XMask<67>(); break;
        case 68: info.set_reg = getSAM3XSetReg<68>(); info.clear_reg = getSAM3XClearReg<68>(); info.mask = getSAM3XMask<68>(); break;
        case 69: info.set_reg = getSAM3XSetReg<69>(); info.clear_reg = getSAM3XClearReg<69>(); info.mask = getSAM3XMask<69>(); break;
        case 70: info.set_reg = getSAM3XSetReg<70>(); info.clear_reg = getSAM3XClearReg<70>(); info.mask = getSAM3XMask<70>(); break;
        case 71: info.set_reg = getSAM3XSetReg<71>(); info.clear_reg = getSAM3XClearReg<71>(); info.mask = getSAM3XMask<71>(); break;
        case 72: info.set_reg = getSAM3XSetReg<72>(); info.clear_reg = getSAM3XClearReg<72>(); info.mask = getSAM3XMask<72>(); break;
        case 73: info.set_reg = getSAM3XSetReg<73>(); info.clear_reg = getSAM3XClearReg<73>(); info.mask = getSAM3XMask<73>(); break;
        case 74: info.set_reg = getSAM3XSetReg<74>(); info.clear_reg = getSAM3XClearReg<74>(); info.mask = getSAM3XMask<74>(); break;
        case 75: info.set_reg = getSAM3XSetReg<75>(); info.clear_reg = getSAM3XClearReg<75>(); info.mask = getSAM3XMask<75>(); break;
        case 76: info.set_reg = getSAM3XSetReg<76>(); info.clear_reg = getSAM3XClearReg<76>(); info.mask = getSAM3XMask<76>(); break;
        case 77: info.set_reg = getSAM3XSetReg<77>(); info.clear_reg = getSAM3XClearReg<77>(); info.mask = getSAM3XMask<77>(); break;
        case 78: info.set_reg = getSAM3XSetReg<78>(); info.clear_reg = getSAM3XClearReg<78>(); info.mask = getSAM3XMask<78>(); break;

        // Digix extended pins (90-113)
        case 90: info.set_reg = getSAM3XSetReg<90>(); info.clear_reg = getSAM3XClearReg<90>(); info.mask = getSAM3XMask<90>(); break;
        case 91: info.set_reg = getSAM3XSetReg<91>(); info.clear_reg = getSAM3XClearReg<91>(); info.mask = getSAM3XMask<91>(); break;
        case 92: info.set_reg = getSAM3XSetReg<92>(); info.clear_reg = getSAM3XClearReg<92>(); info.mask = getSAM3XMask<92>(); break;
        case 93: info.set_reg = getSAM3XSetReg<93>(); info.clear_reg = getSAM3XClearReg<93>(); info.mask = getSAM3XMask<93>(); break;
        case 94: info.set_reg = getSAM3XSetReg<94>(); info.clear_reg = getSAM3XClearReg<94>(); info.mask = getSAM3XMask<94>(); break;
        case 95: info.set_reg = getSAM3XSetReg<95>(); info.clear_reg = getSAM3XClearReg<95>(); info.mask = getSAM3XMask<95>(); break;
        case 96: info.set_reg = getSAM3XSetReg<96>(); info.clear_reg = getSAM3XClearReg<96>(); info.mask = getSAM3XMask<96>(); break;
        case 97: info.set_reg = getSAM3XSetReg<97>(); info.clear_reg = getSAM3XClearReg<97>(); info.mask = getSAM3XMask<97>(); break;
        case 98: info.set_reg = getSAM3XSetReg<98>(); info.clear_reg = getSAM3XClearReg<98>(); info.mask = getSAM3XMask<98>(); break;
        case 99: info.set_reg = getSAM3XSetReg<99>(); info.clear_reg = getSAM3XClearReg<99>(); info.mask = getSAM3XMask<99>(); break;
        case 100: info.set_reg = getSAM3XSetReg<100>(); info.clear_reg = getSAM3XClearReg<100>(); info.mask = getSAM3XMask<100>(); break;
        case 101: info.set_reg = getSAM3XSetReg<101>(); info.clear_reg = getSAM3XClearReg<101>(); info.mask = getSAM3XMask<101>(); break;
        case 102: info.set_reg = getSAM3XSetReg<102>(); info.clear_reg = getSAM3XClearReg<102>(); info.mask = getSAM3XMask<102>(); break;
        case 103: info.set_reg = getSAM3XSetReg<103>(); info.clear_reg = getSAM3XClearReg<103>(); info.mask = getSAM3XMask<103>(); break;
        case 104: info.set_reg = getSAM3XSetReg<104>(); info.clear_reg = getSAM3XClearReg<104>(); info.mask = getSAM3XMask<104>(); break;
        case 105: info.set_reg = getSAM3XSetReg<105>(); info.clear_reg = getSAM3XClearReg<105>(); info.mask = getSAM3XMask<105>(); break;
        case 106: info.set_reg = getSAM3XSetReg<106>(); info.clear_reg = getSAM3XClearReg<106>(); info.mask = getSAM3XMask<106>(); break;
        case 107: info.set_reg = getSAM3XSetReg<107>(); info.clear_reg = getSAM3XClearReg<107>(); info.mask = getSAM3XMask<107>(); break;
        case 108: info.set_reg = getSAM3XSetReg<108>(); info.clear_reg = getSAM3XClearReg<108>(); info.mask = getSAM3XMask<108>(); break;
        case 109: info.set_reg = getSAM3XSetReg<109>(); info.clear_reg = getSAM3XClearReg<109>(); info.mask = getSAM3XMask<109>(); break;
        case 110: info.set_reg = getSAM3XSetReg<110>(); info.clear_reg = getSAM3XClearReg<110>(); info.mask = getSAM3XMask<110>(); break;
        case 111: info.set_reg = getSAM3XSetReg<111>(); info.clear_reg = getSAM3XClearReg<111>(); info.mask = getSAM3XMask<111>(); break;
        case 112: info.set_reg = getSAM3XSetReg<112>(); info.clear_reg = getSAM3XClearReg<112>(); info.mask = getSAM3XMask<112>(); break;
        case 113: info.set_reg = getSAM3XSetReg<113>(); info.clear_reg = getSAM3XClearReg<113>(); info.mask = getSAM3XMask<113>(); break;

        default:
            // Invalid pin - return nullptr
            break;
    }

    return info;
}

} // namespace detail

//==============================================================================
// FastPinsSamePort<> Implementation
//==============================================================================

/// SAM3X same-port validation
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // SAM3X: Validate all pins on same PIO port (A/B/C/D)
    // Get SODR register address for first pin (each port has unique SODR address)
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

/// SAM3X same-port implementation
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // SAM3X: Use SODR and CODR registers for atomic writes
    volatile uint32_t* sodr = (volatile uint32_t*)mSodrReg;
    volatile uint32_t* codr = (volatile uint32_t*)mCodrReg;
    *sodr = set_mask;
    *codr = clear_mask;
}

/// Build LUT for SAM3X
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine PIO port (use first pin's SODR/CODR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mSodrReg = first_info.set_reg;
    mCodrReg = first_info.clear_reg;

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

//==============================================================================
// FastPinsWithClock<> Implementation
//==============================================================================

/// SAM3X: Validate clock and data pins are on same port
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(
    uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {

    // SAM3X: Clock and all data pins must be on same PIO port (A/B/C/D)
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

/// SAM3X: Build clock mask
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store clock mask and register pointers
    auto clock_info = detail::getPinInfo(clockPin);
    mClockMask = clock_info.mask;
    mClockSet = clock_info.set_reg;
    mClockClear = clock_info.clear_reg;
}

//==============================================================================
// FastPins<> Auto-Detect Implementation
//==============================================================================

/// SAM3X: Check if all pins are on same PIO port
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // SAM3X: Check if all pins are on same PIO port (A/B/C/D)
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

/// SAM3X: Same-port write implementation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // SAM3X: Same as FastPinsSamePort<>::writeImpl()
    volatile uint32_t* sodr = (volatile uint32_t*)mSodrReg;
    volatile uint32_t* codr = (volatile uint32_t*)mCodrReg;
    *sodr = set_mask;
    *codr = clear_mask;
}

/// SAM3X: Multi-port write implementation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // SAM3X: Sequential writes to multiple PIO ports (A/B/C/D)
    for (uint8_t i = 0; i < entry.port_count; i++) {
        volatile uint32_t* set_reg = (volatile uint32_t*)entry.ports[i].port_set;
        volatile uint32_t* clear_reg = (volatile uint32_t*)entry.ports[i].port_clear;

        *set_reg = entry.ports[i].set_mask;
        *clear_reg = entry.ports[i].clear_mask;
    }
}

/// SAM3X: Build same-port LUT
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine PIO port (use first pin's SODR/CODR registers)
    auto first_info = detail::getPinInfo(pins[0]);
    mSodrReg = first_info.set_reg;
    mCodrReg = first_info.clear_reg;

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

/// SAM3X: Build multi-port LUT
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Group pins by PIO port
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
        volatile uint32_t* ports[4] = {nullptr};  // Max 4 ports (A, B, C, D)
        uint8_t port_count = 0;

        // Identify unique ports (by register address)
        for (uint8_t bit = 0; bit < count; bit++) {
            volatile uint32_t* port = pin_info[bit].set_reg;
            bool found = false;
            for (uint8_t p = 0; p < port_count; p++) {
                if (ports[p] == port) {
                    found = true;
                    break;
                }
            }
            if (!found && port_count < 4) {
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
            mMultiPortLUT[pattern].ports[p].port_clear = pin_info[0].clear_reg;  // Use any pin's clear_reg
            // Better: Calculate CODR from SODR (offset depends on SAM3X register layout)
            // PIO_SODR is at offset 0x30, PIO_CODR is at offset 0x34
            mMultiPortLUT[pattern].ports[p].port_clear = (volatile uint32_t*)((uintptr_t)port_set + 4);
            mMultiPortLUT[pattern].ports[p].set_mask = set_mask;
            mMultiPortLUT[pattern].ports[p].clear_mask = clear_mask;
        }

        mMultiPortLUT[pattern].port_count = port_count;
    }
}

} // namespace fl

//==============================================================================
// Platform-Specific Storage Members
//==============================================================================

#if defined(__SAM3X8E__) || defined(ARDUINO_SAM_DUE) || defined(ARDUINO_ARCH_SAM)

namespace fl {

// FastPinsSamePort<> platform-specific members
template<uint8_t MAX_PINS>
class FastPinsSamePort {
private:
    volatile uint32_t* mSodrReg = nullptr;  // SAM3X: PIO_SODR register pointer
    volatile uint32_t* mCodrReg = nullptr;  // SAM3X: PIO_CODR register pointer
};

// FastPins<> platform-specific members
template<uint8_t MAX_PINS>
class FastPins {
private:
    volatile uint32_t* mSodrReg = nullptr;  // SAM3X: PIO_SODR register for same-port mode
    volatile uint32_t* mCodrReg = nullptr;  // SAM3X: PIO_CODR register for same-port mode
};

} // namespace fl

#endif // SAM3X platform detection

#endif // __SAM3X8E__

#endif // __INC_FAST_PINS_SAM3X_H
