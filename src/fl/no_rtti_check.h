#pragma once

// RTTI Detection and Enforcement
// This header ensures RTTI is properly disabled in FastLED builds

#include "fl/namespace.h"

// Only perform RTTI check when explicitly requested
#ifdef FASTLED_CHECK_NO_RTTI

// Compile-time RTTI detection
#ifdef __cpp_rtti
    #error "🚨 CRITICAL: RTTI is ENABLED! FastLED requires RTTI to be disabled. Use -fno-rtti compiler flag."
#endif

// Check for RTTI via compiler-specific macros
#if defined(__GXX_RTTI) || defined(_CPPRTTI) || defined(__RTTI) || defined(__RTTI__)
    #error "🚨 CRITICAL: RTTI is ENABLED! FastLED requires RTTI to be disabled. Use -fno-rtti compiler flag."
#endif

// GCC-specific RTTI detection
#ifdef __GNUC__
    #if defined(__GXX_RTTI)
        #error "🚨 CRITICAL: GCC RTTI is ENABLED! Use -fno-rtti compiler flag."
    #endif
#endif

// Clang-specific RTTI detection
#ifdef __clang__
    #if __has_feature(cxx_rtti)
        #error "🚨 CRITICAL: Clang RTTI is ENABLED! Use -fno-rtti compiler flag."
    #endif
#endif

// MSVC-specific RTTI detection
#ifdef _MSC_VER
    #ifdef _CPPRTTI
        #error "🚨 CRITICAL: MSVC RTTI is ENABLED! Use /GR- compiler flag."
    #endif
#endif

// Runtime check function for additional verification
namespace fl {

#ifdef FASTLED_TESTING
inline void verify_no_rtti_runtime() {
    // This function will be called during test initialization
    // to ensure RTTI is properly disabled at runtime
    
    // Note: We can't use typeid or dynamic_cast here since RTTI is disabled
    // This is just a placeholder for any future runtime checks
}
#endif

} // namespace fl

#endif // FASTLED_CHECK_NO_RTTI 
