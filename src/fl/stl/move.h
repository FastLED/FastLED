#pragma once
#include "fl/stl/noexcept.h"
#include "fl/stl/has_include.h"

// FastLED#4827: fl::CRGB lives in namespace fl, so ADL pulls fl::move /
// fl::forward into unqualified `move(led)` / `forward<T>(x)` calls made under
// `using namespace std`, where a separate fl template would tie with std's.
// When the standard <utility> exists,
// fl::move and fl::forward ARE std::move and std::forward, so ADL finds one
// function. AVR and other toolchains without <utility> keep fl's own versions.
#ifndef FL_MOVE_FORWARD_IS_STD
#if FL_HAS_INCLUDE(<utility>)
#define FL_MOVE_FORWARD_IS_STD 1
#else
#define FL_MOVE_FORWARD_IS_STD 0
#endif
#endif

#if FL_MOVE_FORWARD_IS_STD
// IWYU pragma: begin_keep
#include <utility>  // IWYU pragma: keep
// IWYU pragma: end_keep
#endif

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

#if FL_MOVE_FORWARD_IS_STD
using std::move;  // okay std namespace // ok bare using
#else
// Implementation of move
template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) FL_NO_EXCEPT {
    return static_cast<typename remove_reference<T>::type &&>(t);
}
#endif

} // namespace fl
