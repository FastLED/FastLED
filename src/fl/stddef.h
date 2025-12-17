#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Standard Definition Types (stddef.h equivalents)
//
// IMPORTANT: FastLED has carefully purged <stddef.h> from the include path
// because including that header adds compilation time to every *.o file.
//
// This file provides size_t, ptrdiff_t, NULL, and offsetof macro for C++
// using the fl/cstddef.h foundation, similar to how fl/stdint.h works.
///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

#include "fl/stl/cstddef.h"

// Provide NULL definition for third-party code that needs it
#ifndef NULL
#define NULL nullptr
#endif

#else
// C language support: delegate to fl/cstddef.h
#include "fl/stl/cstddef.h"

#endif  // __cplusplus
