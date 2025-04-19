
namespace fl {

template<typename PairXyType>
struct point_xy_traits {
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
        *this = point_xy_traits<point_xy>::div(*this, f);
        return *this;
    }
    point_xy& operator*=(const double& f) {
        *this = point_xy_traits<point_xy>::mul(*this, f);
        return *this;
    }
    point_xy& operator/=(const double& f) {
        *this = point_xy_traits<point_xy>::div(*this, f);
        return *this;
    }

    point_xy& operator/=(const uint16_t& d) {
        *this = point_xy_traits<point_xy>::div(*this, d);
        return *this;
    }

    point_xy& operator/=(const int& d) {
        *this = point_xy_traits<point_xy>::div(*this, d);
        return *this;
    }

    point_xy& operator/=(const point_xy& p) {
        *this = point_xy_traits<point_xy>::div(*this, p);
        return *this;
    }

    point_xy& operator+=(const point_xy& p) {
        *this = point_xy_traits<point_xy>::add(*this, p);
        return *this;
    }

    point_xy& operator-=(const point_xy& p) {
        *this = point_xy_traits<point_xy>::sub(*this, p);
        return *this;
    }

    point_xy& operator=(const point_xy& p) {
        x = p.x;
        y = p.y;
        return *this;
    }

    point_xy operator-(const point_xy& p) const {
        return point_xy_traits<point_xy>::sub(*this, p);
    }

    point_xy operator+(const point_xy& p) const {
        return point_xy_traits<point_xy>::add(*this, p);
    }

    point_xy operator*(const point_xy& p) const {
        return point_xy_traits<point_xy>::mul(*this, p);
    }

    point_xy operator/(const point_xy& p) const {
        return point_xy_traits<point_xy>::div(*this, p);
    }

    template<typename NumberT>
    point_xy operator+(const NumberT& p) const {
        return point_xy_traits<point_xy>::add(*this, p);
    }

    template<typename NumberT>
    point_xy operator-(const NumberT& p) const {
        return point_xy_traits<point_xy>::sub(*this, p);
    }

    template<typename NumberT>
    point_xy operator*(const NumberT& p) const {
        return point_xy_traits<point_xy>::mul(*this, p);
    }

    template<typename NumberT>
    point_xy operator/(const NumberT& p) const {
        return point_xy_traits<point_xy>::div(*this, p);
    }

    bool operator==(const point_xy& p) const {
        return (x == p.x && y == p.y);
    }

    bool operator!=(const point_xy& p) const {
        return (x != p.x || y != p.y);
    }
};

using point_xy_float = point_xy<float>;  // It's just easier if we allow negative values.
using pair_xy_float = point_xy<float>;  // Legacy name for point_xy_float


}  // namespace fl
