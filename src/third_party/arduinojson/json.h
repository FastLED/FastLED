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
#pragma push_macro("ARDUINO")
#undef ARDUINO
#define FASTLED_RESTORE_ARDUINO
#endif

#define FASTLED_JSON_GUARD

#include "json.hpp"

#undef FASTLED_JSON_GUARD

#if defined(FASTLED_RESTORE_ARDUINO)
#pragma pop_macro("ARDUINO")
#undef FASTLED_RESTORE_ARDUINO
#endif
