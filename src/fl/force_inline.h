#pragma once

// Force inline for member functions (no static keyword)
#ifdef FASTLED_NO_FORCE_INLINE
#define FASTLED_FORCE_INLINE inline
#else
#define FASTLED_FORCE_INLINE __attribute__((always_inline)) inline
#endif

// Force inline for static free functions (with static keyword)
// Replacement for LIB8STATIC_ALWAYS_INLINE
#ifdef FASTLED_NO_FORCE_INLINE
#define FL_ALWAYS_INLINE static inline
#else
#define FL_ALWAYS_INLINE __attribute__((always_inline)) static inline
#endif
