// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTPIN_ARM_LPC11_LEGACY_H
#define __INC_FASTPIN_ARM_LPC11_LEGACY_H

#include "fl/stl/compiler_control.h"
#include "fl/system/fastpin_base.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

// =============================================================================
// FastPin for the legacy LPC11xx GPIO controller (LPC1110 / LPC1112 / LPC1114
// / LPC1115) -- NXP UM10398 sec. 9 / UM10462 sec. 9. Stage 4.1 (legacy) of
// FastLED #2845, tracked as FastLED #2878.
//
// Layout: per-port block at base + N*0x10000, N in 0..3.
//
//   - MASKED_ACCESS[mask] at offset 0x0000 -- writing to the
//     `base + (mask << 2)` address only updates the bits selected by `mask`.
//     This is the canonical fast hi()/lo() path: write `_MASK` to set those
//     bits, write `0` to clear them, both as a single store with no
//     read-modify-write needed in software.
//   - DATA      at offset 0x3FFC -- full-port read / write (read-modify-write).
//   - DIR       at offset 0x8000 -- direction (1 = output).
//
// The "modern" 0xA0000000 controller used by LPC8xx / LPC11Uxx / LPC15xx
// is in `fastpin_arm_lpc.h`; this header is its sibling for the legacy
// parts. Both share the same `clockless_arm_lpc.h` (M0 C++ implementation)
// because the clockless driver only consumes the `_ARMPIN` interface.
//
// Pin coverage: PIO0_0..PIO0_11 + PIO1_0..PIO1_11 + PIO2_0..PIO2_11 +
//               PIO3_0..PIO3_5 (LPC1114 LQFP48). Per-package availability is
//               enforced by hardware; the FastPin template fans out the
//               full grid and the linker keeps only what is used.
// =============================================================================

#if defined(FL_IS_ARM_LPC_11_LEGACY)

namespace fl {

#ifndef FL_LPC11_LEGACY_GPIO_BASE
#define FL_LPC11_LEGACY_GPIO_BASE 0x50000000UL
#endif
#ifndef FL_LPC11_LEGACY_GPIO_PORT_STRIDE
#define FL_LPC11_LEGACY_GPIO_PORT_STRIDE 0x10000UL
#endif

// Minimal per-port view of the legacy controller. The MCUXpresso CMSIS
// device header defines a fuller equivalent (LPC_GPIO0_TYPE etc.); when
// that header is on the include path the real struct is what code links
// against. This local mirror is only consulted when the device header
// could not be located, which is useful for host-side IDE introspection.
typedef struct {
    volatile u32 MASKED_ACCESS[4096];  // 0x0000..0x3FFC -- masked write/read
    volatile u32 DATA;                 // 0x3FFC alias also lives at MASKED[4095]
    volatile u32 RESERVED0[1023];      // pad to 0x8000
    volatile u32 DIR;                  // 0x8000
    volatile u32 IS;                   // 0x8004 -- interrupt sense (unused here)
    volatile u32 IBE;                  // 0x8008 -- interrupt both edges
    volatile u32 IEV;                  // 0x800C -- interrupt event
    volatile u32 IE;                   // 0x8010 -- interrupt enable
    volatile u32 RIS;                  // 0x8014 -- raw interrupt status
    volatile u32 MIS;                  // 0x8018 -- masked interrupt status
    volatile u32 IC;                   // 0x801C -- interrupt clear
} FL_LPC11_LEGACY_GPIO_Type;

#ifndef LPC_GPIO0
#define LPC_GPIO0 ((FL_LPC11_LEGACY_GPIO_Type*)(FL_LPC11_LEGACY_GPIO_BASE + 0u * FL_LPC11_LEGACY_GPIO_PORT_STRIDE))
#endif
#ifndef LPC_GPIO1
#define LPC_GPIO1 ((FL_LPC11_LEGACY_GPIO_Type*)(FL_LPC11_LEGACY_GPIO_BASE + 1u * FL_LPC11_LEGACY_GPIO_PORT_STRIDE))
#endif
#ifndef LPC_GPIO2
#define LPC_GPIO2 ((FL_LPC11_LEGACY_GPIO_Type*)(FL_LPC11_LEGACY_GPIO_BASE + 2u * FL_LPC11_LEGACY_GPIO_PORT_STRIDE))
#endif
#ifndef LPC_GPIO3
#define LPC_GPIO3 ((FL_LPC11_LEGACY_GPIO_Type*)(FL_LPC11_LEGACY_GPIO_BASE + 3u * FL_LPC11_LEGACY_GPIO_PORT_STRIDE))
#endif

// Map a FastLED logical pin number to the (port, bit) pair this controller
// uses. Each port carries 12 logical pins (the legacy LPC11xx ports have
// at most 12 PIO lines each per UM10398). The template parameter `PIN`
// is the 0-based linear index; ports 0..3 cover pins 0..47.
template <u8 PIN>
struct LpcLegacyPinAddr {
    static constexpr u8 PORT = static_cast<u8>(PIN / 12u);
    static constexpr u8 BIT  = static_cast<u8>(PIN % 12u);
};

// FastPin template for the legacy LPC11xx GPIO. The MASKED_ACCESS write
// at base + (mask<<2) only updates the bits in `mask`, so hi()/lo() are
// single-store operations with no read-modify-write -- the same property
// the SET/CLR registers give on the modern controller.
template <u8 PIN, u32 _MASK, u8 _PORT>
class _ARMPIN_LPC11LEGACY : public ValidPinBase {
public:
    typedef volatile u32* port_ptr_t;
    typedef u32           port_t;

