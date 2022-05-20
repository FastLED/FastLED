#ifndef __INC_FASTLED_ARM_D21_H
#define __INC_FASTLED_ARM_D21_H

#include "fastpin_arm_d21.h"
#ifdef FASTLED_ARM_M0_DMA
    #include "clockless_arm_d21_dma.h"
#else
    #include "clockless_arm_d21.h"
#endif

#endif
