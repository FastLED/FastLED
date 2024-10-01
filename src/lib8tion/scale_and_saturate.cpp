#include <stdint.h>
#include "namespace.h"
#include "scale_and_saturate.h"

using namespace std;


FASTLED_NAMESPACE_BEGIN

namespace detail {
template <typename T> T min(const T &a, const T &b) { return (a < b) ? a : b; }
template <typename T> T max(const T &a, const T &b) { return (a > b) ? a : b; }
} // namespace

using detail::min;
using detail::max;

float scale_and_saturate_float(float a, float b) {
    float bprime = (a * b) / 255.0;
    if (bprime >= 1.0) {
        return bprime;
    }

    // in the case bprime is too small so we need to scale it by the product of a and b
    float product = a * b;
    return (product / bprime);
}

void scale_and_saturate_u8(uint8_t a, uint8_t b, uint8_t* a_prime, uint8_t* b_prime) {
    // Maximize a_prime to 255
    *a_prime = 255;

    // Calculate the product of a and b
    uint16_t product = static_cast<uint16_t>(a) * static_cast<uint16_t>(b);

    // Calculate b_prime to preserve the original product as closely as possible
    *b_prime = (product + *a_prime / 2) / *a_prime;  // Adding half to round properly
}

FASTLED_NAMESPACE_END