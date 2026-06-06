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

// Scoped to chips that share the "modern" LPC GPIO controller (DIR / MASK /
// PIN / MPIN / SET / CLR / NOT register layout at 0xA0000000):
//   - LPC8xx       (LPC845 / LPC804)            — UM11029 / UM11065
//   - LPC11Uxx     (LPC11U24 / LPC11U35)        — UM10462 §9   (#2845 Stage 4.1)
//   - LPC15xx      (LPC15{17,18,19,47,48,49})   — UM11074      (#2845 Stage 4.2)
//
// Out of scope here: LPC11xx legacy parts (LPC1110 / LPC1112 / LPC1114 /
// LPC1115) which use the older "legacy GPIO" controller at 0x50000000 with
// 12-bit masked-access semantics (UM10398). Those need their own fastpin
// layout; emit a clear `#error` rather than silently mis-encoding.
#if defined(FL_LPC11_LEGACY) && \
    !(defined(FL_LPC845) || defined(FL_LPC804) || \
      defined(FL_LPC11_USB) || defined(FL_LPC15))
#error "FastLED LPC1110/1112/1114/1115 (legacy M0 GPIO at 0x50000000) driver wiring is a Stage 4 follow-up to #2845. The modern LPC8xx-style fastpin in this file does not apply. Either target LPC11U24/U35 / LPC845 / LPC804 / LPC15xx, or contribute the legacy-GPIO fastpin via #2845."
#endif

#if defined(FL_LPC845) || defined(FL_LPC804) || \
    defined(FL_LPC11_USB) || defined(FL_LPC15)

namespace fl {

// Minimal "modern" LPC GPIO controller view used by the FastPin templates
// (LPC8xx/LPC11Uxx/LPC15xx share the same DIR/MASK/PIN/SET/CLR/NOT layout at
// 0xA0000000 — legacy LPC11xx at 0x50000000 is gated out above). The
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

// FastPin template for "modern" LPC GPIO pins (LPC8xx / LPC11Uxx / LPC15xx).
// PORT0 is the primary port populated on the LPC845/LPC804 packages this
// driver covers today; multi-port support is per-variant and not currently
// wired up.
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

#endif  // FL_LPC845 || FL_LPC804 || FL_LPC11_USB || FL_LPC15

FL_DISABLE_WARNING_POP

#endif  // __INC_FASTPIN_ARM_LPC_H
