#pragma once

#include "fl/stdint.h"
#include "lib8tion/random8.h"

namespace fl {

/// @brief A random number generator class that wraps FastLED's random functions
/// 
/// This class provides a standard C++ random generator interface that can be used
/// with algorithms like fl::shuffle. Each instance maintains its own seed state
/// independent of other instances and the global FastLED random state.
///
/// @code
/// fl::fl_random rng;
/// fl::vector<int> vec = {1, 2, 3, 4, 5};
/// fl::shuffle(vec.begin(), vec.end(), rng);
/// @endcode
class fl_random {
private:
    /// The current seed state for this instance
    uint16_t seed_;

    /// Generate next 16-bit random number using this instance's seed
    /// @returns The next 16-bit random number
    uint16_t next_random16() {
        seed_ = APPLY_FASTLED_RAND16_2053(seed_) + FASTLED_RAND16_13849;
        return seed_;
    }

    /// Generate next 32-bit random number using this instance's seed
    /// @returns The next 32-bit random number
    uint32_t next_random32() {
        uint32_t high = next_random16();
        uint32_t low = next_random16();
        return (high << 16) | low;
    }

public:
    /// The result type for this random generator (32-bit unsigned integer)
    typedef uint32_t result_type;

    /// Default constructor - uses current global random seed
    fl_random() : seed_(random16_get_seed()) {}

    /// Constructor with explicit seed
    /// @param seed The seed value for the random number generator
    explicit fl_random(uint16_t seed) : seed_(seed) {}

    /// Generate a random number
    /// @returns A random 32-bit unsigned integer
    result_type operator()() {
        return next_random32();
    }

    /// Generate a random number in the range [0, n)
    /// @param n The upper bound (exclusive)
    /// @returns A random number from 0 to n-1
    result_type operator()(result_type n) {
        if (n == 0) return 0;
        uint32_t r = next_random32();
        uint64_t p = (uint64_t)n * (uint64_t)r;
        return (uint32_t)(p >> 32);
    }

    /// Generate a random number in the range [min, max)
    /// @param min The lower bound (inclusive)
    /// @param max The upper bound (exclusive)
    /// @returns A random number from min to max-1
    result_type operator()(result_type min, result_type max) {
        result_type delta = max - min;
        result_type r = (*this)(delta) + min;
        return r;
    }

    /// Set the seed for this random number generator instance
    /// @param seed The new seed value
    void set_seed(uint16_t seed) {
        seed_ = seed;
    }

    /// Get the current seed value for this instance
    /// @returns The current seed value
    uint16_t get_seed() const {
        return seed_;
    }

    /// Add entropy to this random number generator instance
    /// @param entropy The entropy value to add
    void add_entropy(uint16_t entropy) {
        seed_ += entropy;
    }

    /// Get the minimum value this generator can produce
    /// @returns The minimum value (always 0)
    static constexpr result_type minimum() {
        return 0;
    }

    /// Get the maximum value this generator can produce
    /// @returns The maximum value (4294967295)
    static constexpr result_type maximum() {
        return 4294967295U;
    }

    /// Generate an 8-bit random number
    /// @returns A random 8-bit unsigned integer (0-255)
    uint8_t random8() {
        uint16_t r = next_random16();
        // return the sum of the high and low bytes, for better mixing
        return (uint8_t)(((uint8_t)(r & 0xFF)) + ((uint8_t)(r >> 8)));
    }

    /// Generate an 8-bit random number in the range [0, n)
    /// @param n The upper bound (exclusive)
    /// @returns A random 8-bit number from 0 to n-1
    uint8_t random8(uint8_t n) {
        uint8_t r = random8();
        r = (r * n) >> 8;
        return r;
    }

    /// Generate an 8-bit random number in the range [min, max)
    /// @param min The lower bound (inclusive)
    /// @param max The upper bound (exclusive)
    /// @returns A random 8-bit number from min to max-1
    uint8_t random8(uint8_t min, uint8_t max) {
        uint8_t delta = max - min;
        uint8_t r = random8(delta) + min;
        return r;
    }

    /// Generate a 16-bit random number
    /// @returns A random 16-bit unsigned integer (0-65535)
    uint16_t random16() {
        return next_random16();
    }

    /// Generate a 16-bit random number in the range [0, n)
    /// @param n The upper bound (exclusive)
    /// @returns A random 16-bit number from 0 to n-1
    uint16_t random16(uint16_t n) {
        return (*this)(n);
    }

    /// Generate a 16-bit random number in the range [min, max)
    /// @param min The lower bound (inclusive)
    /// @param max The upper bound (exclusive)
    /// @returns A random 16-bit number from min to max-1
    uint16_t random16(uint16_t min, uint16_t max) {
        return (*this)(min, max);
    }
};

/// @brief Global default random number generator instance
/// 
/// This provides a convenient global instance that can be used when you
/// don't need to manage multiple separate random generators.
///
/// @code
/// // Using the global instance
/// auto value = fl::default_random()();
/// fl::shuffle(vec.begin(), vec.end(), fl::default_random());
/// @endcode
fl_random& default_random();

} // namespace fl 
