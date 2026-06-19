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
    random() FL_NO_EXCEPT : mSeed(random16_get_seed()) {}

    /// Constructor with explicit seed
    explicit random(u16 seed) FL_NO_EXCEPT : mSeed(seed) {}

    result_type operator()() FL_NO_EXCEPT { return generate_nolock(); }
    result_type operator()(result_type n) FL_NO_EXCEPT { return generate_nolock(n); }
    result_type operator()(result_type min, result_type max) FL_NO_EXCEPT { return generate_nolock(min, max); }

    void set_seed(u16 seed) FL_NO_EXCEPT { mSeed = seed; }
    u16 get_seed() const FL_NO_EXCEPT { return mSeed; }
    void add_entropy(u16 entropy) FL_NO_EXCEPT { mSeed += entropy; }

    static constexpr result_type minimum() FL_NO_EXCEPT { return 0; }
    static constexpr result_type maximum() FL_NO_EXCEPT { return 4294967295U; }

    u8 random8() FL_NO_EXCEPT { return random8_nolock(); }
    u8 random8(u8 n) FL_NO_EXCEPT { return random8_nolock(n); }
    u8 random8(u8 min, u8 max) FL_NO_EXCEPT { return random8_nolock(min, max); }

    u16 random16() FL_NO_EXCEPT { return random16_nolock(); }
    u16 random16(u16 n) FL_NO_EXCEPT { return static_cast<u16>(generate_nolock(n)); }
    u16 random16(u16 min, u16 max) FL_NO_EXCEPT { return static_cast<u16>(generate_nolock(min, max)); }

private:
    u16 mSeed;

    u16 next_random16_nolock() FL_NO_EXCEPT {
        mSeed = APPLY_FASTLED_RAND16_2053(mSeed) + FASTLED_RAND16_13849;
        return mSeed;
    }

    u32 next_random32_nolock() FL_NO_EXCEPT {
        u32 high = next_random16_nolock();
        u32 low = next_random16_nolock();
        return (high << 16) | low;
    }

    result_type generate_nolock() FL_NO_EXCEPT {
        return next_random32_nolock();
    }

    result_type generate_nolock(result_type n) FL_NO_EXCEPT {
        if (n == 0) return 0;
        u32 r = next_random32_nolock();
        fl::u64 p = (fl::u64)n * (fl::u64)r;
        return (u32)(p >> 32);
    }

    result_type generate_nolock(result_type min, result_type max) FL_NO_EXCEPT {
        result_type delta = max - min;
        return generate_nolock(delta) + min;
    }

    u8 random8_nolock() FL_NO_EXCEPT {
        u16 r = next_random16_nolock();
        return (u8)(((u8)(r & 0xFF)) + ((u8)(r >> 8)));
    }

    u8 random8_nolock(u8 n) FL_NO_EXCEPT {
        u8 r = random8_nolock();
        r = (r * n) >> 8;
        return r;
    }

    u8 random8_nolock(u8 min, u8 max) FL_NO_EXCEPT {
        u8 delta = max - min;
        return random8_nolock(delta) + min;
    }

    u16 random16_nolock() FL_NO_EXCEPT {
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
math::random& default_random() FL_NO_EXCEPT;

} // namespace fl
