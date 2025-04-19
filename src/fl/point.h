
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

}  // namespace fl
