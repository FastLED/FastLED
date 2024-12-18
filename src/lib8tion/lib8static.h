/// @file lib8static.h
/// Defines static inlining macros for lib8tion functions

#pragma once

/// @addtogroup lib8tion
/// @{

/// Define a LIB8TION member function as static inline with an "unused" attribute
#define LIB8STATIC __attribute__ ((unused)) static inline
/// Define a LIB8TION member function as always static inline
#define LIB8STATIC_ALWAYS_INLINE __attribute__ ((always_inline)) static inline

/// @} lib8tion
