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

uint8_t scale_and_saturate_u8(uint8_t a, uint8_t b) {
    uint16_t result = (static_cast<uint16_t>(a) * static_cast<uint16_t>(b) + 127) / 255;
    return static_cast<uint8_t>(result);
}


FASTLED_NAMESPACE_END