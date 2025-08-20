#pragma once

#include "fl/int.h"
#include "fl/math.h"
#include "fl/compiler_control.h"
#include "fl/move.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

namespace fl {

template <typename T> struct vec3 {
    // value_type
    using value_type = T;
    T x = 0;
    T y = 0;
    T z = 0;
    constexpr vec3() = default;
    constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) {}

    template <typename U>
    explicit constexpr vec3(U xyz) : x(xyz), y(xyz), z(xyz) {}

    constexpr vec3(const vec3 &p) = default;
    constexpr vec3(vec3 &&p) noexcept = default;
    vec3 &operator=(vec3 &&p) noexcept = default;
    
    vec3 &operator*=(const float &f) {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    vec3 &operator/=(const float &f) {
        x /= f;
        y /= f;
        z /= f;
        return *this;
    }
    vec3 &operator*=(const double &f) {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    vec3 &operator/=(const double &f) {
        x /= f;
        y /= f;
        z /= f;
        return *this;
    }

    vec3 &operator/=(const u16 &d) {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }

    vec3 &operator/=(const int &d) {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }

    vec3 &operator/=(const vec3 &p) {
        x /= p.x;
        y /= p.y;
        z /= p.z;
        return *this;
    }

    vec3 &operator+=(const vec3 &p) {
        x += p.x;
        y += p.y;
        z += p.z;
        return *this;
    }

    vec3 &operator-=(const vec3 &p) {
        x -= p.x;
        y -= p.y;
        z -= p.z;
        return *this;
    }

    vec3 &operator=(const vec3 &p) {
        x = p.x;
        y = p.y;
        z = p.z;
        return *this;
    }

    vec3 operator-(const vec3 &p) const {
        return vec3(x - p.x, y - p.y, z - p.z);
    }

    vec3 operator+(const vec3 &p) const {
        return vec3(x + p.x, y + p.y, z + p.z);
    }

    vec3 operator*(const vec3 &p) const {
        return vec3(x * p.x, y * p.y, z * p.z);
    }

    vec3 operator/(const vec3 &p) const {
        return vec3(x / p.x, y / p.y, z / p.z);
    }

    template <typename NumberT> vec3 operator+(const NumberT &p) const {
        return vec3(x + p, y + p, z + p);
    }

    template <typename U> vec3 operator+(const vec3<U> &p) const {
        return vec3(x + p.x, y + p.y, z + p.z);
    }

    template <typename NumberT> vec3 operator-(const NumberT &p) const {
        return vec3(x - p, y - p, z - p);
    }

    template <typename NumberT> vec3 operator*(const NumberT &p) const {
        return vec3(x * p, y * p, z * p);
    }

    template <typename NumberT> vec3 operator/(const NumberT &p) const {
        T a = x / p;
        T b = y / p;
        T c = z / p;
        return vec3<T>(a, b, c);
    }

    bool operator==(const vec3 &p) const {
        return (x == p.x && y == p.y && z == p.z);
    }

    bool operator!=(const vec3 &p) const {
        return (x != p.x || y != p.y || z != p.z);
    }

    template <typename U> bool operator==(const vec3<U> &p) const {
        return (x == p.x && y == p.y && z == p.z);
    }

    template <typename U> bool operator!=(const vec3<U> &p) const {
        return (x != p.x || y != p.y || z != p.z);
    }

    vec3 getMax(const vec3 &p) const {
        return vec3(MAX(x, p.x), MAX(y, p.y), MAX(z, p.z));
    }

    vec3 getMin(const vec3 &p) const {
        return vec3(MIN(x, p.x), MIN(y, p.y), MIN(z, p.z));
    }

    template <typename U> vec3<U> cast() const {
        return vec3<U>(static_cast<U>(x), static_cast<U>(y), static_cast<U>(z));
    }

    template <typename U> vec3 getMax(const vec3<U> &p) const {
        return vec3<U>(MAX(x, p.x), MAX(y, p.y), MAX(z, p.z));
    }

    template <typename U> vec3 getMin(const vec3<U> &p) const {
        return vec3<U>(MIN(x, p.x), MIN(y, p.y), MIN(z, p.z));
    }

    T distance(const vec3 &p) const {
        T dx = x - p.x;
        T dy = y - p.y;
        T dz = z - p.z;
        return sqrt(dx * dx + dy * dy + dz * dz);
    }

    bool is_zero() const { return (x == 0 && y == 0 && z == 0); }
};

using vec3f = vec3<float>; // Full precision but slow.

template <typename T> struct vec2 {
    // value_type
    using value_type = T;
    value_type x = 0;
    value_type y = 0;
    constexpr vec2() = default;
    constexpr vec2(T x, T y) : x(x), y(y) {}

    template <typename U> explicit constexpr vec2(U xy) : x(xy), y(xy) {}

    constexpr vec2(const vec2 &p) = default;
    constexpr vec2(vec2 &&p) noexcept = default;
    vec2 &operator=(vec2 &&p) noexcept = default;
    
    vec2 &operator*=(const float &f) {
        x *= f;
        y *= f;
        return *this;
    }
    vec2 &operator/=(const float &f) {
        // *this = point_xy_math::div(*this, f);
        x /= f;
        y /= f;
        return *this;
    }
    vec2 &operator*=(const double &f) {
        // *this = point_xy_math::mul(*this, f);
        x *= f;
        y *= f;
        return *this;
    }
    vec2 &operator/=(const double &f) {
        // *this = point_xy_math::div(*this, f);
        x /= f;
        y /= f;
        return *this;
    }

    vec2 &operator/=(const u16 &d) {
        // *this = point_xy_math::div(*this, d);
        x /= d;
        y /= d;
        return *this;
    }

    vec2 &operator/=(const int &d) {
        // *this = point_xy_math::div(*this, d);
        x /= d;
        y /= d;
        return *this;
    }

    vec2 &operator/=(const vec2 &p) {
        // *this = point_xy_math::div(*this, p);
        x /= p.x;
        y /= p.y;
        return *this;
    }

    vec2 &operator+=(const vec2 &p) {
        //*this = point_xy_math::add(*this, p);
        x += p.x;
        y += p.y;
        return *this;
    }

    vec2 &operator-=(const vec2 &p) {
        // *this = point_xy_math::sub(*this, p);
        x -= p.x;
        y -= p.y;
        return *this;
    }

    vec2 &operator=(const vec2 &p) {
        x = p.x;
        y = p.y;
        return *this;
    }

    vec2 operator-(const vec2 &p) const { return vec2(x - p.x, y - p.y); }

    vec2 operator+(const vec2 &p) const { return vec2(x + p.x, y + p.y); }

    vec2 operator*(const vec2 &p) const { return vec2(x * p.x, y * p.y); }

    vec2 operator/(const vec2 &p) const { return vec2(x / p.x, y / p.y); }

    template <typename NumberT> vec2 operator+(const NumberT &p) const {
        return vec2(x + p, y + p);
    }

    template <typename U> vec2 operator+(const vec2<U> &p) const {
        return vec2(x + p.x, y + p.x);
    }

    template <typename NumberT> vec2 operator-(const NumberT &p) const {
        return vec2(x - p, y - p);
    }

    template <typename NumberT> vec2 operator*(const NumberT &p) const {
        return vec2(x * p, y * p);
    }

    template <typename NumberT> vec2 operator/(const NumberT &p) const {
        T a = x / p;
        T b = y / p;
        return vec2<T>(a, b);
    }

    bool operator==(const vec2 &p) const { return (x == p.x && y == p.y); }

    bool operator!=(const vec2 &p) const { return (x != p.x || y != p.y); }

    template <typename U> bool operator==(const vec2<U> &p) const {
        return (x == p.x && y == p.y);
    }

    template <typename U> bool operator!=(const vec2<U> &p) const {
        return (x != p.x || y != p.y);
    }

    vec2 getMax(const vec2 &p) const { return vec2(MAX(x, p.x), MAX(y, p.y)); }

    vec2 getMin(const vec2 &p) const { return vec2(MIN(x, p.x), MIN(y, p.y)); }

    template <typename U> vec2<U> cast() const {
        return vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }

    template <typename U> vec2 getMax(const vec2<U> &p) const {
        return vec2<U>(MAX(x, p.x), MAX(y, p.y));
    }

    template <typename U> vec2 getMin(const vec2<U> &p) const {
        return vec2<U>(MIN(x, p.x), MIN(y, p.y));
    }

    T distance(const vec2 &p) const {
        T dx = x - p.x;
        T dy = y - p.y;
        return sqrt(dx * dx + dy * dy);
    }

    bool is_zero() const { return (x == 0 && y == 0); }
};

using vec2f = vec2<float>; // Full precision but slow.
using vec2u8 = vec2<fl::u8>; // 8-bit unsigned integer vector.
using vec2i16 = vec2<i16>; // 16-bit signed integer vector.

// Legacy support for vec3
using pair_xyz_float = vec3<float>; // Legacy name for vec3f

// Legacy support for vec2

using pair_xy_float = vec2<float>; // Legacy name for vec2f

// pair_xy<T> is the legacy name for vec2<T>
template <typename T> struct pair_xy : public vec2<T> {
    using value_type = T;
    using vec2<T>::vec2;
    pair_xy() = default;
    pair_xy(const vec2<T> &p) : vec2<T>(p) {}
};

template <typename T> struct line_xy {
    vec2<T> start;
    vec2<T> end;

    line_xy() = default;
    line_xy(const vec2<T> &start, const vec2<T> &end)
        : start(start), end(end) {}

    line_xy(T start_x, T start_y, T end_x, T end_y)
        : start(start_x, start_y), end(end_x, end_y) {}
    
    line_xy(const line_xy &other) = default;
    line_xy &operator=(const line_xy &other) = default;
    line_xy(line_xy &&other) noexcept = default;
    line_xy &operator=(line_xy &&other) noexcept = default;

    bool empty() const { return (start == end); }

    float distance_to(const vec2<T> &p,
                      vec2<T> *out_projected = nullptr) const {
        return distance_to_line_with_point(p, start, end, out_projected);
    }

  private:
    // Computes the closest distance from `p` to the line through `a` and `b`,
    // and writes the projected point.
    static float distance_to_line_with_point(vec2<T> p, vec2<T> a, vec2<T> b,
                                             vec2<T> *out_projected) {
        vec2<T> maybe;
        vec2<T> &out_proj = out_projected ? *out_projected : maybe;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len_sq = dx * dx + dy * dy;

        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(float-equal)
        const bool is_zero = (len_sq == 0.0f);
        FL_DISABLE_WARNING_POP

        if (is_zero) {
            // a == b, the segment is a point
            out_proj = a;
            dx = p.x - a.x;
            dy = p.y - a.y;
            return sqrt(dx * dx + dy * dy);
        }

        // Project point p onto the line segment, computing parameter t
        float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len_sq;

        // Clamp t to [0,1] to stay within the segment
        if (t < 0.0f)
            t = 0.0f;
        else if (t > 1.0f)
            t = 1.0f;

        // Find the closest point
        out_proj.x = a.x + t * dx;
        out_proj.y = a.y + t * dy;

        dx = p.x - out_proj.x;
        dy = p.y - out_proj.y;
        return sqrt(dx * dx + dy * dy);
    }
};

template <typename T> struct rect {
    vec2<T> mMin;
    vec2<T> mMax;

    rect() = default;
    rect(const vec2<T> &min, const vec2<T> &max) : mMin(min), mMax(max) {}

    rect(T min_x, T min_y, T max_x, T max_y)
        : mMin(min_x, min_y), mMax(max_x, max_y) {}
    
    rect(const rect &other) = default;
    rect &operator=(const rect &other) = default;
    rect(rect &&other) noexcept = default;
    rect &operator=(rect &&other) noexcept = default;

    u16 width() const { return mMax.x - mMin.x; }

    u16 height() const { return mMax.y - mMin.y; }

    bool empty() const { return (mMin.x == mMax.x && mMin.y == mMax.y); }

    void expand(const vec2<T> &p) { expand(p.x, p.y); }

    void expand(const rect &r) {
        expand(r.mMin);
        expand(r.mMax);
    }

    void expand(T x, T y) {
        mMin.x = MIN(mMin.x, x);
        mMin.y = MIN(mMin.y, y);
        mMax.x = MAX(mMax.x, x);
        mMax.y = MAX(mMax.y, y);
    }

    bool contains(const vec2<T> &p) const {
        return (p.x >= mMin.x && p.x < mMax.x && p.y >= mMin.y && p.y < mMax.y);
    }

    bool contains(const T &x, const T &y) const {
        return contains(vec2<T>(x, y));
    }

    bool operator==(const rect &r) const {
        return (mMin == r.mMin && mMax == r.mMax);
    }

    bool operator!=(const rect &r) const { return !(*this == r); }

    template <typename U> bool operator==(const rect<U> &r) const {
        return (mMin == r.mMin && mMax == r.mMax);
    }

    template <typename U> bool operator!=(const rect<U> &r) const {
        return !(*this == r);
    }
};

} // namespace fl

FL_DISABLE_WARNING_POP
