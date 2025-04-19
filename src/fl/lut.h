#pragma once

/*
LUT - Look up table implementation for various types.
*/

#include <stdint.h>
#include "fl/ptr.h"
#include "fl/force_inline.h"
#include "fl/allocator.h"

#include "fl/namespace.h"

namespace fl {

template<typename PairXyType>
struct pair_xy_traits {
    using value_type = typename PairXyType::value_type;
    static constexpr PairXyType zero() {
        return PairXyType();
    }

    static constexpr PairXyType add(const PairXyType& a, const PairXyType& b) {
        return PairXyType(a.x + b.x, a.y + b.y);
    }

    static constexpr PairXyType sub(const PairXyType& a, const PairXyType& b) {
        return PairXyType(a.x - b.x, a.y - b.y);
    }

    static constexpr PairXyType mul(const PairXyType& a, const PairXyType& b) {
        return PairXyType(a.x * b.x, a.y * b.y);
    }

    static constexpr PairXyType div(const PairXyType& a, const PairXyType& b) {
        return PairXyType(a.x / b.x, a.y / b.y);
    }

    template<typename NumberT>
    static constexpr PairXyType add(const PairXyType& a, const NumberT& b) {
        return PairXyType(a.x + b, a.y + b);
    }

    template<typename NumberT>
    static constexpr PairXyType sub(const PairXyType& a, const NumberT& b) {
        return PairXyType(a.x - b, a.y - b);
    }

    template<typename NumberT>
    static constexpr PairXyType mul(const PairXyType& a, const NumberT& b) {
        return PairXyType(a.x * b, a.y * b);
    }

    template<typename NumberT>
    static constexpr PairXyType div(const PairXyType& a, const NumberT& b) {
        return PairXyType(a.x / b, a.y / b);
    }
};

template<typename T>
struct pair_xy {
    // value_type
    using value_type = T;
    T x = 0;
    T y = 0;
    constexpr pair_xy() = default;
    constexpr pair_xy(T x, T y) : x(x), y(y) {}
    constexpr pair_xy(const pair_xy& p) : x(p.x), y(p.y) {}
    pair_xy& operator*=(const float& f) {
        x *= f;
        y *= f;
        return *this;
    }
    pair_xy& operator/=(const float& f) {
        *this = pair_xy_traits<pair_xy>::div(*this, f);
        return *this;
    }
    pair_xy& operator*=(const double& f) {
        *this = pair_xy_traits<pair_xy>::mul(*this, f);
        return *this;
    }
    pair_xy& operator/=(const double& f) {
        *this = pair_xy_traits<pair_xy>::div(*this, f);
        return *this;
    }

    pair_xy& operator/=(const uint16_t& d) {
        *this = pair_xy_traits<pair_xy>::div(*this, d);
        return *this;
    }

    pair_xy& operator/=(const int& d) {
        *this = pair_xy_traits<pair_xy>::div(*this, d);
        return *this;
    }

    pair_xy& operator/=(const pair_xy& p) {
        *this = pair_xy_traits<pair_xy>::div(*this, p);
        return *this;
    }

    pair_xy& operator+=(const pair_xy& p) {
        *this = pair_xy_traits<pair_xy>::add(*this, p);
        return *this;
    }

    pair_xy& operator-=(const pair_xy& p) {
        *this = pair_xy_traits<pair_xy>::sub(*this, p);
        return *this;
    }

    pair_xy& operator=(const pair_xy& p) {
        x = p.x;
        y = p.y;
        return *this;
    }

    pair_xy operator-(const pair_xy& p) const {
        return pair_xy_traits<pair_xy>::sub(*this, p);
    }

    pair_xy operator+(const pair_xy& p) const {
        return pair_xy_traits<pair_xy>::add(*this, p);
    }

    pair_xy operator*(const pair_xy& p) const {
        return pair_xy_traits<pair_xy>::mul(*this, p);
    }

    pair_xy operator/(const pair_xy& p) const {
        return pair_xy_traits<pair_xy>::div(*this, p);
    }

