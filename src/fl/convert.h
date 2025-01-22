#pragma once

#include <stdint.h>

// Conversion from FastLED timings to the type found in datasheets.
inline void convert_fastled_timings_to_timedeltas(
	uint16_t T1, uint16_t T2, uint16_t T3,
	uint16_t* T0H, uint16_t* T0L, uint16_t* T1H, uint16_t* T1L) {
    *T0H = T1;
    *T0L = T2 + T3;
    *T1H = T1 + T2;
    *T1L = T3;
}