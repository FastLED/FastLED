#pragma once

#include "fl/str.h"
#include "fl/type_traits.h"
#include "fl/int.h"
#include "fl/stdint.h"
#include "fl/force_inline.h"
#include "fl/memfill.h"
#include <string.h>
#include "fl/compiler_control.h"

namespace fl {

template <typename T> struct vec2;



//-----------------------------------------------------------------------------
// MurmurHash3 x86 32-bit
//-----------------------------------------------------------------------------
// Based on the public‐domain implementation by Austin Appleby:
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
static inline u32 MurmurHash3_x86_32(const void *key, fl::size len,
                                          u32 seed = 0) {

    FL_DISABLE_WARNING_PUSH;
    FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH;

    const fl::u8 *data = static_cast<const fl::u8 *>(key);
    const int nblocks = int(len / 4);

    u32 h1 = seed;
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    // body
    const u32 *blocks = reinterpret_cast<const u32 *>(data);
    for (int i = 0; i < nblocks; ++i) {
        u32 k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // tail
    const fl::u8 *tail = data + (nblocks * 4);
    u32 k1 = 0;
    switch (len & 3) {
    case 3:
        k1 ^= u32(tail[2]) << 16;
    case 2:
        k1 ^= u32(tail[1]) << 8;
    case 1:
        k1 ^= u32(tail[0]);
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
    }

    // finalization
    h1 ^= u32(len);
    // fmix32
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;

    FL_DISABLE_WARNING_POP;
}

//-----------------------------------------------------------------------------
// Fast, cheap 32-bit integer hash (Thomas Wang)
//-----------------------------------------------------------------------------
static inline u32 fast_hash32(u32 x) noexcept {
    x = (x ^ 61u) ^ (x >> 16);
    x = x + (x << 3);
    x = x ^ (x >> 4);
    x = x * 0x27d4eb2dU;
    x = x ^ (x >> 15);
    return x;
}

// 3) Handy two-word hasher
static inline u32 hash_pair(u32 a, u32 b,
                                 u32 seed = 0) noexcept {
    // mix in 'a', then mix in 'b'
    u32 h = fast_hash32(seed ^ a);
    return fast_hash32(h ^ b);
}

static inline u32 fast_hash64(u64 x) noexcept {
    u32 x1 = static_cast<u32>(x & 0x00000000FFFFFFFF);
    u32 x2 = static_cast<u32>(x >> 32);
    return hash_pair(x1, x2);
}

//-----------------------------------------------------------------------------
// Functor for hashing arbitrary byte‐ranges to a 32‐bit value
//-----------------------------------------------------------------------------
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
template <typename T> struct Hash {
    static_assert(fl::is_pod<T>::value,
                  "fl::Hash<T> only supports POD types (integrals, floats, "
                  "etc.), you need to define your own hash.");
    u32 operator()(const T &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(T));
    }
};

template <typename T> struct FastHash {
    static_assert(fl::is_pod<T>::value,
                  "fl::FastHash<T> only supports POD types (integrals, floats, "
                  "etc.), you need to define your own hash.");
    u32 operator()(const T &key) const noexcept {
        return fast_hash32(key);
    }
};

template <typename T> struct FastHash<vec2<T>> {
    u32 operator()(const vec2<T> &key) const noexcept {
        if (sizeof(T) == sizeof(fl::u8)) {
            u32 x = static_cast<u32>(key.x) +
                         (static_cast<u32>(key.y) << 8);
            return fast_hash32(x);
        }
        if (sizeof(T) == sizeof(u16)) {
            u32 x = static_cast<u32>(key.x) +
                         (static_cast<u32>(key.y) << 16);
            return fast_hash32(x);
        }
        if (sizeof(T) == sizeof(u32)) {
            return hash_pair(key.x, key.y);
        }
        return MurmurHash3_x86_32(&key, sizeof(T));
    }
};

template <typename T> struct Hash<T *> {
    u32 operator()(T *key) const noexcept {
        if (sizeof(T *) == sizeof(u32)) {
            u32 key_u = reinterpret_cast<fl::uptr>(key);
            return fast_hash32(key_u);
        } else {
            return MurmurHash3_x86_32(key, sizeof(T *));
        }
    }
};

template <typename T> struct Hash<vec2<T>> {
    u32 operator()(const vec2<T> &key) const noexcept {
#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        T packed[2] = {key.x, key.y};
        const void *p = &packed[0];
        return MurmurHash3_x86_32(p, sizeof(packed));
#ifndef __clang__
#pragma GCC diagnostic pop
#endif
    }
};

template <typename T> struct Hash<fl::shared_ptr<T>> {
    u32 operator()(const T &key) const noexcept {
        auto hasher = Hash<T *>();
        return hasher(key.get());
    }
};

template <typename T> struct Hash<fl::WeakPtr<T>> {
    u32 operator()(const fl::WeakPtr<T> &key) const noexcept {
        fl::uptr val = key.ptr_value();
        return MurmurHash3_x86_32(&val, sizeof(fl::uptr));
    }
};

template<> struct Hash<float> {
    u32 operator()(const float key) const noexcept {
        u32 ikey = fl::bit_cast<u32>(key);
        return fast_hash32(ikey);
    }
};

template<> struct Hash<double> {
    u32 operator()(const double& key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(double));
    }
};

template<> struct Hash<i32> {
    u32 operator()(const i32 key) const noexcept {
        u32 ukey = static_cast<u32>(key);
        return fast_hash32(ukey);
    }
};

template<> struct Hash<bool> {
    u32 operator()(const bool key) const noexcept {
        return fast_hash32(key);
    }
};

template<> struct Hash<fl::u8> {
    u32 operator()(const fl::u8 &key) const noexcept {
        return fast_hash32(key);
    }
};

template<> struct Hash<u16> {
    u32 operator()(const u16 &key) const noexcept {
        return fast_hash32(key);
    }
};

template<> struct Hash<u32> {
    u32 operator()(const u32 &key) const noexcept {
        return fast_hash32(key);
    }
};

template<> struct Hash<i8> {
    u32 operator()(const i8 &key) const noexcept {
        u8 v = static_cast<u8>(key);
        return fast_hash32(v);
    }
};

template<> struct Hash<i16> {
    u32 operator()(const i16 &key) const noexcept {
        u16 ukey = static_cast<u16>(key);
        return fast_hash32(ukey);
    }
};

// FASTLED_DEFINE_FAST_HASH(fl::u8)
// FASTLED_DEFINE_FAST_HASH(u16)
// FASTLED_DEFINE_FAST_HASH(u32)
// FASTLED_DEFINE_FAST_HASH(i8)
// FASTLED_DEFINE_FAST_HASH(i16)
// FASTLED_DEFINE_FAST_HASH(bool)

// FASTLED_DEFINE_FAST_HASH(int)

//-----------------------------------------------------------------------------
// Convenience for std::string → u32
//----------------------------------------------------------------------------
template <> struct Hash<fl::string> {
    u32 operator()(const fl::string &key) const noexcept {
        return MurmurHash3_x86_32(key.data(), key.size());
    }
};



} // namespace fl
