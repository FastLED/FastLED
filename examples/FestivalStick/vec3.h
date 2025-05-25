#pragma once


namespace sketch {

template <typename T> struct vec3 {
    // value_type
    using value_type = T;
    T x = 0;
    T y = 0;
    T z = 0;
    constexpr vec3() = default;
    constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) {}

    template <typename U> explicit constexpr vec3(U xyz) : x(xyz), y(xyz), z(xyz) {}

    constexpr vec3(const vec3 &p) : x(p.x), y(p.y), z(p.z) {}
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

    vec3 &operator/=(const uint16_t &d) {
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

    vec3 operator-(const vec3 &p) const { return vec3(x - p.x, y - p.y, z - p.z); }

    vec3 operator+(const vec3 &p) const { return vec3(x + p.x, y + p.y, z + p.z); }

    vec3 operator*(const vec3 &p) const { return vec3(x * p.x, y * p.y, z * p.z); }

    vec3 operator/(const vec3 &p) const { return vec3(x / p.x, y / p.y, z / p.z); }

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

    bool operator==(const vec3 &p) const { return (x == p.x && y == p.y && z == p.z); }

    bool operator!=(const vec3 &p) const { return (x != p.x || y != p.y || z != p.z); }

    template <typename U> bool operator==(const vec3<U> &p) const {
        return (x == p.x && y == p.y && z == p.z);
    }

    template <typename U> bool operator!=(const vec3<U> &p) const {
        return (x != p.x || y != p.y || z != p.z);
    }

    vec3 getMax(const vec3 &p) const { return vec3(MAX(x, p.x), MAX(y, p.y), MAX(z, p.z)); }

    vec3 getMin(const vec3 &p) const { return vec3(MIN(x, p.x), MIN(y, p.y), MIN(z, p.z)); }

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

using vec3f = vec3<float>;

}  // namespace sketch
