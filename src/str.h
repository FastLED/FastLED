#pragma once

#ifndef FASTLED_HAS_NATIVE_WSTRING
#if __has_include("wstring.h") || __has_include(<wstring.h>)
#define FASTLED_HAS_NATIVE_WSTRING 1
#else
#define FASTLED_HAS_NATIVE_WSTRING 0
#endif
#endif  // FASTLED_HAS_NATIVE_WSTRING

#if FASTLED_HAS_NATIVE_WSTRING
#include "wstring.h"
#else
#include "third_party/wstring/wstring.h"
#endif  // FASTLED_HAS_NATIVE_WSTRING