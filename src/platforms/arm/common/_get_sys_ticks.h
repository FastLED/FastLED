
#ifdef __GET_SYS_TICKS_H__
#define __GET_SYS_TICKS_H__
// #include <core_cm0plus.h>  // This seems to be missing?

#include <stdint.h>

inline uint32_t _get_sys_ticks() {
    return SysTick->VAL;
}


#endif  // __ARM_ARCH_6M__
