#pragma once

// constrict the ArduinoJson library to only the features we need. Otherwise
// we get compiler errors on the avr platforms and others.
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#define ARDUINOJSON_ENABLE_STRING_VIEW 0
#define ARDUINOJSON_ENABLE_STD_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0

#if defined(ARDUINO)
#define __RESTORE_ARDUINO ARDUINO
#undef ARDUINO
#endif

#define FASTLED_JSON_GUARD

#include "json.hpp"

#undef FASTLED_JSON_GUARD

#if defined(__RESTORE_ARDUINO)
#define ARDUINO __RESTORE_ARDUINO
#undef __RESTORE_ARDUINO
#endif
