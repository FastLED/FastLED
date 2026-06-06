// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTLED_ARM_LPC_H
#define __INC_FASTLED_ARM_LPC_H

#include "platforms/arm/lpc/fastpin_arm_lpc.h"

// LPC family driver-wiring status (#2845 Stage 4 items 1 + 2 land here):
//   FL_LPC845 / FL_LPC804  — supported (Stage 2a #2837, optional Stage 2b/2c)
//   FL_LPC11_USB           — supported via shared LPC8xx fastpin + M0
//                            clockless path. UM10462 §9 confirms the
//                            LPC11Uxx GPIO controller layout is identical
//                            to LPC8xx (DIR/MASK/PIN/SET/CLR/NOT at the
//                            same offsets). The M0-vs-M0+ ISA swap is
//                            handled in led_sysdefs_arm_lpc.h via
//                            FL_IS_ARM_M0 (not _PLUS), and the C++
//                            clockless implementation is already used
//                            (FASTLED_M0_USE_C_IMPLEMENTATION), so the
//                            single-instruction encoding limit of plain
//                            M0 doesn't bite us.
//   FL_LPC15               — supported via shared LPC8xx fastpin + the
//                            same C++ clockless path. UM11074 confirms
//                            the same modern GPIO controller at
//                            0xA0000000. M3 has DWT cycle counters and
//                            full Thumb-2 encoding (no offset limit), but
//                            the unified C++ path keeps the driver
//                            surface consistent.
//   FL_LPC11_LEGACY        — NOT supported. LPC1110/1112/1114/1115 use
//                            the legacy GPIO controller at 0x50000000
//                            with 12-bit masked-access semantics
//                            (UM10398). The fastpin header emits a
//                            #error if this family is detected without
//                            an overlapping modern variant. Driver
//                            wiring is a follow-up.

// LPC845 has an optional PWM+DMA-to-GPIO clockless driver (Stage 2c of #2836,
// see #2842). It is opt-in via FASTLED_LPC_PWM_DMA=1 because it consumes the
// SCT plus 3 DMA channels - a global resource users may want for other
// peripherals. When the macro is set on an LPC845 build, the PWM+DMA driver
// supplies fl::ClocklessController; otherwise the Stage 2a bit-bang driver in
// clockless_arm_lpc.h remains the default.
#if defined(FL_LPC845) && defined(FASTLED_LPC_PWM_DMA)
#include "platforms/arm/lpc/clockless_arm_lpc_pwm_dma.h"
#else
#include "platforms/arm/lpc/clockless_arm_lpc.h"
// LPC804-specific PLU clockless driver (Stage 2b of #2836, see #2841).
// Build-time opt-in via FASTLED_LPC_PLU. The header self-gates with
// `#if defined(FL_LPC804) && defined(FASTLED_LPC_PLU)` and the bit-banged
// driver above compiles itself out under the same gate, so the LPC845 path
// is completely unaffected.
#include "platforms/arm/lpc/clockless_arm_lpc_plu.h"
#endif

// LPC8xx hardware SPI driver — APA102 / SK9822 / WS2801 strip support.
// Stage 4 item 3 of #2845. Self-gated to LPC845 / LPC804; LPC11xx /
// LPC15xx variants have different SPI controllers and are not yet wired.
#include "platforms/arm/lpc/spi_arm_lpc.h"

#endif
