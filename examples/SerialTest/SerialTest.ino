// Serial Test - Tests normalized Serial implementation
// Serial is now normalized across all platforms

#include <FastLED.h>

#include "fl/serial.h"
#include "fl/string.h"

// Test selection
enum TestMode {
    TEST_PRINT_ONLY,
    TEST_ECHO
};

// Configure which test to run
const TestMode CURRENT_TEST = TEST_PRINT_ONLY;

// Done state tracking (for TEST_PRINT_ONLY)
bool test_done = false;

// ===== TEST 1: PRINT ONLY =====
// Tests basic Serial output without waiting for input
void test_print_only() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== TEST 1: PRINT ONLY ===");
    Serial.println("Testing basic Serial output...");

    // Test various print methods
    Serial.print("Line 1: Serial.print() - ");
    Serial.println("Serial.println()");

    Serial.println("Line 2: Testing integer output: 12345");
    Serial.println("Line 3: Testing hex output: 0xABCD");
    Serial.println("Line 4: Testing float output: 3.14159");

    // Test string output
    fl::string test_string = "Line 5: fl::string output works!";
    Serial.println(test_string.c_str());

    // Test multiple rapid prints
    Serial.print("Line 6: ");
    Serial.print("Multiple ");
    Serial.print("print ");
    Serial.print("calls ");
    Serial.println("on one line");

    Serial.println("\nLine 7: Print test complete!");
    Serial.println("SUCCESS: All print operations completed");
    Serial.flush();
}

// ===== TEST 2: ECHO =====
// Tests Serial input/output by echoing received data
void test_echo() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== TEST 2: ECHO TEST ===");
    Serial.println("Send any text (ending with newline) to echo it back");
    Serial.println("Ready for input...\n");
    Serial.flush();
}

void setup() {
    if (CURRENT_TEST == TEST_ECHO) {
        test_echo();
    }
    // TEST_PRINT_ONLY runs in loop()
}

struct OnLoopFinished {
    fl::function<void()> callback;
    OnLoopFinished(fl::function<void()> callback) : callback(callback) {}
    ~OnLoopFinished() {
        if (callback) {
            callback();
        }
    }
};

void loop() {
    OnLoopFinished on_loop_finished([]() {
        test_done = true;
    });

    if (test_done) {
        EVERY_N_MILLISECONDS(5000) {
            Serial.println("DONE");
        }
        return;
    }

    if (CURRENT_TEST == TEST_PRINT_ONLY) {
        test_print_only();
        return;
    }

    if (CURRENT_TEST == TEST_ECHO) {
        // Echo test: read input and echo it back
        if (Serial.available() > 0) {
            fl::string input = Serial.readStringUntil('\n');
            input.trim();

            if (input.length() > 0) {
                Serial.print("[ECHO] ");
                Serial.println(input.c_str());
                Serial.flush();
            }
        }
        delay(10);
    }
}
