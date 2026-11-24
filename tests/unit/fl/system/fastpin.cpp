#include "FastLED.h"
#include "test.h"

// Issue #893: NO_PIN was `#define NO_PIN 255`, so the preprocessor replaced
// that token everywhere and any other library using NO_PIN as an identifier
// failed to compile merely because FastLED.h had been included first.
//
// Everything below is a compile-time assertion first and a runtime one
// second: as a macro, none of these declarations parse at all -- each reads
// as `255` where a name belongs.

#ifdef NO_PIN
#error "NO_PIN must be a constant, not a macro -- see FastLED#893"
#endif

namespace other_library {
// The shape from the report: an enumerator named NO_PIN.
enum PinSentinel { NO_PIN = 7 };
}  // namespace other_library

namespace another_library {
// A class member, the other common shape.
struct Config {
    int NO_PIN = 3;
};
}  // namespace another_library

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("NO_PIN is a constant, so other libraries can reuse the name (#893)") {
    FL_CHECK_EQ(int(::NO_PIN), 255);
    FL_CHECK_EQ(int(other_library::NO_PIN), 7);
    FL_CHECK_EQ(another_library::Config().NO_PIN, 3);
}

FL_TEST_CASE("a local named NO_PIN shadows the global (#893)") {
    const int NO_PIN = 42;  // impossible while it was a macro
    FL_CHECK_EQ(NO_PIN, 42);
    FL_CHECK_EQ(int(::NO_PIN), 255);
}

} // FL_TEST_FILE
