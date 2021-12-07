#ifndef __INC_LIGHTNESS_LUT_H
#define __INC_LIGHTNESS_LUT_H

#include "FastLED.h"

///@file lut.h
/// contains a lightness look up table for chipsets that supports a higher range than 8 bits.

FASTLED_NAMESPACE_BEGIN

// LightnessLUT defines a lightness look up table at compile time to map 8 bits per
// channel RGB data to a higher range when a display supports more than 8 bits
// per channel.
template<uint16_t maxOut>
struct LightnessLUT {
public:
	// Init initiates a lookup table with the maximum intensity i.
	void init(uint8_t i) {
		if (last == i) {
			return;
		}
		// linearCutOff defines the linear section of the curve. Inputs between
		// [0, linearCutOff] are mapped linearly to the output. It is 1% of maximum
		// output.
		const uint16_t max = (maxOut * i + 127) / 255;
		const uint32_t linearCutOff = uint32_t((max + 50) / 100);
		const uint32_t inRange = 255 - linearCutOff;
		const uint32_t outRange = uint32_t(max) - linearCutOff;
		const uint32_t offset = inRange >> 1;
		for (uint32_t l = 1; l < 256; l++) {
			if (l < linearCutOff) {
				data[l] = uint16_t(l);
			} else {
				uint16_t v = l-linearCutOff;
				data[l] = uint16_t((((v*v*v + offset) / inRange)*outRange+(offset*offset))/inRange/inRange + linearCutOff);
			}
		}
		last = i;
	}
	void init(const LightnessLUT& rhs) {
		if (last == rhs.last) {
			return;
		}
		memcpy(data, rhs.data, sizeof(data));
		last = rhs.last;
	}

	uint16_t data[255];
	uint8_t last;
};

FASTLED_NAMESPACE_END

#endif
