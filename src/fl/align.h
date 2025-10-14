#pragma once



#if defined(FASTLED_TESTING) || defined(__EMSCRIPTEN__)
// alignof is a built-in keyword in C++11, no header needed
// max_align_t is not used in this file
#endif

#ifdef __EMSCRIPTEN__
#define FL_ALIGN_BYTES 8
#define FL_ALIGN alignas(FL_ALIGN_BYTES)
#define FL_ALIGN_AS(T) alignas(alignof(T))
#else
#define FL_ALIGN_BYTES 1
#define FL_ALIGN
#define FL_ALIGN_AS(T)
#endif
