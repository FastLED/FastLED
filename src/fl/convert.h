#pragma once

#include "fl/stdint.h"

#include "fl/int.h"
// Conversion from FastLED timings to the type found in datasheets.
inline void convert_fastled_timings_to_timedeltas(fl::u16 T1, fl::u16 T2,
                                                  fl::u16 T3, fl::u16 *T0H,
                                                  fl::u16 *T0L, fl::u16 *T1H,
                                                  fl::u16 *T1L) {
    *T0H = T1;
    *T0L = T2 + T3;
    *T1H = T1 + T2;
    *T1L = T3;
}
