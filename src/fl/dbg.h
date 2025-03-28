#pragma once

#include "fl/strstream.h"

namespace fl {
// ".build/src/fl/dbg.h" -> "src/fl/dbg.h"
// "blah/blah/blah.h" -> "blah.h"
inline const char* fastled_file_offset(const char* file) {
  const char* p = file;
  const char* last_slash = nullptr;
  
  while (*p) {
    if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && p[3] == '/') {
      return p;  // Skip past "src/"
    }
    if (*p == '/') {  // fallback to using last slash
      last_slash = p;
    }
    p++;
  }
  // If "src/" not found but we found at least one slash, return after the last slash
  if (last_slash) {
    return last_slash + 1;
  }
  return file;  // If no slashes found at all, return original path
}
}  // namespace fl

#ifdef __EMSCRIPTEN__
#define FASTLED_DBG_USE_PRINTF 1
#endif

#ifndef FASTLED_DBG_USE_PRINTF
#if defined(DEBUG) && (defined(__IMXRT1062__) || defined(ESP32))
#define FASTLED_DBG_USE_PRINTF 1
#else
#define FASTLED_DBG_USE_PRINTF 0
#endif
#endif


#if FASTLED_DBG_USE_PRINTF
#define FASTLED_HAS_DBG 1
#include <stdio.h>  // ok include
namespace fl {

}  // namespace fl
#define _FASTLED_DGB(X) \
  printf("%s", \
    (fl::StrStream() << \
       (fl::fastled_file_offset(__FILE__)) <<  "(" << __LINE__ << "): " << X << "\n") \
    .c_str())

#define FASTLED_DBG(X) _FASTLED_DGB(X)
#endif


#ifndef FASTLED_HAS_DBG
// FASTLED_DBG is a macro that can be defined to enable debug printing.
#define FASTLED_DBG(X) (fl::FakeStrStream() << X)
#endif

#ifndef FASTLED_DBG_IF
#ifdef FASTLED_HAS_DBG
#define FASTLED_DBG_IF(COND, MSG) if (COND) FASTLED_DBG(MSG)
#else
#define FASTLED_DBG_IF(COND, MSG) while(false && (COND)) { FASTLED_DBG(MSG); }
#endif  // FASTLED_HAS_DBG
#endif  // FASTLED_DBG_IF
