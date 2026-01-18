// @filter: (platform is esp32)
/// @file TestRunner.h
/// @brief Test Runner for ESP32 Driver Testing
///
/// This header provides a simple test framework for validating LED drivers:
/// - DriverTestRunner: Main class that orchestrates all tests
/// - Automatic driver discovery and validation
/// - Visual LED test patterns for each driver
///
/// Usage:
///   DriverTestRunner runner(leds, NUM_LEDS);
///   runner.runAllTests();
///   runner.printSummary();

#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "fl/stl/sstream.h"
#include "PlatformConfig.h"

// ============================================================================
// BOX DRAWING CHARACTERS (for nice console output)
// ============================================================================

#define BOX_TOP    "\n+================================================================+\n"
#define BOX_MID    "+================================================================+\n"
#define BOX_BOTTOM "+================================================================+\n"
#define LINE_SEP   "----------------------------------------------------------------\n"

// ============================================================================
// TEST RESULT CLASS
// ============================================================================

/// @brief Stores the result of a single test
struct TestResult {
    const char* mTestName;
    bool mPassed;
    const char* mMessage;

    TestResult(const char* name, bool passed, const char* msg = "")
        : mTestName(name), mPassed(passed), mMessage(msg) {}
};

// ============================================================================
// DRIVER TEST RUNNER CLASS
// ============================================================================

/// @brief Main test runner that orchestrates all driver tests
///
/// Example usage:
/// @code
///   CRGB leds[NUM_LEDS];
///   FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
///
///   DriverTestRunner runner(leds, NUM_LEDS);
///   runner.runAllTests();
///   runner.printSummary();
/// @endcode
class DriverTestRunner {
public:
    /// @brief Construct a test runner
    /// @param leds Pointer to LED array
    /// @param numLeds Number of LEDs in the array
    DriverTestRunner(CRGB* leds, int numLeds)
        : mLeds(leds)
        , mNumLeds(numLeds)
        , mTotalTests(0)
        , mPassedTests(0)
        , mAllTestsPassed(true) {}

    /// @brief Run all tests: validation + driver tests
    void runAllTests() {
        printHeader();
        validateExpectedDrivers();
        testAllDrivers();
    }

    /// @brief Print final test summary with pass/fail status
    void printSummary() {
        fl::sstream ss;
        ss << BOX_TOP;
        if (mAllTestsPassed) {
            ss << "|         ALL TESTS PASSED                                       |\n";
        } else {
            ss << "|         SOME TESTS FAILED                                      |\n";
        }
        ss << BOX_MID;
        ss << "| Platform: " << getPlatformName() << "\n";
        ss << "| Tests:    " << mPassedTests << "/" << mTotalTests << " passed\n";
        ss << BOX_BOTTOM;
        Serial.print(ss.str().c_str());

        // Machine-readable output for automated testing
        if (mAllTestsPassed) {
            Serial.println("\nTEST_SUITE_COMPLETE: PASS");
        } else {
            Serial.println("\nTEST_SUITE_COMPLETE: FAIL");
        }
    }

    /// @brief Check if all tests passed
    /// @return true if all tests passed
    bool allPassed() const { return mAllTestsPassed; }

    /// @brief Get total number of tests run
    int getTotalTests() const { return mTotalTests; }

    /// @brief Get number of passed tests
    int getPassedTests() const { return mPassedTests; }

private:
    CRGB* mLeds;
    int mNumLeds;
    int mTotalTests;
    int mPassedTests;
    bool mAllTestsPassed;

    /// @brief Print test startup header
    void printHeader() {
        fl::sstream ss;
        ss << BOX_TOP;
        ss << "| ESP32 Generic Driver Test                                      |\n";
        ss << "| Tests all available LED channel drivers via Channel API        |\n";
        ss << BOX_BOTTOM;
        ss << "\nPlatform:  " << getPlatformName() << "\n";
        ss << "Data Pin:  " << DATA_PIN << "\n";
        ss << "LED Count: " << mNumLeds << "\n";
        Serial.print(ss.str().c_str());
    }

    /// @brief Record a test result
    /// @param name Test name
    /// @param passed Whether test passed
    void recordResult(const char* name, bool passed) {
        mTotalTests++;
        if (passed) {
            mPassedTests++;
            Serial.print("  [PASS] ");
        } else {
            mAllTestsPassed = false;
            Serial.print("  [FAIL] ");
        }
        Serial.println(name);
    }

    // ========================================================================
    // DRIVER VALIDATION
    // ========================================================================

