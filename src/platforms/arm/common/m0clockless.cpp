
#ifdef __ARM_ARCH_6M__
#include <core_cm0plus.h>

#include <stdint.h>

#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

uint32_t _get_sys_ticks() {
    return SysTick->VAL;
}

FASTLED_NAMESPACE_END

#endif  // __ARM_ARCH_6M__
