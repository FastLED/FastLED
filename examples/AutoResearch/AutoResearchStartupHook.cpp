#include "fl/system/sketch_macros.h"

#if defined(FL_IS_TEENSY_4X) && defined(__IMXRT1062__)

#include <Arduino.h>

// Disable the iMXRT1062 RTWDOG before the Arduino runtime starts.
//
// WDOG3 can already be armed when this sketch boots, and a short prior timeout
// can reset the chip before USB enumeration or setup() runs. Teensyduino exposes
// startup_early_hook as a weak symbol specifically for this kind of early reset
// recovery. Keep the override in this .cpp file: Arduino prototype generation
// can give .ino prototypes C++ linkage, which conflicts with the C-linkage weak
// symbol and breaks fbuild deploy.
extern "C" FLASHMEM void startup_early_hook(void) {
    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    WDOG3_CNT = 0xD928C520u;

    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_ULK)) {
            if (++spin > 200000u) {
                break;
            }
        }
    }

    WDOG3_TOVAL = 0xFFFFu;
    WDOG3_WIN = 0;
    WDOG3_CS = WDOG_CS_CMD32EN | WDOG_CS_UPDATE | WDOG_CS_CLK(1);

    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_RCS)) {
            if (++spin > 200000u) {
                break;
            }
        }
    }
}

#endif
