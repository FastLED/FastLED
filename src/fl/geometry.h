
#pragma once

#include "fl/math.h"

namespace fl {

template <typename T> struct point_xy {
    // value_type
    using value_type = T;
    T x = 0;
    T y = 0;
    constexpr point_xy() = default;
    constexpr point_xy(T x, T y) : x(x), y(y) {}

    template <typename U> explicit constexpr point_xy(U xy) : x(xy), y(xy) {}

    constexpr point_xy(const point_xy &p) : x(p.x), y(p.y) {}
    point_xy &operator*=(const float &f) {
        x *= f;
        y *= f;
        return *this;
    }
    point_xy &operator/=(const float &f) {
        // *this = point_xy_math::div(*this, f);
        x /= f;
        y /= f;
        return *this;
    }
    point_xy &operator*=(const double &f) {
        // *this = point_xy_math::mul(*this, f);
        x *= f;
        y *= f;
        return *this;
    }
    point_xy &operator/=(const double &f) {
        // *this = point_xy_math::div(*this, f);
        x /= f;
        y /= f;
        return *this;
    }

    point_xy &operator/=(const uint16_t &d) {
        // *this = point_xy_math::div(*this, d);
        x /= d;
        y /= d;
        return *this;
    }

    point_xy &operator/=(const int &d) {
        // *this = point_xy_math::div(*this, d);
        x /= d;
        y /= d;
        return *this;
    }

    point_xy &operator/=(const point_xy &p) {
        // *this = point_xy_math::div(*this, p);
        x /= p.x;
        y /= p.y;
        return *this;
    }

    point_xy &operator+=(const point_xy &p) {
        //*this = point_xy_math::add(*this, p);
        x += p.x;
        y += p.y;
        return *this;
    }

    point_xy &operator-=(const point_xy &p) {
        // *this = point_xy_math::sub(*this, p);
        x -= p.x;
        y -= p.y;
        return *this;
    }

    point_xy &operator=(const point_xy &p) {
        x = p.x;
        y = p.y;
        return *this;
    }

    point_xy operator-(const point_xy &p) const {
        return point_xy(x - p.x, y - p.y);
    }

    point_xy operator+(const point_xy &p) const {
        return point_xy(x + p.x, y + p.y);
    }

    point_xy operator*(const point_xy &p) const {
        return point_xy(x * p.x, y * p.y);
    }

    point_xy operator/(const point_xy &p) const {
        return point_xy(x / p.x, y / p.y);
    }

    template <typename NumberT> point_xy operator+(const NumberT &p) const {
        return point_xy(x + p, y + p);
    }

    template <typename U> point_xy operator+(const point_xy<U> &p) const {
        return point_xy(x + p.x, y + p.x);
    }

    template <typename NumberT> point_xy operator-(const NumberT &p) const {
        return point_xy(x - p, y - p);
    }

    template <typename NumberT> point_xy operator*(const NumberT &p) const {
        return point_xy(x * p, y * p);
    }

    template <typename NumberT> point_xy operator/(const NumberT &p) const {
        T a = x / p;
        T b = y / p;
        return point_xy<T>(a, b);
    }

    bool operator==(const point_xy &p) const { return (x == p.x && y == p.y); }

    bool operator!=(const point_xy &p) const { return (x != p.x || y != p.y); }

    template <typename U> bool operator==(const point_xy<U> &p) const {
        return (x == p.x && y == p.y);
    }

    template <typename U> bool operator!=(const point_xy<U> &p) const {
        return (x != p.x || y != p.y);
    }

    point_xy getMax(const point_xy &p) const {
        return point_xy(MAX(x, p.x), MAX(y, p.y));
    }

    point_xy getMin(const point_xy &p) const {
        return point_xy(MIN(x, p.x), MIN(y, p.y));
    }

    template <typename U> point_xy<U> cast() const {
        return point_xy<U>(static_cast<U>(x), static_cast<U>(y));
    }

    template <typename U> point_xy getMax(const point_xy<U> &p) const {
        return point_xy<U>(MAX(x, p.x), MAX(y, p.y));
    }

    template <typename U> point_xy getMin(const point_xy<U> &p) const {
        return point_xy<U>(MIN(x, p.x), MIN(y, p.y));
    }

    T distance(const point_xy &p) const {
        T dx = x - p.x;
        T dy = y - p.y;
        return sqrt(dx * dx + dy * dy);
    }

    bool is_zero() const { return (x == 0 && y == 0); }
};

using point_xy_float = point_xy<float>; // Full precision but slow.

// Legacy support

using pair_xy_float = point_xy<float>; // Legacy name for point_xy_float

// pair_xy<T> is the legacy name for point_xy<T>
template <typename T> struct pair_xy : public point_xy<T> {
    using value_type = T;
    using point_xy<T>::point_xy;
    pair_xy() = default;
    pair_xy(const point_xy<T> &p) : point_xy<T>(p) {}
};

template <typename T> struct line_xy {
    point_xy<T> start;
    point_xy<T> end;

    line_xy() = default;
    line_xy(const point_xy<T> &start, const point_xy<T> &end)
        : start(start), end(end) {}

    line_xy(T start_x, T start_y, T end_x, T end_y)
        : start(start_x, start_y), end(end_x, end_y) {}

    bool empty() const { return (start == end); }

    float distance_to(const point_xy<T> &p,
                      point_xy<T> *out_projected = nullptr) const {
        return distance_to_line_with_point(p, start, end, out_projected);
    }

  private:
    // Computes the closest distance from `p` to the line through `a` and `b`,
    // and writes the projected point.
    static float distance_to_line_with_point(point_xy<T> p, point_xy<T> a,
                                             point_xy<T> b,
                                             point_xy<T> *out_projected) {
        point_xy<T> maybe;
        point_xy<T> &out_proj = out_projected ? *out_projected : maybe;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len_sq = dx * dx + dy * dy;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        const bool is_zero = (len_sq == 0.0f);
#pragma GCC diagnostic pop

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

template <typename T> struct rect_xy {
    point_xy<T> mMin;
    point_xy<T> mMax;

    rect_xy() = default;
    rect_xy(const point_xy<T> &min, const point_xy<T> &max)
        : mMin(min), mMax(max) {}

    rect_xy(T min_x, T min_y, T max_x, T max_y)
        : mMin(min_x, min_y), mMax(max_x, max_y) {}

    uint16_t width() const { return mMax.x - mMin.x; }

    uint16_t height() const { return mMax.y - mMin.y; }

    bool empty() const { return (mMin.x == mMax.x && mMin.y == mMax.y); }

    void expand(const point_xy<T> &p) { expand(p.x, p.y); }

    void expand(const rect_xy &r) {
        expand(r.mMin);
        expand(r.mMax);
    }

    void expand(T x, T y) {
        mMin.x = MIN(mMin.x, x);
        mMin.y = MIN(mMin.y, y);
        mMax.x = MAX(mMax.x, x);
        mMax.y = MAX(mMax.y, y);
    }

    bool contains(const point_xy<T> &p) const {
        return (p.x >= mMin.x && p.x < mMax.x && p.y >= mMin.y && p.y < mMax.y);
    }

    bool contains(const T &x, const T &y) const {
        return contains(point_xy<T>(x, y));
    }

    bool operator==(const rect_xy &r) const {
        return (mMin == r.mMin && mMax == r.mMax);
    }

    bool operator!=(const rect_xy &r) const { return !(*this == r); }

    template <typename U> bool operator==(const rect_xy<U> &r) const {
        return (mMin == r.mMin && mMax == r.mMax);
    }

    template <typename U> bool operator!=(const rect_xy<U> &r) const {
        return !(*this == r);
    }
};

} // namespace fl
