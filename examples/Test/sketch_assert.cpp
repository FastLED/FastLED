/// @file    sketch_assert.cpp
/// @brief   Implementation of assertion system for Arduino sketches

#include "sketch_assert.h"

// Global error collection
fl::vector_inlined<Error, MAX_ERRORS> g_errors;

// Error callback
fl::function<void()> g_error_callback;

void setAssertCallback(fl::function<void()> callback) {
    g_error_callback = callback;
}

void triggerErrorCallback() {
    if (g_error_callback) {
        g_error_callback();
    }
}

bool canAddError() {
    return g_errors.size() < MAX_ERRORS;
}

AssertHelper::AssertHelper(bool failed, const char* file, int line, uint8_t actual, uint8_t expected)
    : mFailed(failed), mFile(file), mLine(line), mActual(actual), mExpected(expected) {
}

AssertHelper::~AssertHelper() {
    if (mFailed && canAddError()) {
        // Build the error message
        fl::StrStream msg;
        msg << "FAIL L" << mLine << " exp=" << mExpected << " got=" << mActual;

        // Append optional message if any was streamed
        if (mMsg.str().length() > 0) {
            msg << " - " << mMsg.c_str();
        }

        // Add error to collection
        Error error(mFile, mLine, msg.c_str());
        g_errors.push_back(error);

        // Print to Serial immediately for debugging
        Serial.println(msg.c_str());

        // Trigger the error callback
        triggerErrorCallback();
    }
}