    template<typename NumberT>
    pair_xy operator+(const NumberT& p) const {
        return pair_xy_traits<pair_xy>::add(*this, p);
    }

    template<typename NumberT>
    pair_xy operator-(const NumberT& p) const {
        return pair_xy_traits<pair_xy>::sub(*this, p);
    }

    template<typename NumberT>
    pair_xy operator*(const NumberT& p) const {
        return pair_xy_traits<pair_xy>::mul(*this, p);
    }

    template<typename NumberT>
    pair_xy operator/(const NumberT& p) const {
        return pair_xy_traits<pair_xy>::div(*this, p);
    }

    bool operator==(const pair_xy& p) const {
        return (x == p.x && y == p.y);
    }

    bool operator!=(const pair_xy& p) const {
        return (x != p.x || y != p.y);
    }
};

using pair_xy_float = pair_xy<float>;  // It's just easier if we allow negative values.

// LUT holds a look up table to map data from one
// value to another. This can be quite big (1/3rd of the frame buffer)
// so a Referent is used to allow memory sharing.

template<typename T>
class LUT;

typedef LUT<uint16_t> LUT16;
typedef LUT<pair_xy<uint16_t>> LUTXY16;
typedef LUT<pair_xy_float> LUTXYFLOAT;

FASTLED_SMART_PTR_NO_FWD(LUT16);
FASTLED_SMART_PTR_NO_FWD(LUTXYFLOAT);
FASTLED_SMART_PTR_NO_FWD(LUTXY16);

// Templated lookup table.
template<typename T>
class LUT : public fl::Referent {
public:
    LUT(uint32_t length) : length(length) {
        T* ptr = LargeBlockAllocator<T>::Alloc(length);
        mDataHandle.reset(ptr);
        data = ptr;
    }
    // In this version the data is passed in but not managed by this object.
    LUT(uint32_t length, T* data) : length(length) {
        this->data = data;
    }
    ~LUT() {
        LargeBlockAllocator<T>::Free(mDataHandle.release());
        data = mDataHandle.get();
    }

    const T& operator[](uint32_t index) const {
        return data[index];
    }

    const T& operator[](uint16_t index) const {
        return data[index];
    }

    T* getData() const {
        return data;
    }

    uint32_t size() const {
        return length;
    }

    T interp8(uint8_t alpha) {
        if (length == 0) return T();
        if (alpha ==   0) return data[0];
        if (alpha == 255) return data[length-1];

        // treat alpha/255 as fraction, scale to [0..length-1]
        uint32_t maxIndex = length - 1;
        uint32_t pos      = uint32_t(alpha) * maxIndex;   // numerator
        uint32_t idx0     = pos / 255;                     // floor(position)
        uint32_t idx1     = idx0 < maxIndex ? idx0 + 1 : maxIndex;
        uint8_t  blend    = pos % 255;                     // fractional part

        const T& a = data[idx0];
        const T& b = data[idx1];
        // a + (b-a) * blend/255
        return a + (b - a) * blend / 255;
    }

    T interp16(uint16_t alpha) {
        if (length == 0) return T();
        if (alpha ==   0) return data[0];
        if (alpha == 65535) return data[length-1];

        // treat alpha/65535 as fraction, scale to [0..length-1]
        uint32_t maxIndex = length - 1;
        uint32_t pos      = uint32_t(alpha) * maxIndex;   // numerator
        uint32_t idx0     = pos / 65535;                   // floor(position)
        uint32_t idx1     = idx0 < maxIndex ? idx0 + 1 : maxIndex;
        uint16_t blend    = pos % 65535;                   // fractional part

        const T& a = data[idx0];
        const T& b = data[idx1];
        // a + (b-a) * blend/65535
        return a + (b - a) * blend / 65535;
    }

private:
    fl::scoped_ptr<T> mDataHandle;
    T* data = nullptr;
    uint32_t length;
};

}  // namespace fl

