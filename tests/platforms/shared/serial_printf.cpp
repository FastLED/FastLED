// ok standalone
// Test for Serial.printf functionality on stub platform
#include "platforms/stub/Arduino.h"
#include "test.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/string.h"
#include "fl/system/serial.h"

FL_TEST_CASE("Serial.printf basic") {
    Serial.begin(9600);

    FL_SUBCASE("basic printf functionality") {
        // Test basic printf functionality - these should compile and run without crashing
        Serial.printf("Basic test\n");
        Serial.printf("Integer: %d\n", 42);
        Serial.printf("String: %s\n", "hello");
        Serial.printf("Float: %.2f\n", 3.14159);
        FL_CHECK(true); // If we get here, printf works
    }

    FL_SUBCASE("mixed argument types") {
        // Test mixed argument types
        Serial.printf("Mixed: %d, %.1f, %s\n", 100, 2.5, "world");
        Serial.printf("Multiple ints: %d %d %d\n", 1, 2, 3);
        FL_CHECK(true);
    }

    FL_SUBCASE("floating point precision") {
        // Test floating point precision
        Serial.printf("Pi: %.0f\n", 3.14159);  // Should print 3
        Serial.printf("Pi: %.1f\n", 3.14159);  // Should print 3.1
        Serial.printf("Pi: %.2f\n", 3.14159);  // Should print 3.14
        Serial.printf("Pi: %.3f\n", 3.14159);  // Should print 3.141
        FL_CHECK(true);
    }

    FL_SUBCASE("hex formatting") {
        // Test hex formatting
        Serial.printf("Hex: %x\n", 255);      // Should print ff
        Serial.printf("Hex: %X\n", 255);      // Should print FF
        FL_CHECK(true);
    }

    FL_SUBCASE("character formatting") {
        // Test character formatting
        Serial.printf("Char: %c\n", 'A');
        Serial.printf("Char: %c%c%c\n", 'F', 'o', 'o');
        FL_CHECK(true);
    }

    FL_SUBCASE("unsigned formatting") {
        // Test unsigned formatting
        Serial.printf("Unsigned: %u\n", 42u);
        Serial.printf("Unsigned: %u\n", 0u);
        FL_CHECK(true);
    }

    FL_SUBCASE("percent escape") {
        // Test percent escape
        Serial.printf("Percent: 100%%\n");
        FL_CHECK(true);
    }
}

// fl::SerialPort::printf formats out of line (FastLED#5307); the output must
// stay what the former 256-byte snprintf() buffer produced: the text up to
// the first NUL, clipped to kPrintfMaxLen characters.
FL_TEST_CASE("Serial.printf captured output") {
    const size_t kMax = fl::SerialPort::kPrintfMaxLen;
    fl::string captured;
    fl::inject_print_handler([&](const char* s) { captured += s; });

    FL_SUBCASE("formatted text and length") {
        const size_t n = fl::fl_serial.printf("Integer: %d, %s", 42, "ok");
        FL_CHECK_EQ(captured, fl::string("Integer: 42, ok"));
        FL_CHECK_EQ(n, size_t(15));
    }

    FL_SUBCASE("no arguments") {
        const size_t n = fl::fl_serial.printf("100%% {{x}} %d");
        FL_CHECK_EQ(captured, fl::string("100% {x} <missing_arg>"));
        FL_CHECK_EQ(n, captured.size());
    }

    FL_SUBCASE("exactly the limit is not clipped") {
        const fl::string text(kMax, 'x');
        const size_t n = fl::fl_serial.printf("%s", text.c_str());
        FL_CHECK_EQ(captured, text);
        FL_CHECK_EQ(n, kMax);
    }

    FL_SUBCASE("longer output is clipped") {
        const fl::string text(300, 'y');
        const size_t n = fl::fl_serial.printf("ab%s", text.c_str());
        FL_CHECK_EQ(captured.size(), kMax);
        FL_CHECK_EQ(n, kMax);
        FL_CHECK_EQ(captured.substr(0, 3), fl::string("aby"));
    }

    FL_SUBCASE("null format prints nothing") {
        const char* format = nullptr;
        FL_CHECK_EQ(fl::fl_serial.printf(format), size_t(0));
        FL_CHECK_EQ(captured.size(), size_t(0));
    }

    fl::clear_io_handlers();
}
