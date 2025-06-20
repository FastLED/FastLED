#pragma once

#include <stddef.h>
#include "fl/namespace.h"

// Define if initializer_list is available
// Check for C++11 and if std::initializer_list exists
#if __cplusplus >= 201103L && !defined(__AVR__) && !defined(FASTLED_NO_STD_INITIALIZER_LIST)
// Use standard library version on most platforms
#include <initializer_list>
#define FASTLED_HAS_INITIALIZER_LIST 1
namespace fl {
    using std::initializer_list;
}
#else
// No initializer list support - containers should not try to emulate the features
#define FASTLED_HAS_INITIALIZER_LIST 0
#endif 