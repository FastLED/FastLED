#pragma once

#include "fl/stl/int.h"
#include "fl/math/random8.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace math {

/// @brief A random number generator class that wraps FastLED's random functions
///
/// This class provides a standard C++ random generator interface that can be used
/// with algorithms like fl::shuffle. Each instance maintains its own seed state
/// independent of other instances and the global FastLED random state.
///
/// All public methods delegate to private _nolock helpers. This means
/// external locking (via the LockedRandom singleton in random.cpp.hpp)
/// only needs to wrap the public entry points.
///
/// @code
/// fl::math::random rng;
/// fl::vector<int> vec = {1, 2, 3, 4, 5};
/// fl::shuffle(vec.begin(), vec.end(), rng);
/// @endcode
class random {
public:
    typedef u32 result_type;

    /// Default constructor - uses current global random seed
    random() FL_NOEXCEPT : mSeed(random16_get_seed()) {}

    /// Constructor with explicit seed
    explicit random(u16 seed) : mSeed(seed) {}

    result_type operator()() { return generate_nolock(); }
    result_type operator()(result_type n) { return generate_nolock(n); }
    result_type operator()(result_type min, result_type max) { return generate_nolock(min, max); }

    void set_seed(u16 seed) { mSeed = seed; }
    u16 get_seed() const { return mSeed; }
    void add_entropy(u16 entropy) { mSeed += entropy; }

    static constexpr result_type minimum() { return 0; }
    static constexpr result_type maximum() { return 4294967295U; }

    u8 random8() { return random8_nolock(); }
    u8 random8(u8 n) { return random8_nolock(n); }
    u8 random8(u8 min, u8 max) { return random8_nolock(min, max); }

    u16 random16() { return random16_nolock(); }
    u16 random16(u16 n) { return static_cast<u16>(generate_nolock(n)); }
    u16 random16(u16 min, u16 max) { return static_cast<u16>(generate_nolock(min, max)); }

private:
    u16 mSeed;

    u16 next_random16_nolock() {
        mSeed = APPLY_FASTLED_RAND16_2053(mSeed) + FASTLED_RAND16_13849;
        return mSeed;
    }

    u32 next_random32_nolock() {
        u32 high = next_random16_nolock();
        u32 low = next_random16_nolock();
        return (high << 16) | low;
    }

    result_type generate_nolock() {
        return next_random32_nolock();
    }

    result_type generate_nolock(result_type n) {
        if (n == 0) return 0;
        u32 r = next_random32_nolock();
        fl::u64 p = (fl::u64)n * (fl::u64)r;
        return (u32)(p >> 32);
    }

    result_type generate_nolock(result_type min, result_type max) {
        result_type delta = max - min;
        return generate_nolock(delta) + min;
    }

    u8 random8_nolock() {
        u16 r = next_random16_nolock();
        return (u8)(((u8)(r & 0xFF)) + ((u8)(r >> 8)));
    }

    u8 random8_nolock(u8 n) {
        u8 r = random8_nolock();
        r = (r * n) >> 8;
        return r;
    }

    u8 random8_nolock(u8 min, u8 max) {
        u8 delta = max - min;
        return random8_nolock(delta) + min;
    }

    u16 random16_nolock() {
        return next_random16_nolock();
    }
};

} // namespace math

// Backward compatibility alias
using fl_random = math::random;

/// @brief Global default random number generator instance
///
/// All threads share a single Singleton<LockedRandom> so no two threads
/// can produce duplicate sequences from identical seeds.
math::random& default_random();

} // namespace fl
