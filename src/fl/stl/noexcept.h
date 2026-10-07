#pragma once

// Experimental real-noexcept measurement branch (#4707).
// This branch enables existing annotations for board and native validation.
// Generic callback/construction contracts still need an audit before shipping.

#ifndef FL_NO_EXCEPT
#define FL_NO_EXCEPT noexcept
#define FL_HAS_NOEXCEPT 1
#endif

// Ownership destructors release memory without propagating exceptions.
#ifndef FL_DTOR_NOEXCEPT
#define FL_DTOR_NOEXCEPT noexcept
#endif
