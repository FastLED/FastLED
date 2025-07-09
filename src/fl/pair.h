#pragma once

#include "fl/move.h"
#include "fl/compiler_control.h"
#include "fl/type_traits.h"

namespace fl {

template <typename T1, typename T2> struct Pair {
    // Member typedefs for std::pair compatibility
    using first_type = T1;
    using second_type = T2;
    
    T1 first = T1();
    T2 second = T2();
    
    // Default constructor
    Pair() = default;
    
    // Constructor from values
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    Pair(const T1 &k, const T2 &v) : first(k), second(v) {}
    FL_DISABLE_WARNING_POP
    
    // Perfect forwarding constructor
    template <typename U1, typename U2>
    Pair(U1&& u1, U2&& u2) : first(fl::forward<U1>(u1)), second(fl::forward<U2>(u2)) {}
    
    // Copy constructor from different pair types
    template <typename U1, typename U2>
    Pair(const Pair<U1, U2> &other) : first(other.first), second(other.second) {}
    
    // Move constructor from different pair types
    template <typename U1, typename U2>
    Pair(Pair<U1, U2> &&other) : first(fl::move(other.first)), second(fl::move(other.second)) {}
    
    // Rule of 5: copy constructor, copy assignment, move constructor, move assignment, destructor
    Pair(const Pair &other) = default;
    Pair &operator=(const Pair &other) = default;
    Pair(Pair &&other) noexcept : first(fl::move(other.first)), second(fl::move(other.second)) {}
    Pair &operator=(Pair &&other) noexcept {
        if (this != &other) {
            first = fl::move(other.first);
            second = fl::move(other.second);
        }
        return *this;
    }
    
    // Assignment from different pair types
    template <typename U1, typename U2>
    Pair &operator=(const Pair<U1, U2> &other) {
        first = other.first;
        second = other.second;
        return *this;
    }
    
    template <typename U1, typename U2>
    Pair &operator=(Pair<U1, U2> &&other) {
        first = fl::move(other.first);
        second = fl::move(other.second);
        return *this;
    }
    
    // Swap member function
    void swap(Pair &other) noexcept {
        fl::swap(first, other.first);
        fl::swap(second, other.second);
    }
};

// Comparison operators
template <typename T1, typename T2, typename U1, typename U2>
bool operator==(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator!=(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return !(lhs == rhs);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator<(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return lhs.first < rhs.first || (!(rhs.first < lhs.first) && lhs.second < rhs.second);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator<=(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return !(rhs < lhs);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator>(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return rhs < lhs;
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator>=(const Pair<T1, T2> &lhs, const Pair<U1, U2> &rhs) {
    return !(lhs < rhs);
}

// Non-member swap function
template <typename T1, typename T2>
void swap(Pair<T1, T2> &lhs, Pair<T1, T2> &rhs) noexcept {
    lhs.swap(rhs);
}

// make_pair function
template <typename T1, typename T2>
Pair<typename fl::decay<T1>::type, typename fl::decay<T2>::type> make_pair(T1&& t, T2&& u) {
    return Pair<typename fl::decay<T1>::type, typename fl::decay<T2>::type>(fl::forward<T1>(t), fl::forward<T2>(u));
}

// Helper for get function
template <fl::size I, typename T1, typename T2>
struct pair_element;

template <typename T1, typename T2>
struct pair_element<0, T1, T2> {
    using type = T1;
};

template <typename T1, typename T2>
struct pair_element<1, T1, T2> {
    using type = T2;
};

// get function overloads for tuple-like access by index
template <fl::size I, typename T1, typename T2>
typename pair_element<I, T1, T2>::type& get(Pair<T1, T2> &p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if constexpr (I == 0) {
        return p.first;
    } else {
        return p.second;
    }
}

template <fl::size I, typename T1, typename T2>
const typename pair_element<I, T1, T2>::type& get(const Pair<T1, T2> &p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if constexpr (I == 0) {
        return p.first;
    } else {
        return p.second;
    }
}

template <fl::size I, typename T1, typename T2>
typename pair_element<I, T1, T2>::type&& get(Pair<T1, T2> &&p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if constexpr (I == 0) {
        return fl::move(p.first);
    } else {
        return fl::move(p.second);
    }
}

// get by type overloads (when T1 and T2 are different types)
template <typename T, typename T1, typename T2>
T& get(Pair<T1, T2> &p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if constexpr (fl::is_same<T, T1>::value) {
        return p.first;
    } else {
        return p.second;
    }
}

template <typename T, typename T1, typename T2>
const T& get(const Pair<T1, T2> &p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if constexpr (fl::is_same<T, T1>::value) {
        return p.first;
    } else {
        return p.second;
    }
}

template <typename T, typename T1, typename T2>
T&& get(Pair<T1, T2> &&p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if constexpr (fl::is_same<T, T1>::value) {
        return fl::move(p.first);
    } else {
        return fl::move(p.second);
    }
}

// Tuple-like helper classes
template <typename T>
struct tuple_size;

template <typename T1, typename T2>
struct tuple_size<Pair<T1, T2>> {
    static constexpr fl::size value = 2;
};

template <fl::size I, typename T>
struct tuple_element;

template <typename T1, typename T2>
struct tuple_element<0, Pair<T1, T2>> {
    using type = T1;
};

template <typename T1, typename T2>
struct tuple_element<1, Pair<T1, T2>> {
    using type = T2;
};

// std compatibility
template <typename T1, typename T2> using pair = Pair<T1, T2>;

} // namespace fl
