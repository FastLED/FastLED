// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTLED_ARM_LPC_H
#define __INC_FASTLED_ARM_LPC_H

#include "platforms/arm/lpc/fastpin_arm_lpc.h"

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

#endif
