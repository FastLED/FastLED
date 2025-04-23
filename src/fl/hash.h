

#include <stdint.h>
#include <string.h>
#include "fl/str.h"
#include "fl/template_magic.h"

namespace fl {
//-----------------------------------------------------------------------------
// MurmurHash3 x86 32-bit
//-----------------------------------------------------------------------------
// Based on the public‐domain implementation by Austin Appleby:
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
static inline uint32_t MurmurHash3_x86_32(const void* key, size_t len, uint32_t seed = 0) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    const int nblocks = int(len / 4);

    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // body
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data);
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
    const uint8_t* tail = data + (nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= uint32_t(tail[2]) << 16;
        case 2: k1 ^= uint32_t(tail[1]) << 8;
        case 1: k1 ^= uint32_t(tail[0]);
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
}

//-----------------------------------------------------------------------------
// Functor for hashing arbitrary byte‐ranges to a 32‐bit value
//-----------------------------------------------------------------------------
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
template<typename T>
struct Hash {
    static_assert(fl::is_pod<T>::value,
                  "fl::Hash<T> only supports POD types (integrals, floats, etc.), you need to define your own hash.");
    uint32_t operator()(const T &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(T));
    }
};

template<typename T>
struct Hash<T*> {
    uint32_t operator()(const T *key) const noexcept {
        return MurmurHash3_x86_32(key, sizeof(T*));
    }
};

template<typename T>
struct Hash<Ptr<T>> {
    uint32_t operator()(const T& key) const noexcept {
        auto hasher = Hash<T*>();
        return hasher(key.get());
    }
};


//-----------------------------------------------------------------------------
// Convenience for std::string → uint32_t
//----------------------------------------------------------------------------
template<>
struct Hash<fl::Str> {
    uint32_t operator()(const fl::Str &key) const noexcept {
        return MurmurHash3_x86_32(key.data(), key.size());
    }
};



}  // namespace fl
