#pragma once

// Arduino JSON may be included by the user, so we need to save the current state
// of the macros and restore them after including the library
#pragma push_macro("ARDUINO")
#pragma push_macro("ARDUINOJSON_ENABLE_STD_STREAM")
#pragma push_macro("ARDUINOJSON_ENABLE_STRING_VIEW")
#pragma push_macro("ARDUINOJSON_ENABLE_STD_STRING")
#pragma push_macro("ARDUINOJSON_ENABLE_ARDUINO_STRING")
#pragma push_macro("ARDUINOJSON_ENABLE_ARDUINO_STREAM")
#pragma push_macro("ARDUINOJSON_ENABLE_ARDUINO_PRINT")
#pragma push_macro("ARDUINOJSON_ENABLE_PROGMEM")
#pragma push_macro("min")
#pragma push_macro("max")

#define ARDUINOJSON_USE_LONG_LONG 1

// Safely undefine FLArduinoJson macros if defined
#ifdef ARDUINOJSON_ENABLE_STD_STREAM
#undef ARDUINOJSON_ENABLE_STD_STREAM
#endif
#define ARDUINOJSON_ENABLE_STD_STREAM 0

#ifdef ARDUINOJSON_ENABLE_STRING_VIEW
#undef ARDUINOJSON_ENABLE_STRING_VIEW
#endif
#define ARDUINOJSON_ENABLE_STRING_VIEW 0

#ifdef ARDUINOJSON_ENABLE_STD_STRING
#undef ARDUINOJSON_ENABLE_STD_STRING
#endif

#ifdef __EMSCRIPTEN__
#define ARDUINOJSON_ENABLE_STD_STRING 1
#else
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif

#ifdef ARDUINOJSON_ENABLE_ARDUINO_STRING
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
#endif
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0

#ifdef ARDUINOJSON_ENABLE_ARDUINO_STREAM
#undef ARDUINOJSON_ENABLE_ARDUINO_STREAM
#endif
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0

#ifdef ARDUINOJSON_ENABLE_ARDUINO_PRINT
#undef ARDUINOJSON_ENABLE_ARDUINO_PRINT
#endif
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0

#ifdef ARDUINOJSON_ENABLE_PROGMEM
#undef ARDUINOJSON_ENABLE_PROGMEM
#endif
#define ARDUINOJSON_ENABLE_PROGMEM 0

#ifdef ARDUINO
#undef ARDUINO
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif


#define FASTLED_JSON_GUARD
#include "json.hpp"
#undef FASTLED_JSON_GUARD


#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("ARDUINOJSON_ENABLE_PROGMEM")
#pragma pop_macro("ARDUINOJSON_ENABLE_ARDUINO_PRINT")
#pragma pop_macro("ARDUINOJSON_ENABLE_ARDUINO_STREAM")
#pragma pop_macro("ARDUINOJSON_ENABLE_ARDUINO_STRING")
#pragma pop_macro("ARDUINOJSON_ENABLE_STD_STRING")
#pragma pop_macro("ARDUINOJSON_ENABLE_STRING_VIEW")
#pragma pop_macro("ARDUINOJSON_ENABLE_STD_STREAM")
#pragma pop_macro("ARDUINO")
