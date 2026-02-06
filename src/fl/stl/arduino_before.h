// Arduino compatibility: Include Arduino.h and handle macro conflicts
// This is the ONLY file in fl/** that should directly include <Arduino.h>
// All other files should include this header instead.
//
// This file is designed to be idempotent - multiple includes are safe.
// Each macro is only pushed once (first include), and popped once (last include).

#pragma once

#include "fl/has_include.h"

// Include Arduino.h if available
#if FL_HAS_INCLUDE(<Arduino.h>)
#include <Arduino.h>
#if defined(Serial)
#define FL_ARDUINO_SERIAL_NEEDS_SAVE 1
#else
#define FL_ARDUINO_SERIAL_NEEDS_SAVE 0
#endif

#if defined(String)
#define FL_ARDUINO_STRING_NEEDS_SAVE 1
#else
#define FL_ARDUINO_STRING_NEEDS_SAVE 0
#endif
#endif

// Save Serial object and push macro (master guard: FL_ARDUINO_SERIAL_NEEDS_SAVE)
#if FL_ARDUINO_SERIAL_NEEDS_SAVE
    // Save the actual Serial object as C++ reference before pushing macro
    namespace fl { namespace detail {
        static inline auto& arduino_serial_save = Serial;
    }}
    #define ArduinoSerial_Save (::fl::detail::arduino_serial_save)

    // Push Serial macro to remove it from namespace
    #pragma push_macro("Serial")
    #undef Serial
#else
    // Serial is not available - use fl::fl_serial instead
    #define ArduinoSerial_Save Serial  // ArduinoSerial_Save is just Serial
#endif

// Save String class and push macro (master guard: FL_ARDUINO_STRING_NEEDS_SAVE)
#if FL_ARDUINO_STRING_NEEDS_SAVE
    // Save the actual String class as type alias before pushing macro
    namespace fl { namespace detail {
        using arduino_string_save = String;
    }}
    using ArduinoString_Save = ::fl::detail::arduino_string_save;

    // Push String macro to remove it from namespace
    #pragma push_macro("String")
    #undef String
#else
    // String is not available - no type alias needed
    // using ArduinoString_Save = void;  // Placeholder type
    #define ArduinoString_Save String  // ArduinoString_Save is just String
#endif

// Additional Arduino macros can be added here as needed
// Pattern: Check macro exists AND hasn't been processed (FL_<MACRO>_NEEDS_SAVE)
// Example:
// #if defined(Wire) && !defined(FL_WIRE_NEEDS_SAVE)
//     #pragma push_macro("Wire")
//     #undef Wire
//     #define FL_WIRE_NEEDS_SAVE
// #endif
