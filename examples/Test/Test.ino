/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino

#include <Arduino.h>
#include <FastLED.h>

// Test configuration
#define NUM_LEDS 8
#define DATA_PIN 3

// Test array
CRGB leds[NUM_LEDS];

// Test tracking
static uint8_t g_test_idx = 0;
static uint8_t g_failed_tests = 0;
static uint8_t g_passed_tests = 0;

// Helper class for assertion messages - outputs in destructor if failed
class AssertHelper {
public:
    AssertHelper(bool failed, uint8_t line, uint8_t actual, uint8_t expected)
        : mFailed(failed), mLine(line), mActual(actual), mExpected(expected) {
    }

    ~AssertHelper() {
        if (mFailed) {
            Serial.print(F("FAIL L"));
            Serial.print(mLine);
            Serial.print(F(" exp="));
            Serial.print(mExpected);
            Serial.print(F(" got="));
            Serial.print(mActual);

            // Print optional message if any was streamed
            if (mMsg.str().length() > 0) {
                Serial.print(F(" - "));
                Serial.print(mMsg.c_str());
            }
            Serial.println();
            g_failed_tests++;
        }
    }

    // Stream operator for optional message
    template<typename T>
    AssertHelper& operator<<(const T& value) {
        if (mFailed) {
            mMsg << value;
        }
        return *this;
    }

    // Check if assertion failed
    bool failed() const { return mFailed; }

private:
    bool mFailed;
    uint8_t mLine;
    uint8_t mActual;
    uint8_t mExpected;
    fl::StrStream mMsg;
};

// ASSERT_EQ macro with optional message support via << operator
// Uses nested for-loops for C++11 compatibility (no init-statement in if)
#define ASSERT_EQ(actual, expected) \
    for (bool _assert_guard = false; !_assert_guard; _assert_guard = true) \
        for (AssertHelper _assert_helper = AssertHelper( \
                (actual) != (expected), \
                __LINE__ & 0xFF, \
                (actual), \
                (expected)); \
             !_assert_guard; _assert_guard = true) \
            if (_assert_helper.failed()) \
                return false; \
            else \
                _assert_helper

// Test functions return true on success, false on failure
bool test_math8_qadd() {
    // Test qadd8 (saturating addition)
    ASSERT_EQ(qadd8(100, 50), 150);
    ASSERT_EQ(qadd8(200, 100), 255) << "qadd8 should saturate at 255";
    ASSERT_EQ(qadd8(255, 1), 255) << "Already at max";
    ASSERT_EQ(qadd8(0, 0), 0);

    g_passed_tests++;
    return true;
}

bool test_math8_qsub() {
    // Test qsub8 (saturating subtraction)
    ASSERT_EQ(qsub8(100, 50), 50);
    ASSERT_EQ(qsub8(50, 100), 0);     // Should saturate at 0
    ASSERT_EQ(qsub8(0, 1), 0);        // Already at min
    ASSERT_EQ(qsub8(255, 255), 0);

    g_passed_tests++;
    return true;
}

bool test_math8_add() {
    // Test add8 (wrapping addition)
    ASSERT_EQ(add8(100, 50), 150);
    ASSERT_EQ(add8(200, 100), 44);    // Should wrap: (200+100) & 0xFF = 44
    ASSERT_EQ(add8(255, 1), 0);       // Wraps to 0

    g_passed_tests++;
    return true;
}

bool test_math8_sub() {
    // Test sub8 (wrapping subtraction)
    ASSERT_EQ(sub8(100, 50), 50);
    ASSERT_EQ(sub8(50, 100), 206);    // Should wrap: (50-100) & 0xFF = 206
    ASSERT_EQ(sub8(0, 1), 255);       // Wraps to 255

    g_passed_tests++;
    return true;
}

bool test_math8_scale() {
    // Test scale8 (8-bit scaling)
    ASSERT_EQ(scale8(255, 255), 255);
    ASSERT_EQ(scale8(255, 128), 128);
    ASSERT_EQ(scale8(128, 128), 64);
    ASSERT_EQ(scale8(0, 255), 0);
    ASSERT_EQ(scale8(255, 0), 0);

    g_passed_tests++;
    return true;
}

bool test_math8_avg() {
    // Test avg8 (average of two 8-bit values)
    ASSERT_EQ(avg8(100, 200), 150);
    ASSERT_EQ(avg8(0, 255), 127);     // (0+255)/2 rounded down
    ASSERT_EQ(avg8(128, 128), 128);

    g_passed_tests++;
    return true;
}

bool test_math8_blend() {
    // Test blend8 (linear interpolation)
    ASSERT_EQ(blend8(0, 255, 128), 127);    // 50% blend
    ASSERT_EQ(blend8(0, 255, 0), 0);        // 0% blend (all first value)
    ASSERT_EQ(blend8(0, 255, 255), 255);    // 100% blend (all second value)
    ASSERT_EQ(blend8(100, 200, 128), 150);  // 50% blend

    g_passed_tests++;
    return true;
}

