#pragma once

#include <string.h>  // for standard memset fallback

namespace fl {

// fl::memset - provides a FastLED-specific memset implementation
// that is compatible across all supported platforms
template <typename T>
inline void* memset(T* ptr, int value, size_t num) {
    return ::memset(static_cast<void*>(ptr), value, num);
}

// Overload for void* to maintain standard memset signature
inline void* memset(void* ptr, int value, size_t num) {
    return ::memset(ptr, value, num);
}

} // namespace fl
