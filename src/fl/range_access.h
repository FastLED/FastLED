#pragma once

#include "fl/stdint.h"

namespace fl {

// fl::begin for arrays
template <typename T, fl::size N>
constexpr T* begin(T (&array)[N]) noexcept {
    return array;
}

// fl::end for arrays
template <typename T, fl::size N>
constexpr T* end(T (&array)[N]) noexcept {
    return array + N;
}

// fl::begin for containers with begin() member function
template <typename Container>
constexpr auto begin(Container& c) -> decltype(c.begin()) {
    return c.begin();
}

// fl::begin for const containers with begin() member function
template <typename Container>
constexpr auto begin(const Container& c) -> decltype(c.begin()) {
    return c.begin();
}

// fl::end for containers with end() member function
template <typename Container>
constexpr auto end(Container& c) -> decltype(c.end()) {
    return c.end();
}

// fl::end for const containers with end() member function
template <typename Container>
constexpr auto end(const Container& c) -> decltype(c.end()) {
    return c.end();
}

} // namespace fl
