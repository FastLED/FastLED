#ifndef __INC_PLATFORMS_H
#define __INC_PLATFORMS_H

#include "fastled_config.h"

#if defined(__MK20DX128__) || defined(__MK20DX256__)
// Include k20/T3 headers
#include "platforms/arm/k20/fastled_arm_k20.h"
#elif defined(__MKL26Z64__)
// Include k26/T-LC headers
#include "platforms/arm/k26/fastled_arm_k26.h"
#elif defined(__SAM3X8E__)
// Include sam/due headers
#include "platforms/arm/sam/fastled_arm_sam.h"
#else
// AVR platforms
#include "platforms/avr/fastled_avr.h"
#endif

#endif
