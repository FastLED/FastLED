// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTLED_ARM_LPC_H
#define __INC_FASTLED_ARM_LPC_H

#include "platforms/arm/lpc/fastpin_arm_lpc.h"

// LPC11xx (Cortex-M0) and LPC15xx (Cortex-M3) detection scaffolds shipped in
// #2849 / #2859 (#2845 Stage 4). Driver wiring for those families is a
// follow-up - the existing LPC8xx fastpin / clockless headers target the
// LPC8xx GPIO controller at 0xA0000000 (SET / CLR registers) and the M0+ ASM
// path, neither of which applies to LPC11xx (legacy GPIO at 0x50000000,
// masked-access semantics, M0 instruction encoding) or LPC15xx (different
// GPIO layout per UM11074, M3 instruction set). Emit a clear error rather
// than silently compiling LPC8xx-targeted code for the wrong family.
#if (defined(FL_LPC11) || defined(FL_LPC15)) && \
    !(defined(FL_LPC845) || defined(FL_LPC804))
#error "FastLED LPC11xx / LPC15xx driver wiring is a follow-up to #2849 / #2859, tracked in #2845 Stage 4. The LPC8xx clockless driver targets a different GPIO controller and cannot be used on LPC11xx (Cortex-M0, legacy GPIO at 0x50000000) or LPC15xx (Cortex-M3, GPIO per UM11074). Either contribute the family-specific driver via #2845 or revert to a supported LPC variant."
#endif

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
