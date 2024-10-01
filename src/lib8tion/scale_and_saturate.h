#pragma once

#include "crgb.h"
#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


void scale_and_saturate_u8(uint8_t a, uint8_t b, uint8_t* a_prime, uint8_t* b_prime);


FASTLED_NAMESPACE_END
