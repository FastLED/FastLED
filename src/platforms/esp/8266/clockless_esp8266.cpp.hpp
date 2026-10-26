#pragma once

// IWYU pragma: private
// ok no namespace fl
#include "platforms/esp/is_esp.h" // IWYU pragma: keep

#ifdef FL_IS_ESP8266

#include "platforms/esp/8266/clockless_esp8266_bits.h"

// IWYU pragma: begin_keep
#include <esp8266_peri.h>  // SDK GPIO register map
// IWYU pragma: end_keep

namespace fl {
namespace esp8266_clockless {

bool IRAM_ATTR sendPixel(u32& last_mark, const u8* bytes, u32 bitsPerByte,
                       u8 pin, const BitTiming& t) FL_NO_EXCEPT {
    const u32 t1 = t.t1;
    const u32 t12 = t.t1 + t.t2;
    const u32 t123 = t.t1 + t.t2 + t.t3;
    const u32 mask = 1u << (pin & 0x0F);
    const bool gpio16 = pin >= 16;
    for (int byte = 0; byte < 3; ++byte) {
        u32 b = ~(static_cast<u32>(bytes[byte]) << 24);
        for (u32 i = bitsPerByte; i > 0; --i) {
            while ((cycles() - last_mark) < t123) {
            }
            last_mark = cycles();
            if (gpio16) { GP16O = 1; } else { GPOS = mask; }
            while ((cycles() - last_mark) < t1) {
            }
            if (b & 0x80000000u) {
                if (gpio16) { GP16O = 0; } else { GPOC = mask; }
            }
            b <<= 1;
            while ((cycles() - last_mark) < t12) {
            }
            if (gpio16) { GP16O = 0; } else { GPOC = mask; }
            // Even with interrupts masked the NMI can stretch a bit; abort
            // the frame rather than send a corrupted one.
            if ((cycles() - last_mark) >= t123 - 5) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace esp8266_clockless
}  // namespace fl

#endif  // FL_IS_ESP8266
