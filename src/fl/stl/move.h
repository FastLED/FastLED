#pragma once

namespace fl {


// Define remove_reference trait
template <typename T> struct remove_reference {
    using type = T;
};

// Specialization for lvalue reference
template <typename T> struct remove_reference<T &> {
    using type = T;
};

// Specialization for rvalue reference
template <typename T> struct remove_reference<T &&> {
    using type = T;
};

// Helper alias template for remove_reference
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

// Implementation of move
template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) noexcept {
    return static_cast<typename remove_reference<T>::type &&>(t);
}

} // namespace fl
