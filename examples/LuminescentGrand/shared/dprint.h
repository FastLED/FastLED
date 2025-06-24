
#pragma once


extern bool is_debugging;
//#define ENABLE_DPRINT
#ifdef ENABLE_DPRINT
 #define dprint(x) if (is_debugging) { Serial.print(x); }
 #define dprintln(x) if (is_debugging) { Serial.println(x); }
#else
 #define dprint(x)
 #define dprintln(x)
#endif