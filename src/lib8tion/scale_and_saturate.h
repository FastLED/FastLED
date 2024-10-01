#pragma once

#include "crgb.h"
#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// These are function designed to saturate one value while stealing value from the other.
// This is useful for brightness stealing where software brightness needs to be maximized
// while hardware brightness can be minimized (because it's free).


// Scale and saturate the 16-bit unsigned integer a using the scaling factor b 
// (8-bit unsigned integer). This function modifies both a and b in place.
//
// Parameters:
//   - a: Pointer to a 16-bit unsigned integer to be scaled and saturated.
//   - b: Pointer to an 8-bit unsigned integer scaling factor.
//
// Returns:
//   - True if a reaches its maximum value (0xFFFF), otherwise false. The function 
//     modifies a in place by increasing it based on b, but it never exceeds 0xFFFF. 
//     It also adjusts b but ensures it does not drop below 1.
bool scale_and_saturate(uint16_t *a, uint8_t *b);

// Scale and saturate the 8-bit unsigned integer a using the scaling factor b 
// (8-bit unsigned integer). This function modifies both a and b in place.
//
// Parameters:
//   - a: Pointer to an 8-bit unsigned integer to be scaled and saturated.
//   - b: Pointer to an 8-bit unsigned integer scaling factor.
//
// Returns:
//   - True if a reaches its maximum value (0xFF), otherwise false. The function 
//     modifies a in place by increasing it based on b, but it never exceeds 0xFF. 
//     It also adjusts b but ensures it does not drop below 1.
bool scale_and_saturate(uint8_t *a, uint8_t *b);

// Scale and saturate the 8-bit unsigned integer a using a 5-bit precision scaling 
// factor for b. The value of b is clamped between 1 and 31 (5-bit range).
//
// Parameters:
//   - a: Pointer to an 8-bit unsigned integer to be scaled and saturated.
//   - b: Pointer to an 8-bit unsigned integer treated as having 5-bit precision 
//        (values between 1 and 31).
//
// Returns:
//   - True if a reaches its maximum value (0xFF), otherwise false. The function 
//     modifies both a and b in place, clamping b to the 5-bit range, and ensures 
//     a does not exceed 0xFF.
bool scale_and_saturate_with_5bit_b(uint8_t *a, uint8_t *b);

// Scale and saturate the 16-bit unsigned integer a using a 5-bit precision scaling 
// factor for b. The value of b is clamped between 1 and 31 (5-bit range).
//
// Parameters:
//   - a: Pointer to a 16-bit unsigned integer to be scaled and saturated.
//   - b: Pointer to an 8-bit unsigned integer treated as having 5-bit precision 
//        (values between 1 and 31).
//
// Returns:
//   - True if a reaches its maximum value (0xFFFF), otherwise false. The function 
//     modifies both a and b in place, clamping b to the 5-bit range, and ensures 
//     a does not exceed 0xFFFF.
bool scale_and_saturate_with_5bit_b(uint16_t *a, uint8_t *b);


bool scale_and_saturate(uint16_t largest_component, uint8_t* b, CRGB* out);


FASTLED_NAMESPACE_END
