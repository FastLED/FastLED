#pragma once

#ifndef FASTLED_HAS_NATIVE_WSTRING
#if FASTLED_NATIVE_WSTRING_INCLUDE_OVERRIDE
// Allow platforms to define their own wstring location.
#define FASTLED_HAS_NATIVE_WSTRING 1
#define FASTLED_NATIVE_WSTRING_INCLUDE FASTLED_NATIVE_WSTRING_INCLUDE_OVERRIDE
#elif __has_include("wstring.h") || __has_include(<wstring.h>) || defined(__AVR__)
#define FASTLED_HAS_NATIVE_WSTRING 1
#define FASTLED_NATIVE_WSTRING_INCLUDE <wstring.h>
#else
#define FASTLED_HAS_NATIVE_WSTRING 0
#endif
#endif  // FASTLED_HAS_NATIVE_WSTRING


#if FASTLED_HAS_NATIVE_WSTRING
#include FASTLED_NATIVE_WSTRING_INCLUDE
#else
#include "third_party/wstring/wstring.h"
#endif  // FASTLED_HAS_NATIVE_WSTRING