    /// @brief Validate that all expected drivers are present
    void validateExpectedDrivers() {
        fl::vector<const char*> expected;
        getExpectedDrivers(expected);

        if (expected.empty()) {
            Serial.println("\n[WARNING] Unknown platform - skipping driver validation");
            return;
        }

        fl::sstream ss;
        ss << BOX_TOP;
        ss << "| DRIVER VALIDATION FOR " << getPlatformName() << "\n";
        ss << BOX_BOTTOM;
        Serial.print(ss.str().c_str());

        // Get available drivers from FastLED
        auto drivers = FastLED.getDriverInfos();

        // Print expected drivers
        ss.clear();
        ss << "\nExpected drivers (" << expected.size() << "):\n";
        for (fl::size i = 0; i < expected.size(); i++) {
            ss << "  - " << expected[i] << "\n";
        }
        Serial.print(ss.str().c_str());

        // Print available drivers with details
        ss.clear();
        ss << "\nAvailable drivers (" << drivers.size() << "):\n";
        for (fl::size i = 0; i < drivers.size(); i++) {
            ss << "  - " << drivers[i].name.c_str()
               << " (priority: " << drivers[i].priority
               << ", enabled: " << (drivers[i].enabled ? "yes" : "no") << ")\n";
        }
        Serial.print(ss.str().c_str());

        // Check each expected driver is present
        Serial.println("\nValidation results:");
        for (fl::size i = 0; i < expected.size(); i++) {
            const char* expName = expected[i];
            bool found = false;

            for (fl::size j = 0; j < drivers.size(); j++) {
                if (fl::strcmp(drivers[j].name.c_str(), expName) == 0) {
                    found = true;
                    break;
                }
            }

            fl::sstream resultMsg;
            resultMsg << expName << " driver " << (found ? "found" : "MISSING!");
            recordResult(resultMsg.str().c_str(), found);
        }
    }

    // ========================================================================
    // DRIVER TESTING
    // ========================================================================

    /// @brief Test all available drivers
    void testAllDrivers() {
        auto drivers = FastLED.getDriverInfos();

        fl::sstream ss;
        ss << BOX_TOP;
        ss << "| TESTING ALL AVAILABLE DRIVERS                                  |\n";
        ss << BOX_BOTTOM;
        Serial.print(ss.str().c_str());

        Serial.print("Found ");
        Serial.print(drivers.size());
        Serial.println(" driver(s) to test\n");

        int tested = 0;
        int skipped = 0;

        for (fl::size i = 0; i < drivers.size(); i++) {
            const char* name = drivers[i].name.c_str();
            if (drivers[i].name.empty()) {
                Serial.println("  [SKIP] Unnamed driver");
                skipped++;
                continue;
            }

            if (testSingleDriver(name)) {
                tested++;
            } else {
                skipped++;
            }

            delay(500);  // Brief pause between driver tests
        }

        Serial.print("\n");
        Serial.print(LINE_SEP);
        Serial.print("Driver tests complete: ");
        Serial.print(tested);
        Serial.print(" tested, ");
        Serial.print(skipped);
        Serial.println(" skipped");
    }

    /// @brief Test a single driver with LED patterns
    /// @param driverName Name of driver to test
    /// @return true if driver was tested successfully
    bool testSingleDriver(const char* driverName) {
        fl::sstream ss;
        ss << "\n" << LINE_SEP;
        ss << "Testing driver: " << driverName << "\n";
        ss << LINE_SEP;
        Serial.print(ss.str().c_str());

        // Attempt to set this driver as exclusive
        if (!FastLED.setExclusiveDriver(driverName)) {
            Serial.print("  [SKIP] Could not set ");
            Serial.print(driverName);
            Serial.println(" as exclusive driver (not available)");
            return false;
        }

        Serial.print("  [INFO] ");
        Serial.print(driverName);
        Serial.println(" set as exclusive driver");

        // Run visual test patterns
        runTestPatterns();

        // Record success
        fl::sstream resultMsg;
        resultMsg << driverName << " driver test completed";
        recordResult(resultMsg.str().c_str(), true);

        return true;
    }

    /// @brief Run visual LED test patterns
    void runTestPatterns() {
        // Clear first
        fill_solid(mLeds, mNumLeds, CRGB::Black);
        FastLED.show();
        delay(50);

        // Pattern 1: Rainbow gradient
        Serial.println("  [INFO] Sending rainbow pattern...");
        fill_rainbow(mLeds, mNumLeds, 0, 256 / mNumLeds);
        FastLED.show();
        delay(100);

        // Pattern 2: Solid red
        Serial.println("  [INFO] Sending solid red...");
        fill_solid(mLeds, mNumLeds, CRGB::Red);
        FastLED.show();
        delay(100);

        // Pattern 3: Solid green
        Serial.println("  [INFO] Sending solid green...");
        fill_solid(mLeds, mNumLeds, CRGB::Green);
        FastLED.show();
        delay(100);

        // Pattern 4: Solid blue
        Serial.println("  [INFO] Sending solid blue...");
        fill_solid(mLeds, mNumLeds, CRGB::Blue);
        FastLED.show();
        delay(100);

        // Clear for next test
        fill_solid(mLeds, mNumLeds, CRGB::Black);
        FastLED.show();
    }
};
