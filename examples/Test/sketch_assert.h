/// @file    sketch_assert.h
/// @brief   Assertion system for Arduino sketches with callback-based error handling

#pragma once

#include <Arduino.h>
#include <FastLED.h>

// Maximum number of errors to collect before stopping
#define MAX_ERRORS 10

// Error information structure
struct Error {
    fl::string source_file;
    int32_t line;
    fl::string message;

    Error() : line(0) {}
    Error(const char* file, int32_t l, const char* msg)
        : source_file(file), line(l), message(msg) {}
};

// Global error collection
extern fl::vector_inlined<Error, MAX_ERRORS> g_errors;

// Error callback type
extern fl::function<void()> g_error_callback;

// Set the error callback to be invoked on assertion failure
void setAssertCallback(fl::function<void()> callback);

// Trigger the error callback (called internally by AssertHelper)
void triggerErrorCallback();

// Check if we can still add errors (haven't reached MAX_ERRORS)
bool canAddError();

// Helper class for assertion messages - outputs in destructor if failed
class AssertHelper {
public:
    AssertHelper(bool failed, const char* file, int line, uint8_t actual, uint8_t expected);
    ~AssertHelper();

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
    const char* mFile;
    int mLine;
    uint8_t mActual;
    uint8_t mExpected;
    fl::StrStream mMsg;
};

// ASSERT_EQ macro with optional message support via << operator
// Uses nested for-loops for C++11 compatibility (no init-statement in if)
// Now continues testing even after errors (up to MAX_ERRORS)
#define ASSERT_EQ(actual, expected) \
    for (bool _assert_guard = false; !_assert_guard; _assert_guard = true) \
        for (AssertHelper _assert_helper = AssertHelper( \
                (actual) != (expected), \
                __FILE__, \
                __LINE__, \
                (actual), \
                (expected)); \
             !_assert_guard; _assert_guard = true) \
            if (_assert_helper.failed()) { \
                if (!canAddError()) return false; \
            } else \
                (void)_assert_helper
