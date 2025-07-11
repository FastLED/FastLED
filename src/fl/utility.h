#pragma once
#include "fl/type_traits.h"

namespace fl {

/**
 * @brief Binary function object that returns whether the first argument is less
 * than the second
 *
 * This functor mimics std::less from the C++ standard library.
 *
 * @tparam T Type of the arguments being compared
 */
template <typename T = void> struct less {
    /**
     * @brief Function call operator that performs the less-than comparison
     *
     * @param lhs Left-hand side argument
     * @param rhs Right-hand side argument
     * @return true if lhs < rhs, false otherwise
     */
    constexpr bool operator()(const T &lhs, const T &rhs) const {
        return lhs < rhs;
    }
};

/**
 * @brief Specialization of less for void, allowing for transparent comparisons
 *
 * This specialization allows the functor to be used with different types
 * as long as they support the less-than operator.
 */
template <> struct less<void> {
    /**
     * @brief Function call operator that performs the less-than comparison
     *
     * @tparam T Type of the left-hand side argument
     * @tparam U Type of the right-hand side argument
     * @param lhs Left-hand side argument
     * @param rhs Right-hand side argument
     * @return true if lhs < rhs, false otherwise
     */
    template <typename T, typename U>
    constexpr auto operator()(T &&lhs, U &&rhs) const
        -> decltype(fl::forward<T>(lhs) < fl::forward<U>(rhs)) {
        return fl::forward<T>(lhs) < fl::forward<U>(rhs);
    }
};

// Backward compatibility alias
template <typename T> using DefaultLess = less<T>;

} // namespace fl
