#include "test.h"

// Model the CMSIS core marker without CoreDebug_BASE (CMSIS 6 on M33).
// No register types are mocked: this checks that the fallback emits nothing.
#define __CORTEX_M 33U
#include "platforms/arm/stm32/cm3_regs.h"

FL_TEST_CASE("STM32 CMSIS core suppresses legacy register definitions") {
#ifdef CoreDebug_BASE
    FL_FAIL("CMSIS must not acquire a fallback CoreDebug register map");
#endif
#ifdef DWT_BASE
    FL_FAIL("CMSIS must not acquire a fallback DWT register map");
#endif
}

#undef __CORTEX_M
