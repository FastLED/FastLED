#pragma once

#include "fl/strstream.h"

#ifndef FASTLED_DBG_USE_IOSTREAM
#if defined(DEBUG) && (defined(__EMSCRIPTEN__) || defined(__IMXRT1062__) || defined(ESP32))
#define FASTLED_DBG_USE_IOSTREAM 1
#else
#define FASTLED_DBG_USE_IOSTREAM 0
#endif
#endif


#if FASTLED_DBG_USE_IOSTREAM
#define FASTLED_HAS_DBG 1
#include <iostream>  // ok include
inline const char* _fastled_file_offset(const char* file) {
  const char* p = file;
  while (*p) {
    if (*p == '/') {
      file = p + 1;
    }
    p++;
  }
  return file;
}
#define _FASTLED_DGB(X) \
  (std::cout <<         \
    (fl::StrStream() << \
       (_fastled_file_offset(__FILE__)) <<  "(" << __LINE__ << "): " << X) \
    .c_str() << std::endl)

#define FASTLED_DBG(X) _FASTLED_DGB(X) << std::endl
#endif


#ifndef FASTLED_HAS_DBG
// FASTLED_DBG is a macro that can be defined to enable debug printing.
#define FASTLED_DBG(X)
#endif

#ifndef FASTLED_DBG_IF
#ifdef FASTLED_HAS_DBG
#define FASTLED_DBG_IF(COND, MSG) if (COND) FASTLED_DBG(MSG)
#else
#define FASTLED_DBG_IF(COND, MSG)
#endif  // FASTLED_HAS_DBG
#endif  // FASTLED_DBG_IF