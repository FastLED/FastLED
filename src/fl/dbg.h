#pragma once

#if defined(DEBUG) && defined(FASTLED_DBG_USE_IOSTREAM) && (!defined(FASTLED_DBG_USE_IOSTREAM) && (defined(__EMSCRIPTEN__) || defined(__IMXRT1062__)) || defined(ESP32))
#ifdef DEBUG
#define FASTLED_DBG_USE_IOSTREAM
#endif
#endif

#if defined(FASTLED_DBG_USE_IOSTREAM)
#define FASTLED_HAS_DBG 1
#include <iostream>  // ok include
using std::endl;
using std::cout;
#define _FASTLED_DBG_FILE_OFFSET 12  // strlen("fastled/src/")
#define _FASTLED_DGB(X) cout << (&__FILE__[_FASTLED_DBG_FILE_OFFSET]) << "(" << __LINE__ << "): " << X
#define FASTLED_DBG(X) _FASTLED_DGB(X) << endl
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