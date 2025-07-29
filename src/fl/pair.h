#pragma once

#include "fl/move.h"
#include "fl/compiler_control.h"
#include "fl/type_traits.h"

namespace fl {

template <typename T1, typename T2> struct pair {
    // Member typedefs for std::pair compatibility
    using first_type = T1;
    using second_type = T2;
    
    T1 first = T1();
    T2 second = T2();
    
    // Default constructor
    pair() = default;
    
    // Constructor from values
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    pair(const T1 &k, const T2 &v) : first(k), second(v) {}
    FL_DISABLE_WARNING_POP
    
    // Perfect forwarding constructor
    template <typename U1, typename U2>
    pair(U1&& u1, U2&& u2) : first(fl::forward<U1>(u1)), second(fl::forward<U2>(u2)) {}
    
    // Copy constructor from different pair types
    template <typename U1, typename U2>
    pair(const pair<U1, U2> &other) : first(other.first), second(other.second) {}
    
    // Move constructor from different pair types
    template <typename U1, typename U2>
    pair(pair<U1, U2> &&other) : first(fl::move(other.first)), second(fl::move(other.second)) {}
    
    // Rule of 5: copy constructor, copy assignment, move constructor, move assignment, destructor
    pair(const pair &other) = default;
    pair &operator=(const pair &other) = default;
    pair(pair &&other) noexcept : first(fl::move(other.first)), second(fl::move(other.second)) {}
    pair &operator=(pair &&other) = default;
    
    // Note: Template assignment operators removed to avoid issues with const members
    // The default copy and move assignment operators will handle same-type assignments
    
    // Swap member function
    void swap(pair &other) noexcept {
        fl::swap(first, other.first);
        fl::swap(second, other.second);
    }
};

// Comparison operators
template <typename T1, typename T2, typename U1, typename U2>
bool operator==(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator!=(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return !(lhs == rhs);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator<(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return lhs.first < rhs.first || (!(rhs.first < lhs.first) && lhs.second < rhs.second);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator<=(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return !(rhs < lhs);
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator>(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return rhs < lhs;
}

template <typename T1, typename T2, typename U1, typename U2>
bool operator>=(const pair<T1, T2> &lhs, const pair<U1, U2> &rhs) {
    return !(lhs < rhs);
}

// Non-member swap function
template <typename T1, typename T2>
void swap(pair<T1, T2> &lhs, pair<T1, T2> &rhs) noexcept {
    lhs.swap(rhs);
}

// make_pair function
template <typename T1, typename T2>
pair<typename fl::decay<T1>::type, typename fl::decay<T2>::type> make_pair(T1&& t, T2&& u) {
    return pair<typename fl::decay<T1>::type, typename fl::decay<T2>::type>(fl::forward<T1>(t), fl::forward<T2>(u));
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
typename pair_element<I, T1, T2>::type& get(pair<T1, T2> &p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if (I == 0) {
        return p.first;
    } else {
        return p.second;
    }
}

template <fl::size I, typename T1, typename T2>
const typename pair_element<I, T1, T2>::type& get(const pair<T1, T2> &p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if (I == 0) {
        return p.first;
    } else {
        return p.second;
    }
}

template <fl::size I, typename T1, typename T2>
typename pair_element<I, T1, T2>::type&& get(pair<T1, T2> &&p) noexcept {
    static_assert(I < 2, "Index out of bounds for pair");
    if (I == 0) {
        return fl::move(p.first);
    } else {
        return fl::move(p.second);
    }
}

// get by type overloads (when T1 and T2 are different types)
template <typename T, typename T1, typename T2>
T& get(pair<T1, T2> &p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if (fl::is_same<T, T1>::value) {
        return p.first;
    } else {
        return p.second;
    }
}

template <typename T, typename T1, typename T2>
const T& get(const pair<T1, T2> &p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if (fl::is_same<T, T1>::value) {
        return p.first;
    } else {
        return p.second;
    }
}

template <typename T, typename T1, typename T2>
T&& get(pair<T1, T2> &&p) noexcept {
    static_assert(fl::is_same<T, T1>::value || fl::is_same<T, T2>::value, 
                  "Type T must be one of the pair's element types");
    static_assert(!(fl::is_same<T, T1>::value && fl::is_same<T, T2>::value), 
                  "Type T must be unique in the pair");
    if (fl::is_same<T, T1>::value) {
        return fl::move(p.first);
    } else {
        return fl::move(p.second);
    }
}

// Tuple-like helper classes
template <typename T>
struct tuple_size;

template <typename T1, typename T2>
struct tuple_size<pair<T1, T2>> {
    static constexpr fl::size value = 2;
};

template <fl::size I, typename T>
struct tuple_element;

template <typename T1, typename T2>
struct tuple_element<0, pair<T1, T2>> {
    using type = T1;
};

template <typename T1, typename T2>
struct tuple_element<1, pair<T1, T2>> {
    using type = T2;
};

template <typename T1, typename T2>
using Pair = pair<T1, T2>;  // Backwards compatibility for 3.10.1

} // namespace fl