bool test_array_fill() {
    // Test array fill operations
    fill_solid(leds, NUM_LEDS, CRGB::Red);

    ASSERT_EQ(leds[0].r, 255);
    ASSERT_EQ(leds[0].g, 0);
    ASSERT_EQ(leds[0].b, 0);

    g_passed_tests++;
    return true;
}

bool test_array_fade() {
    // Test fadeToBlackBy
    leds[0] = CRGB(200, 200, 200);
    fadeToBlackBy(leds, 1, 128);  // Reduce by ~50%

    // Value should be reduced to ~100
    ASSERT_EQ(leds[0].r, 100);

    g_passed_tests++;
    return true;
}

bool test_array_scale() {
    // Test nscale8
    leds[0] = CRGB(200, 200, 200);
    nscale8(&leds[0], 1, 128);  // Scale by 50%

    // Should be approximately halved
    ASSERT_EQ(leds[0].r, 100);

    g_passed_tests++;
    return true;
}

bool test_array_blur() {
    // Test blur1d
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[4] = CRGB(255, 0, 0);  // Bright red pixel in middle

    blur1d(leds, NUM_LEDS, 64);

    // Middle should still have most light, neighbor should have some
    // Just verify neighbor got some light (blur worked)
    if (leds[3].r == 0) {
        // Manual fail since we can't use ASSERT_EQ for comparison
        Serial.print(F("FAIL L"));
        Serial.print(__LINE__ & 0xFF);
        Serial.println(F(" - Blur did not spread light to neighbor"));
        g_failed_tests++;
        return false;
    }

    g_passed_tests++;
    return true;
}

// Test enumeration
#define TEST_MAX 11
enum TestIndex {
    TEST_MATH8_QADD = 0,
    TEST_MATH8_QSUB,
    TEST_MATH8_ADD,
    TEST_MATH8_SUB,
    TEST_MATH8_SCALE,
    TEST_MATH8_AVG,
    TEST_MATH8_BLEND,
    TEST_ARRAY_FILL,
    TEST_ARRAY_FADE,
    TEST_ARRAY_SCALE,
    TEST_ARRAY_BLUR
};

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port to connect (needed for some boards)
    }

    Serial.println("FastLED Runtime Test Suite");
    Serial.println("===========================");

    // Initialize FastLED
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}

void loop() {
    static bool tests_complete = false;

    if (tests_complete) {
        FL_WARN("All tests complete");
        delay(5000);
        return;
    }

    int idx = g_test_idx;
    g_test_idx = (g_test_idx + 1) % TEST_MAX;

    bool result = false;

    switch (idx) {
        case TEST_MATH8_QADD:
            Serial.print("Running test_math8_qadd... ");
            result = test_math8_qadd();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_QSUB:
            Serial.print("Running test_math8_qsub... ");
            result = test_math8_qsub();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_ADD:
            Serial.print("Running test_math8_add... ");
            result = test_math8_add();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_SUB:
            Serial.print("Running test_math8_sub... ");
            result = test_math8_sub();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_SCALE:
            Serial.print("Running test_math8_scale... ");
            result = test_math8_scale();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_AVG:
            Serial.print("Running test_math8_avg... ");
            result = test_math8_avg();
            if (result) Serial.println("PASS");
            break;

        case TEST_MATH8_BLEND:
            Serial.print("Running test_math8_blend... ");
            result = test_math8_blend();
            if (result) Serial.println("PASS");
            break;

        case TEST_ARRAY_FILL:
            Serial.print("Running test_array_fill... ");
            result = test_array_fill();
            if (result) Serial.println("PASS");
            break;

        case TEST_ARRAY_FADE:
            Serial.print("Running test_array_fade... ");
            result = test_array_fade();
            if (result) Serial.println("PASS");
            break;

        case TEST_ARRAY_SCALE:
            Serial.print("Running test_array_scale... ");
            result = test_array_scale();
            if (result) Serial.println("PASS");
            break;

        case TEST_ARRAY_BLUR:
            Serial.print("Running test_array_blur... ");
            result = test_array_blur();
            if (result) Serial.println("PASS");
            break;

        default:
            // All tests complete
            Serial.println("\n===========================");
            Serial.println("Test Summary:");
            Serial.print("Passed: ");
            Serial.println(g_passed_tests);
            Serial.print("Failed: ");
            Serial.println(g_failed_tests);

            if (g_failed_tests == 0) {
                Serial.println("\nAll tests PASSED!");
            } else {
                Serial.println("\nSome tests FAILED!");
            }

            tests_complete = true;
            break;
    }

    delay(100);  // Small delay between tests
}
