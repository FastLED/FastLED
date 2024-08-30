
#ifndef __GET_SYS_TICKS_H__
#define __GET_SYS_TICKS_H__

// only include this for the cortex m0 platform
#ifdef ___ARM_ARCH_6M__

// Stripped down version of <core_mo0.h> for the SysTick structure. This is needed
// by some code for the cortex m0

#include <stdint.h>

// Define the SysTick base address
#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                    /*!< SysTick Base Address */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct */

#ifdef __cplusplus
extern "C" {
#endif

// Define the SysTick structure
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile const uint32_t CALIB;
} SysTick_Type;

// Define the SysTick
#define SysTick ((SysTick_Type *)SysTick_BASE)


#ifdef __cplusplus
}
#endif

inline uint32_t _get_sys_ticks() {
    return SysTick->VAL;
}

#endif  // ___ARM_ARCH_6M__

#endif  // __GET_SYS_TICKS_H__
