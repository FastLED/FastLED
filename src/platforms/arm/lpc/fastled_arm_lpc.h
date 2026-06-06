// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTLED_ARM_LPC_H
#define __INC_FASTLED_ARM_LPC_H

#include "platforms/arm/lpc/fastpin_arm_lpc.h"
#include "platforms/arm/lpc/clockless_arm_lpc.h"
// LPC804-specific PLU clockless driver. Build-time opt-in via FASTLED_LPC_PLU.
// The header self-gates with `#if defined(FL_LPC804) && defined(FASTLED_LPC_PLU)`
// and the bit-banged driver above compiles itself out under the same gate,
// so the LPC845 path is completely unaffected.
#include "platforms/arm/lpc/clockless_arm_lpc_plu.h"

#endif
