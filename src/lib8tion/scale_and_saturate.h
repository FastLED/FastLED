#pragma once

#include "crgb.h"
#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// Used to test against. This is the float version and always works.
float scale_and_saturate_float(float a, float b);


// This is the integer version that we want to test.
void scale_and_saturate_u8(uint8_t a, uint8_t b, uint8_t* a_prime, uint8_t* b_prime);


FASTLED_NAMESPACE_END
