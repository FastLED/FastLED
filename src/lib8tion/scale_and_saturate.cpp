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

#include <stdint.h>

void scale_and_saturate_u8(uint8_t a, uint8_t b, uint8_t* a_prime, uint8_t* b_prime) {
    // Saturate a to 255 (or as close as possible)
    *a_prime = 255;

    // Calculate b_prime such that a_prime * b_prime is close to a * b
    uint16_t product = static_cast<uint16_t>(a) * static_cast<uint16_t>(b);
    *b_prime = (product + *a_prime / 2) / *a_prime;  // Rounding for better precision
}


FASTLED_NAMESPACE_END