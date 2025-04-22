
#pragma once

#include "fl/math_macros.h"

namespace fl {

template<typename T> struct point_xy_math;

template<typename T>
struct point_xy {
    // value_type
    using value_type = T;
    T x = 0;
    T y = 0;
    constexpr point_xy() = default;
    constexpr point_xy(T x, T y) : x(x), y(y) {}
    constexpr point_xy(const point_xy& p) : x(p.x), y(p.y) {}
    point_xy& operator*=(const float& f) {
        x *= f;
        y *= f;
        return *this;
    }
    point_xy& operator/=(const float& f) {
        *this = point_xy_math<point_xy>::div(*this, f);
        return *this;
    }
    point_xy& operator*=(const double& f) {
        *this = point_xy_math<point_xy>::mul(*this, f);
        return *this;
    }
    point_xy& operator/=(const double& f) {
        *this = point_xy_math<point_xy>::div(*this, f);
        return *this;
    }

    point_xy& operator/=(const uint16_t& d) {
        *this = point_xy_math<point_xy>::div(*this, d);
        return *this;
    }

    point_xy& operator/=(const int& d) {
        *this = point_xy_math<point_xy>::div(*this, d);
        return *this;
    }

    point_xy& operator/=(const point_xy& p) {
        *this = point_xy_math<point_xy>::div(*this, p);
        return *this;
    }

    point_xy& operator+=(const point_xy& p) {
        *this = point_xy_math<point_xy>::add(*this, p);
        return *this;
    }

    point_xy& operator-=(const point_xy& p) {
        *this = point_xy_math<point_xy>::sub(*this, p);
        return *this;
    }

    point_xy& operator=(const point_xy& p) {
        x = p.x;
        y = p.y;
        return *this;
    }

    point_xy operator-(const point_xy& p) const {
        return point_xy_math<point_xy>::sub(*this, p);
    }

    point_xy operator+(const point_xy& p) const {
        return point_xy_math<point_xy>::add(*this, p);
    }

    point_xy operator*(const point_xy& p) const {
        return point_xy_math<point_xy>::mul(*this, p);
    }

    point_xy operator/(const point_xy& p) const {
        return point_xy_math<point_xy>::div(*this, p);
    }

    template<typename NumberT>
    point_xy operator+(const NumberT& p) const {
        return point_xy_math<point_xy>::add(*this, p);
    }

    template<typename NumberT>
    point_xy operator-(const NumberT& p) const {
        return point_xy_math<point_xy>::sub(*this, p);
    }

    template<typename NumberT>
    point_xy operator*(const NumberT& p) const {
        return point_xy_math<point_xy>::mul(*this, p);
    }

    template<typename NumberT>
    point_xy operator/(const NumberT& p) const {
        return point_xy_math<point_xy>::div(*this, p);
    }

    bool operator==(const point_xy& p) const {
        return (x == p.x && y == p.y);
    }

    bool operator!=(const point_xy& p) const {
        return (x != p.x || y != p.y);
    }
};

using point_xy_float = point_xy<float>;  // Full precision but slow.

// Legacy support

using pair_xy_float = point_xy<float>;  // Legacy name for point_xy_float

// pair_xy<T> is the legacy name for point_xy<T>
template<typename T>
struct pair_xy : public point_xy<T> {
    using value_type = T;
    using point_xy<T>::point_xy;
    pair_xy() = default;
    pair_xy(const point_xy<T>& p) : point_xy<T>(p) {}
};

////////////// point_xy operations //////////////

template<typename T>
struct point_xy_math {
    using value_type = typename T::value_type;
    static constexpr T zero() {
        return T();
    }

    static constexpr T add(const T& a, const T& b) {
        return T(a.x + b.x, a.y + b.y);
    }

    static constexpr T sub(const T& a, const T& b) {
        return T(a.x - b.x, a.y - b.y);
    }

    static constexpr T mul(const T& a, const T& b) {
        return T(a.x * b.x, a.y * b.y);
    }

    static constexpr T div(const T& a, const T& b) {
        return T(a.x / b.x, a.y / b.y);
    }

    template<typename NumberT>
    static constexpr T add(const T& a, const NumberT& b) {
        return T(a.x + b, a.y + b);
    }

    template<typename NumberT>
    static constexpr T sub(const T& a, const NumberT& b) {
        return T(a.x - b, a.y - b);
    }

    template<typename NumberT>
    static constexpr T mul(const T& a, const NumberT& b) {
        return T(a.x * b, a.y * b);
    }

    template<typename NumberT>
    static constexpr T div(const T& a, const NumberT& b) {
        return T(a.x / b, a.y / b);
    }
};

template<typename T>
struct rect_xy {
    point_xy<T> mMin;
    point_xy<T> mMax;

    rect_xy() = default;
    rect_xy(const point_xy<T>& min, const point_xy<T>& max)
        : mMin(min), mMax(max) {}

    rect_xy(T min_x, T min_y, T max_x, T max_y)
        : mMin(min_x, min_y), mMax(max_x, max_y) {}

    uint16_t width() const {
        return mMax.x - mMin.x;
    }

    uint16_t height() const {
        return mMax.y - mMin.y;
    }

    void expand(const point_xy<T>& p) {
        expand(p.x, p.y);
    }

    void expand(const rect_xy& r) {
        expand(r.mMin);
        expand(r.mMax);
    }

    void expand(T x, T y) {
        mMin.x = MIN(mMin.x, x);
        mMin.y = MIN(mMin.y, y);
        mMax.x = MAX(mMax.x, x);
        mMax.y = MAX(mMax.y, y);
    }

    bool contains(const point_xy<T>& p) const {
        return (p.x >= mMin.x && p.x <= mMax.x && p.y >= mMin.y &&
                p.y <= mMax.y);
    }

    bool contains(const T& x, const T& y) const {
        return contains(point_xy<T>(x, y));
    }

    bool operator==(const rect_xy& r) const {
        return (mMin == r.mMin && mMax == r.mMax);
    }

    bool operator!=(const rect_xy& r) const {
        return !(*this == r);
    }
};

}  // namespace fl
