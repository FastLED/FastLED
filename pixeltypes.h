#ifndef __INC_PIXELS_H
#define __INC_PIXELS_H

#include <stdint.h>


struct CRGB {
	union {
		struct { uint8_t r; uint8_t g; uint8_t b; };
		uint8_t raw[3];
	};
};

#ifdef SUPPORT_ARGB
struct CARGB {
	union {
		struct { uint8_t a; uint8_t g; uint8_t r; uint8_t b; };
		uint8_t raw[4];
		uint32_t all32;
	};
};

#endif


struct CHSV {
    union {
        uint8_t hue;
        uint8_t h; };
    union {
        uint8_t saturation;
        uint8_t sat;
        uint8_t s; };
    union {
        uint8_t value;
        uint8_t val;
        uint8_t v; };
};


// Define RGB orderings
enum EOrder {
	RGB=0012,
	RBG=0021,
	GRB=0102,
	GBR=0120,
	BRG=0201,
	BGR=0210
};


#endif
