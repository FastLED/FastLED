#ifndef _FIVE_BIT_HD_GAMMA_H_
#define _FIVE_BIT_HD_GAMMA_H_

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

enum FiveBitGammaCorrectionMode {
	kFiveBitGammaCorrectionMode_Null = 0,
	kFiveBitGammaCorrectionMode_BitShift = 1
};

// Applies gamma correction for the RGBV(8, 8, 8, 5) color space, where
// the last byte is the brightness byte at 5 bits.
// To override this five_bit_hd_gamma_bitshift you'll need to define
// FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE in your build settings
// then define the function anywhere in your project.
// Example:
//  FASTLED_NAMESPACE_BEGIN
//  void five_bit_hd_gamma_bitshift(
//      uint8_t r8, uint8_t g8, uint8_t b8,
//      uint8_t r8_scale, uint8_t g8_scale, uint8_t b8_scale,
//      uint8_t* out_r8,
//      uint8_t* out_g8,
//      uint8_t* out_b8,
//      uint8_t* out_power_5bit) {
//        cout << "hello world\n";
//  }
//  FASTLED_NAMESPACE_END

#ifdef FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE
// This function is located somewhere else in your project, so it's declared
// extern here.
extern void five_bit_hd_gamma_bitshift(
		uint8_t r8, uint8_t g8, uint8_t b8,
		uint8_t r8_scale, uint8_t g8_scale, uint8_t b8_scale,
		uint8_t* out_r8,
		uint8_t* out_g8,
		uint8_t* out_b8,
		uint8_t* out_power_5bit);
#else
void five_bit_hd_gamma_bitshift(
    uint8_t r8, uint8_t g8, uint8_t b8,
    uint8_t r8_scale, uint8_t g8_scale, uint8_t b8_scale,
    uint8_t* out_r8,
    uint8_t* out_g8,
    uint8_t* out_b8,
    uint8_t* out_power_5bit);
#endif  // FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE

// Simple gamma correction function that converts from
// 8-bit color component and converts it to gamma corrected 16-bit
// color component. Fast and no memory overhead!
// To override this function you'll need to define FASTLED_FIVE_BIT_HD_GAMMA_BITSHIFT_FUNCTION_OVERRIDE
// in your build settings and then define your own version anywhere in your project.
// Example:
//  FASTLED_NAMESPACE_BEGIN
//  void five_bit_hd_gamma_function(
//    uint8_t r8, uint8_t g8, uint8_t b8,
//    uint16_t* r16, uint16_t* g16, uint16_t* b16) {
//      cout << "hello world\n";
//  }
//  FASTLED_NAMESPACE_END
#ifdef FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_OVERRIDE
// This function is located somewhere else in your project, so it's declared
// extern here.
extern void five_bit_hd_gamma_function(
	uint8_t r8, uint8_t g8, uint8_t b8,
	uint16_t* r16, uint16_t* g16, uint16_t* b16);
#else
void five_bit_hd_gamma_function(
  uint8_t r8, uint8_t g8, uint8_t b8,
  uint16_t* r16, uint16_t* g16, uint16_t* b16);
#endif	// FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_OVERRIDE
FASTLED_NAMESPACE_END

#endif // _FIVE_BIT_HD_GAMMA_H_
