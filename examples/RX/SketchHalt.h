#pragma once

#include "fl/stl/ostream.h"
#include "fl/stl/time.h"
#include "fl/delay.h"
#include "fl/stl/string.h"

/// @brief Sketch halting mechanism that prevents watchdog timer resets
///
/// Instead of using an infinite while(1) loop (which blocks loop() from returning
/// and triggers watchdog resets on ESP32-C6), this class uses a flag-based approach
/// that allows loop() to return normally.
///
/// Usage:
///   SketchHalt halt;
///
///   void loop() {
///       if (halt.check()) return;  // MUST be first line
///       // ... rest of loop code
///       if (test_failed) {
///           halt.error("Test failed");
///           return;
///       }
///   }
class SketchHalt {
public:
    /// @brief Check if sketch is halted and handle periodic message printing
    /// @return true if halted (caller should return from loop immediately)
    ///
    /// This should be the FIRST line in loop():
    ///   if (halt.check()) return;
    bool check() {
        if (!mHalted) {
            return false;
        }

        fl::u32 current_time = fl::millis();
        if (mLastPrintTime == 0 || (current_time - mLastPrintTime) >= 5000) {
            fl::cout << "ERROR: HALT: " << mMessage.c_str() << "\n";
            mLastPrintTime = current_time;
        }
        fl::delayMillis(100);
        return true;
    }

    /// @brief Halt sketch execution with an error message
    void error(const char* message) {
        mHalted = true;
        mMessage = message;
        fl::cout << "ERROR: HALT: " << mMessage.c_str() << "\n";
        mLastPrintTime = fl::millis();
    }

private:
    bool mHalted = false;
    fl::string mMessage;
    fl::u32 mLastPrintTime = 0;
};
