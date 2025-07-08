#pragma once


#include "fl/stdint.h"
#include "fl/int.h"
// This file must not be in the fl namespace, it must be in the global
// namespace.

#if (defined(__AVR__) || !defined(__has_include)) && (!defined(FASTLED_HAS_NEW))
#ifndef __has_include
#define _NO_EXCEPT
#else
#define _NO_EXCEPT noexcept
#endif
inline void *operator new(fl::size, void *ptr) _NO_EXCEPT { return ptr; }
#elif __has_include(<new>)
#include <new>
#elif __has_include(<new.h>)
#include <new.h>
#elif __has_include("new.h")
#include "new.h"
#endif
