#include <stdint.h>

namespace {
template <typename T> T min(const T &a, const T &b) { return (a < b) ? a : b; }
template <typename T> T max(const T &a, const T &b) { return (a > b) ? a : b; }
} // namespace


bool scale_and_saturate(uint16_t *a, uint8_t *b) {
    // Calculate the final value of a without overflow
    uint32_t distance_to_max = 0xffff - *a;
    uint32_t scaled_increment = (distance_to_max * (*b)) / 255;

    // Update a with the scaled increment, clamping to 0xffff if necessary
    *a = static_cast<uint16_t>(min<uint32_t>(0xffffu, *a + scaled_increment));

    // Ensure b doesn't reach 0, stays at least 1
    *b = max<uint8_t>(1, *b);

    // Return true if a is fully saturated
    return (*a == 0xffff);
}

bool scale_and_saturate(uint8_t *a, uint8_t *b) {
    // Calculate the final value of a without overflow
    uint16_t distance_to_max = 0xff - *a;
    uint16_t scaled_increment = (distance_to_max * (*b)) / 255;

    // Update a with the scaled increment, clamping to 0xff if necessary
    *a = static_cast<uint8_t>(min<uint16_t>(0xff, *a + scaled_increment));

    // Ensure b doesn't reach 0, stays at least 1
    *b = max<uint8_t>(1, *b);

    // Return true if a is fully saturated
    return (*a == 0xff);
}

bool scale_and_saturate_with_5bit_b(uint8_t *a, uint8_t *b) {
    // Treat b as having 5-bit precision (values between 1 and 31)
    uint8_t b_5bit = min<uint8_t>(*b, 31);

    // Calculate the final value of a without overflow
    uint16_t distance_to_max = 0xff - *a;
    uint16_t scaled_increment =
        (distance_to_max * b_5bit) / 31; // Scale by 5-bit precision

    // Update a with the scaled increment, clamping to 0xff if necessary
    *a = static_cast<uint8_t>(min<uint16_t>(0xff, *a + scaled_increment));

    // Simulate decrementing b with its limited precision (but ensure it stays
    // at least 1)
    *b = max<uint8_t>(1, *b - 1);

    // Return true if a is fully saturated
    return (*a == 0xff);
}

bool scale_and_saturate_with_5bit_b(uint16_t *a, uint8_t *b) {
    // Treat b as having 5-bit precision (values between 1 and 31)
    uint8_t b_5bit = min<uint8_t>(*b, 31);

    // Calculate the final value of a without overflow
    uint32_t distance_to_max = 0xffff - *a;
    uint32_t scaled_increment =
        (distance_to_max * b_5bit) / 31; // Scale by 5-bit precision

    // Update a with the scaled increment, clamping to 0xffff if necessary
    *a = static_cast<uint16_t>(min<uint32_t>(0xffffu, *a + scaled_increment));

    // Simulate decrementing b with its limited precision (but ensure it stays
    // at least 1)
    *b = max<uint8_t>(1, *b - 1);

    // Return true if a is fully saturated
    return (*a == 0xffff);
}
