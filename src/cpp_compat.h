#pragma once

/// @file cpp_compat.h
/// Compatibility functions based on C++ version

#ifndef __INC_CPP_COMPAT_H
#define __INC_CPP_COMPAT_H

#include "fl/stl/static_assert.h"

#if __cplusplus <= 199711L

/// Declares that it is possible to evaluate a value at compile time, introduced in C++11
/// @see https://en.cppreference.com/w/cpp/language/constexpr
#define constexpr const

#endif

#include "fl/stl/compiler_control.h"

#endif
