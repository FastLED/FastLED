/// @file string_view.cpp
/// @brief Out-of-class definitions for fl::string_view

#include "fl/string_view.h"

namespace fl {

// ODR definition for string_view::npos
// Required for C++11/14 when the address of npos is taken (ODR-used)
constexpr fl::size string_view::npos;

} // namespace fl
