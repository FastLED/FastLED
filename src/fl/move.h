#pragma once

namespace fl {

// Forward declaration of remove_reference (defined in type_traits.h)
template <typename T> struct remove_reference {
    using type = T;
};

template <typename T> struct remove_reference<T &> {
    using type = T;
};

template <typename T> struct remove_reference<T &&> {
    using type = T;
};

// Define is_lvalue_reference trait (needed for forward)
template <typename T> struct is_lvalue_reference {
    static constexpr bool value = false;
};

template <typename T> struct is_lvalue_reference<T &> {
    static constexpr bool value = true;
};

// Implementation of move
template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) noexcept {
    return static_cast<typename remove_reference<T>::type &&>(t);
}

// Implementation of forward
template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &t) noexcept {
    return static_cast<T &&>(t);
}

// Overload for rvalue references
template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &&t) noexcept {
    static_assert(!is_lvalue_reference<T>::value,
                  "Cannot forward an rvalue as an lvalue");
    return static_cast<T &&>(t);
}

} // namespace fl
