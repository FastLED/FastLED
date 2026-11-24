#include "test.h"
#include "fl/stl/cstddef.h"

// Peripheral macros must survive inclusion without expanding as DWT members.
#define COMP0 (0x10UL)
#define COMP1 (0x20UL)
#define COMP2 (0x30UL)
#define COMP3 (0x40UL)
#include "platforms/arm/stm32/cm3_regs.h"

FL_TEST_CASE("STM32 legacy debug register layout and macro coexistence") {
    FL_CHECK_EQ(FL_OFFSETOF(CoreDebug_Type, DEMCR), 0x0c);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, CTRL), 0x00);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, CYCCNT), 0x04);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, DWT_COMP0), 0x20);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, DWT_COMP1), 0x30);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, DWT_COMP2), 0x40);
    FL_CHECK_EQ(FL_OFFSETOF(DWT_Type, DWT_COMP3), 0x50);
    FL_CHECK_EQ(sizeof(CoreDebug_Type), 0x10);
    FL_CHECK_EQ(sizeof(DWT_Type), 0x5c);
    FL_CHECK_EQ(COMP0, 0x10UL);
    FL_CHECK_EQ(COMP1, 0x20UL);
    FL_CHECK_EQ(COMP2, 0x30UL);
    FL_CHECK_EQ(COMP3, 0x40UL);
}

#undef COMP0
#undef COMP1
#undef COMP2
#undef COMP3
