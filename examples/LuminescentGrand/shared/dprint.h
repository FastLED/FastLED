
#pragma once

#include "fl/unused.h"


extern bool is_debugging;
//#define ENABLE_DPRINT
#ifdef ENABLE_DPRINT
 #define dprint(x) if (is_debugging) { Serial.print(x); }
 #define dprintln(x) if (is_debugging) { Serial.println(x); }
#else
 #define dprint(x) FL_UNUSED(x)
 #define dprintln(x) FL_UNUSED(x)
#endif
