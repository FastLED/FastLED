// ok no header - public declarations remain in fl/stl/cstdio.h

#include "fl/stl/cstdio.h"
#include "fl/stl/chrono.h"
#include "fl/stl/strstream.h"
#include "platforms/io.h"
#include "fl/system/delay.h"

namespace fl {

bool readStringUntil(sstream& out, char delimiter, char skipChar, fl::optional<u32> timeoutMs) FL_NO_EXCEPT {
    // Follows Arduino Serial.readStringUntil() API - blocks until delimiter found
    u32 startTime = fl::millis();

    // Read characters until we find delimiter or timeout
    while (true) {
        // Check timeout (only if timeout is set)
        if (timeoutMs.has_value()) {
            if (fl::millis() - startTime >= timeoutMs.value()) {
                // Timeout occurred
                return false;
            }
        }

        // Try to read next character
        int c = read();

        // Handle -1 (no data available) like Arduino's timedRead():
        // Keep trying until timeout (or forever if no timeout set)
        if (c == -1) {
            // Brief 1us yield to prevent busy loop without the 1ms
            // minimum sleep that fl::delay(1) imposes on FreeRTOS
            // (vTaskDelay(1) = 1 tick = 1ms). The 1ms delay was too
            // slow for USB CDC multi-packet assembly (64-byte packets).
            fl::delayMicroseconds(1);
            continue;
        }

        // Found delimiter - complete
        if (c == delimiter) {
            break;
        }

        // Skip specified character (e.g., '\r' for cross-platform line endings)
        if (c == skipChar) {
            continue;
        }

        // Valid character - add to output stream
        out << static_cast<char>(c);
    }

    // Successfully read until delimiter
    return true;
}

fl::optional<fl::string> readLine(char delimiter, char skipChar, fl::optional<u32> timeoutMs) FL_NO_EXCEPT {
    // Try platform-native line reading first (e.g., Arduino's Serial.readStringUntil).
    // This is critical for USB CDC platforms (ESP32-C6/S3) where the native
    // implementation uses yield() (immediate context switch) instead of
    // delay(1) (1ms FreeRTOS sleep), enabling correct multi-packet assembly.
    char nativeBuf[512];
    int nativeLen = platforms::readLineNative(delimiter, nativeBuf, sizeof(nativeBuf));
    if (nativeLen >= 0) {
        fl::string result(nativeBuf, nativeLen);
        return fl::string(result.trim());
    }

    // Fallback: character-by-character reading for non-Arduino platforms
    sstream buffer;
    if (!readStringUntil(buffer, delimiter, skipChar, timeoutMs)) {
        return fl::nullopt;  // Timeout occurred
    }

    // Convert to string and trim whitespace (trim() returns a new fl::string)
    fl::string result = buffer.str();
    return fl::string(result.trim());
}

} // namespace fl
