#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/led_sysdefs_esp8266.h"

namespace fl {
#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern u32 _frame_cnt;
extern u32 _retry_cnt;
#endif

namespace esp8266_clockless {

__attribute__((always_inline)) inline u32 cycles() FL_NO_EXCEPT {
    u32 cyc;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(cyc));
    return cyc;
}

struct BitTiming {
    u32 t1;
    u32 t2;
    u32 t3;
};

// Bit-bangs one pixel (three bytes, MSB first) with interrupts already
// masked. It is a plain function, not a template, because GCC ignores
// section attributes on template instantiations: the core's linker script
// then maps them to flash and the first frame after other work stalls on
// instruction-cache misses mid-bit. Returns false if a bit overran.
bool sendPixel(u32& last_mark, const u8* bytes, u32 bitsPerByte,
                       u8 pin, const BitTiming& t) FL_NO_EXCEPT;

}  // namespace esp8266_clockless
}  // namespace fl
