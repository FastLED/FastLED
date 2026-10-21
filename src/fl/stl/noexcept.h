#pragma once

// Load SDK target definitions before selecting the exception contract. This
// also makes selection independent of whether FastLED.h was included first.
#include "platforms/is_platform.h" // IWYU pragma: keep

#ifndef FL_NO_EXCEPT
#if defined(FL_IS_ESP)
#define FL_NO_EXCEPT noexcept
#define FL_HAS_NOEXCEPT 1
#else
#define FL_NO_EXCEPT
#endif
#endif

// External overrides retain their existing caller-owned capability contract:
// define FL_HAS_NOEXCEPT as 1 alongside an override that guarantees no throw.
// Leave the flag absent for empty or potentially throwing overrides. When an
// external override omits the flag, the library makes no capability claim.
// Arbitrary conditional C++ noexcept expressions cannot be evaluated by the
// preprocessor, so preserve them without attempting token-based classification.

// Destructor-only exception specification. Destructors in ownership chains
// that only release memory can opt in without re-enabling FL_NO_EXCEPT across
// platforms for unrelated functions.
#ifndef FL_DTOR_NOEXCEPT
#define FL_DTOR_NOEXCEPT noexcept
#endif

// FastLED assumes nonthrowing operations, including user callbacks. System
// exception frames and crash diagnostics remain owned by the platform.
