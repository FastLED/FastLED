#pragma once


#ifdef __EMSCRIPTEN__
#define FL_ALIGN_BYTES 8
#elif defined(__AVR__)
#define FL_ALIGN_BYTES 1
#else
#define FL_ALIGN_BYTES 4
#endif

#define FL_ALIGN alignas(FL_ALIGN_BYTES)
