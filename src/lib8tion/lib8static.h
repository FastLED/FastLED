/// @file lib8static.h
/// Defines static inlining macros for lib8tion functions

#pragma once

#include "fl/force_inline.h"

/// @addtogroup lib8tion
/// @{

/// Define a LIB8TION member function as static inline with an "unused" attribute
#define LIB8STATIC __attribute__ ((unused)) static inline

/// @deprecated Use FL_ALWAYS_INLINE from fl/force_inline.h instead
/// Define a LIB8TION member function as always static inline
/// This macro is deprecated and should be replaced with FL_ALWAYS_INLINE
#define LIB8STATIC_ALWAYS_INLINE FL_ALWAYS_INLINE

/// @} lib8tion