    inline static FL_LPC11_LEGACY_GPIO_Type* gpio() __attribute__((always_inline)) {
        switch (_PORT) {
            case 1: return LPC_GPIO1;
            case 2: return LPC_GPIO2;
            case 3: return LPC_GPIO3;
            default: return LPC_GPIO0;
        }
    }

    inline static void setOutput() __attribute__((always_inline)) {
        gpio()->DIR |= _MASK;
    }
    inline static void setInput() __attribute__((always_inline)) {
        gpio()->DIR &= ~_MASK;
    }

    inline static void hi() __attribute__((always_inline)) {
        gpio()->MASKED_ACCESS[_MASK] = _MASK;
    }
    inline static void lo() __attribute__((always_inline)) {
        gpio()->MASKED_ACCESS[_MASK] = 0u;
    }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        gpio()->DATA = val;
    }

    inline static void strobe() __attribute__((always_inline)) { toggle(); toggle(); }
    inline static void toggle() __attribute__((always_inline)) {
        gpio()->DATA = gpio()->DATA ^ _MASK;
    }

    inline static void hi(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        *port = val;
    }

    inline static port_t hival() __attribute__((always_inline)) { return gpio()->DATA |  _MASK; }
    inline static port_t loval() __attribute__((always_inline)) { return gpio()->DATA & ~_MASK; }
    inline static port_ptr_t port()  __attribute__((always_inline)) { return &gpio()->DATA; }
    // sport()/cport() return the MASKED_ACCESS slot for this pin -- writing
    // _MASK to it raises the line, writing 0 lowers it. The clockless
    // driver uses sport() as HI_OFFSET=0 / cport() as LO_OFFSET=0, but
    // since both addresses are pin-specific the offsets land in the same
    // slot -- the driver passes `_MASK` and `0` as the values instead.
    inline static port_ptr_t sport() __attribute__((always_inline)) { return &gpio()->MASKED_ACCESS[_MASK]; }
    inline static port_ptr_t cport() __attribute__((always_inline)) { return &gpio()->MASKED_ACCESS[_MASK]; }
    inline static port_t     mask()  __attribute__((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__((always_inline)) {
        return (gpio()->MASKED_ACCESS[_MASK] & _MASK) != 0;
    }
};

#define _FL_DEFPIN_LPC11LEGACY(PIN) \
    template<> class FastPin<PIN> : public _ARMPIN_LPC11LEGACY<PIN, \
        (1u << (LpcLegacyPinAddr<PIN>::BIT)), LpcLegacyPinAddr<PIN>::PORT> {};

// PIO0_0..PIO0_11
#define MAX_PIN 47
_FL_DEFPIN_LPC11LEGACY(0);  _FL_DEFPIN_LPC11LEGACY(1);  _FL_DEFPIN_LPC11LEGACY(2);  _FL_DEFPIN_LPC11LEGACY(3);
_FL_DEFPIN_LPC11LEGACY(4);  _FL_DEFPIN_LPC11LEGACY(5);  _FL_DEFPIN_LPC11LEGACY(6);  _FL_DEFPIN_LPC11LEGACY(7);
_FL_DEFPIN_LPC11LEGACY(8);  _FL_DEFPIN_LPC11LEGACY(9);  _FL_DEFPIN_LPC11LEGACY(10); _FL_DEFPIN_LPC11LEGACY(11);
// PIO1_0..PIO1_11
_FL_DEFPIN_LPC11LEGACY(12); _FL_DEFPIN_LPC11LEGACY(13); _FL_DEFPIN_LPC11LEGACY(14); _FL_DEFPIN_LPC11LEGACY(15);
_FL_DEFPIN_LPC11LEGACY(16); _FL_DEFPIN_LPC11LEGACY(17); _FL_DEFPIN_LPC11LEGACY(18); _FL_DEFPIN_LPC11LEGACY(19);
_FL_DEFPIN_LPC11LEGACY(20); _FL_DEFPIN_LPC11LEGACY(21); _FL_DEFPIN_LPC11LEGACY(22); _FL_DEFPIN_LPC11LEGACY(23);
// PIO2_0..PIO2_11
_FL_DEFPIN_LPC11LEGACY(24); _FL_DEFPIN_LPC11LEGACY(25); _FL_DEFPIN_LPC11LEGACY(26); _FL_DEFPIN_LPC11LEGACY(27);
_FL_DEFPIN_LPC11LEGACY(28); _FL_DEFPIN_LPC11LEGACY(29); _FL_DEFPIN_LPC11LEGACY(30); _FL_DEFPIN_LPC11LEGACY(31);
_FL_DEFPIN_LPC11LEGACY(32); _FL_DEFPIN_LPC11LEGACY(33); _FL_DEFPIN_LPC11LEGACY(34); _FL_DEFPIN_LPC11LEGACY(35);
// PIO3_0..PIO3_11 (LPC1115 / LPC1114 LQFP48 only)
_FL_DEFPIN_LPC11LEGACY(36); _FL_DEFPIN_LPC11LEGACY(37); _FL_DEFPIN_LPC11LEGACY(38); _FL_DEFPIN_LPC11LEGACY(39);
_FL_DEFPIN_LPC11LEGACY(40); _FL_DEFPIN_LPC11LEGACY(41); _FL_DEFPIN_LPC11LEGACY(42); _FL_DEFPIN_LPC11LEGACY(43);
_FL_DEFPIN_LPC11LEGACY(44); _FL_DEFPIN_LPC11LEGACY(45); _FL_DEFPIN_LPC11LEGACY(46); _FL_DEFPIN_LPC11LEGACY(47);

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl

#endif  // FL_IS_ARM_LPC_11_LEGACY

FL_DISABLE_WARNING_POP

#endif  // __INC_FASTPIN_ARM_LPC11_LEGACY_H
