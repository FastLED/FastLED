#pragma once

#include <stddef.h>
#include "fl/namespace.h"

// Define if initializer_list is available
// Check for C++11 first, then check if std::initializer_list exists 
#if __cplusplus >= 201103L && !defined(__AVR__) && !defined(FASTLED_NO_STD_INITIALIZER_LIST)
// Try to use standard library version on most platforms
#include <initializer_list>
#define FASTLED_HAS_INITIALIZER_LIST 1
#define FASTLED_USE_STD_INITIALIZER_LIST 1
namespace fl {
    using std::initializer_list;
}
#elif __cplusplus >= 201103L
// Use a simpler approach for AVR and other platforms that don't have proper std::initializer_list
#define FASTLED_HAS_INITIALIZER_LIST 1  
#define FASTLED_USE_STD_INITIALIZER_LIST 0
// For AVR and other limited platforms, we'll still enable the feature but handle it differently
// The constructors will be implemented differently in vector.h
#else
#define FASTLED_HAS_INITIALIZER_LIST 0
#define FASTLED_USE_STD_INITIALIZER_LIST 0
#endif 