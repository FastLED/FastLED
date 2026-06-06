// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTPIN_ARM_LPC_H
#define __INC_FASTPIN_ARM_LPC_H

#include "fl/stl/compiler_control.h"
#include "fl/system/fastpin_base.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

// Scoped to LPC8xx (LPC845 / LPC804) only. The family-level FL_IS_ARM_LPC
// gate also matches LPC11xx (M0) and LPC15xx (M3) detected via #2849 / #2859,
// but those families have different GPIO controller layouts (legacy GPIO at
// 0x50000000 with masked-access semantics for LPC11xx per UM10398/UM10462;
// LPC15xx GPIO per UM11074 with a different register layout). The driver
// wiring for those families is tracked in #2845 Stage 4.
#if defined(FL_LPC845) || defined(FL_LPC804)

namespace fl {

// Minimal LPC8xx GPIO controller view used by the FastPin templates. The
// MCUXpresso CMSIS device header defines an equivalent LPC_GPIO_Type; when
// that header is on the include path (i.e. inside led_sysdefs_arm_lpc.h via
// <LPC845.h>) the real struct is what code links against. This local mirror
// is only consulted when the device header could not be located, which is
// useful for host-side IDE introspection.
#ifndef FL_LPC_GPIO_BASE
#define FL_LPC_GPIO_BASE 0xA0000000UL
#endif

typedef struct {
    volatile u8  B[2][32];         // 0x0000 byte-pin access
    volatile u32 W[2][32];         // 0x1000 word-pin access (0x80 bytes per port)
    volatile u32 RESERVED0[1024 - 256];
    volatile u32 DIR[2];           // 0x2000
    volatile u32 RESERVED1[30];
    volatile u32 MASK[2];          // 0x2080
    volatile u32 RESERVED2[30];
    volatile u32 PIN[2];           // 0x2100
    volatile u32 RESERVED3[30];
    volatile u32 MPIN[2];          // 0x2180
    volatile u32 RESERVED4[30];
    volatile u32 SET[2];           // 0x2200
    volatile u32 RESERVED5[30];
    volatile u32 CLR[2];           // 0x2280
    volatile u32 RESERVED6[30];
    volatile u32 NOT[2];           // 0x2300
    volatile u32 RESERVED7[30];
    volatile u32 DIRSET[2];        // 0x2380
    volatile u32 RESERVED8[30];
    volatile u32 DIRCLR[2];        // 0x2400
    volatile u32 RESERVED9[30];
    volatile u32 DIRNOT[2];        // 0x2480
} FL_LPC_GPIO_Type;

// Use the CMSIS-supplied LPC_GPIO pointer when available; fall back to our
// own typed pointer when only the local struct is in scope.
#ifndef LPC_GPIO
#define LPC_GPIO ((FL_LPC_GPIO_Type*)FL_LPC_GPIO_BASE)
#endif

// FastPin template for LPC8xx pins. PORT0 is the only port populated on
// LPC845/LPC804 packages currently supported.
template <u8 PIN, u32 _MASK>
class _ARMPIN : public ValidPinBase {
public:
    typedef volatile u32* port_ptr_t;
    typedef u32           port_t;

    inline static void setOutput() __attribute__((always_inline)) {
        LPC_GPIO->DIRSET[0] = _MASK;
    }
    inline static void setInput() __attribute__((always_inline)) {
        LPC_GPIO->DIRCLR[0] = _MASK;
    }

    inline static void hi() __attribute__((always_inline)) {
        LPC_GPIO->SET[0] = _MASK;
    }
    inline static void lo() __attribute__((always_inline)) {
        LPC_GPIO->CLR[0] = _MASK;
    }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        LPC_GPIO->PIN[0] = val;
    }

    inline static void strobe() __attribute__((always_inline)) { toggle(); toggle(); }
    inline static void toggle() __attribute__((always_inline)) { LPC_GPIO->NOT[0] = _MASK; }

    inline static void hi(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        *port = val;
    }

    inline static port_t hival() __attribute__((always_inline)) { return LPC_GPIO->PIN[0] |  _MASK; }
    inline static port_t loval() __attribute__((always_inline)) { return LPC_GPIO->PIN[0] & ~_MASK; }
    // Expose PIN[0] as the "port" so the clockless C++ driver can derive the
    // SET / CLR offsets at compile time (it uses HI_OFFSET / LO_OFFSET as
    // byte offsets from this pointer).
    inline static port_ptr_t port()  __attribute__((always_inline)) { return &LPC_GPIO->PIN[0]; }
    inline static port_ptr_t sport() __attribute__((always_inline)) { return &LPC_GPIO->SET[0]; }
    inline static port_ptr_t cport() __attribute__((always_inline)) { return &LPC_GPIO->CLR[0]; }
    inline static port_t     mask()  __attribute__((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__((always_inline)) {
        return (LPC_GPIO->PIN[0] & _MASK) != 0;
    }
};

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _ARMPIN<PIN, (1u << (PIN))> {};

// LPC845 (HVQFN48 / HVQFN33 / TSSOP20) and LPC804 expose PIO0_0..PIO0_31.
// Pin availability per package is enforced by hardware; the template just
// instantiates the full 32-pin grid and the linker keeps only what is used.
#define MAX_PIN 31
_FL_DEFPIN(0);  _FL_DEFPIN(1);  _FL_DEFPIN(2);  _FL_DEFPIN(3);
_FL_DEFPIN(4);  _FL_DEFPIN(5);  _FL_DEFPIN(6);  _FL_DEFPIN(7);
_FL_DEFPIN(8);  _FL_DEFPIN(9);  _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl

#endif  // FL_LPC845 || FL_LPC804

FL_DISABLE_WARNING_POP

#endif  // __INC_FASTPIN_ARM_LPC_H
