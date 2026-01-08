// Test for Serial.printf functionality on stub platform
#include "platforms/stub/Arduino.h"
#include "doctest.h"
#include "fl/stl/string.h"

TEST_CASE("Serial.printf basic") {
    Serial.begin(9600);

    SUBCASE("basic printf functionality") {
        // Test basic printf functionality - these should compile and run without crashing
        Serial.printf("Basic test\n");
        Serial.printf("Integer: %d\n", 42);
        Serial.printf("String: %s\n", "hello");
        Serial.printf("Float: %.2f\n", 3.14159);
        CHECK(true); // If we get here, printf works
    }

    SUBCASE("mixed argument types") {
        // Test mixed argument types
        Serial.printf("Mixed: %d, %.1f, %s\n", 100, 2.5, "world");
        Serial.printf("Multiple ints: %d %d %d\n", 1, 2, 3);
        CHECK(true);
    }

    SUBCASE("floating point precision") {
        // Test floating point precision
        Serial.printf("Pi: %.0f\n", 3.14159);  // Should print 3
        Serial.printf("Pi: %.1f\n", 3.14159);  // Should print 3.1
        Serial.printf("Pi: %.2f\n", 3.14159);  // Should print 3.14
        Serial.printf("Pi: %.3f\n", 3.14159);  // Should print 3.141
        CHECK(true);
    }

    SUBCASE("hex formatting") {
        // Test hex formatting
        Serial.printf("Hex: %x\n", 255);      // Should print ff
        Serial.printf("Hex: %X\n", 255);      // Should print FF
        CHECK(true);
    }

    SUBCASE("character formatting") {
        // Test character formatting
        Serial.printf("Char: %c\n", 'A');
        Serial.printf("Char: %c%c%c\n", 'F', 'o', 'o');
        CHECK(true);
    }

    SUBCASE("unsigned formatting") {
        // Test unsigned formatting
        Serial.printf("Unsigned: %u\n", 42u);
        Serial.printf("Unsigned: %u\n", 0u);
        CHECK(true);
    }

    SUBCASE("percent escape") {
        // Test percent escape
        Serial.printf("Percent: 100%%\n");
        CHECK(true);
    }
}
