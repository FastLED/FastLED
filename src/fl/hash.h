
#pragma once

#include "fl/str.h"
#include "fl/template_magic.h"
#include <stdint.h>
#include <string.h>

namespace fl {

template <typename T> struct vec2;

//-----------------------------------------------------------------------------
// MurmurHash3 x86 32-bit
//-----------------------------------------------------------------------------
// Based on the public‐domain implementation by Austin Appleby:
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
static inline uint32_t MurmurHash3_x86_32(const void *key, size_t len,
                                          uint32_t seed = 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

    const uint8_t *data = static_cast<const uint8_t *>(key);
    const int nblocks = int(len / 4);

    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // body
    const uint32_t *blocks = reinterpret_cast<const uint32_t *>(data);
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // tail
    const uint8_t *tail = data + (nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
    case 3:
        k1 ^= uint32_t(tail[2]) << 16;
    case 2:
        k1 ^= uint32_t(tail[1]) << 8;
    case 1:
        k1 ^= uint32_t(tail[0]);
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
    }

    // finalization
    h1 ^= uint32_t(len);
    // fmix32
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;

#pragma GCC diagnostic pop
}

//-----------------------------------------------------------------------------
// Fast, cheap 32-bit integer hash (Thomas Wang)
//-----------------------------------------------------------------------------
static inline uint32_t fast_hash32(uint32_t x) noexcept {
    x = (x ^ 61u) ^ (x >> 16);
    x = x + (x << 3);
    x = x ^ (x >> 4);
    x = x * 0x27d4eb2dU;
    x = x ^ (x >> 15);
    return x;
}

// 3) Handy two-word hasher
static inline uint32_t hash_pair(uint32_t a, uint32_t b,
                                 uint32_t seed = 0) noexcept {
    // mix in 'a', then mix in 'b'
    uint32_t h = fast_hash32(seed ^ a);
    return fast_hash32(h ^ b);
}

//-----------------------------------------------------------------------------
// Functor for hashing arbitrary byte‐ranges to a 32‐bit value
//-----------------------------------------------------------------------------
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
template <typename T> struct Hash {
    static_assert(fl::is_pod<T>::value,
                  "fl::Hash<T> only supports POD types (integrals, floats, "
                  "etc.), you need to define your own hash.");
    uint32_t operator()(const T &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(T));
    }
};

template <typename T> struct FastHash {
    static_assert(fl::is_pod<T>::value,
                  "fl::FastHash<T> only supports POD types (integrals, floats, "
                  "etc.), you need to define your own hash.");
    uint32_t operator()(const T &key) const noexcept {
        return fast_hash32(key);
    }
};

template <typename T> struct FastHash<vec2<T>> {
    uint32_t operator()(const vec2<T> &key) const noexcept {
        if (sizeof(T) == sizeof(uint8_t)) {
            uint32_t x = static_cast<uint32_t>(key.x) +
                         (static_cast<uint32_t>(key.y) << 8);
            return fast_hash32(x);
        }
        if (sizeof(T) == sizeof(uint16_t)) {
            uint32_t x = static_cast<uint32_t>(key.x) +
                         (static_cast<uint32_t>(key.y) << 16);
            return fast_hash32(x);
        }
        if (sizeof(T) == sizeof(uint32_t)) {
            return hash_pair(key.x, key.y);
        }
        return MurmurHash3_x86_32(&key, sizeof(T));
    }
};

template <typename T> struct Hash<T *> {
    uint32_t operator()(const T *key) const noexcept {
        return fast_hash32(key, sizeof(T *));
    }
};

template <typename T> struct Hash<vec2<T>> {
    uint32_t operator()(const vec2<T> &key) const noexcept {
#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        T packed[2];
        memset(packed, 0, sizeof(packed));
        packed[0] = key.x;
        packed[1] = key.y; // Protect against alignment issues
        const void *p = &packed[0];
        return MurmurHash3_x86_32(p, sizeof(packed));
#ifndef __clang__
#pragma GCC diagnostic pop
#endif
    }
};

template <typename T> struct Hash<Ptr<T>> {
    uint32_t operator()(const T &key) const noexcept {
        auto hasher = Hash<T *>();
        return hasher(key.get());
    }
};

#define FASTLED_DEFINE_FAST_HASH(T)                                            \
    template <> struct Hash<T> {                                               \
        uint32_t operator()(const int &key) const noexcept {                   \
            return fast_hash32(key);                                           \
        }                                                                      \
    };

FASTLED_DEFINE_FAST_HASH(uint8_t)
FASTLED_DEFINE_FAST_HASH(uint16_t)
FASTLED_DEFINE_FAST_HASH(uint32_t)
FASTLED_DEFINE_FAST_HASH(int8_t)
FASTLED_DEFINE_FAST_HASH(int16_t)
FASTLED_DEFINE_FAST_HASH(int32_t)
FASTLED_DEFINE_FAST_HASH(float)
FASTLED_DEFINE_FAST_HASH(double)
FASTLED_DEFINE_FAST_HASH(bool)
// FASTLED_DEFINE_FAST_HASH(int)

//-----------------------------------------------------------------------------
// Convenience for std::string → uint32_t
//----------------------------------------------------------------------------
template <> struct Hash<fl::Str> {
    uint32_t operator()(const fl::Str &key) const noexcept {
        return MurmurHash3_x86_32(key.data(), key.size());
    }
};

} // namespace fl
