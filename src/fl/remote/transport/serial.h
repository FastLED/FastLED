// src/fl/remote/transport/serial.h
// Cross-platform serial transport layer for JSON-RPC
// Provides factory functions for creating RequestSource and ResponseSink callbacks

#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/delay.h"
#include "fl/int.h"
#include "fl/log.h"
#include "fl/stl/cctype.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/pair.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/string_view.h"

namespace fl {

// =============================================================================
// Core Serialization Functions (Pure, No I/O)
// =============================================================================
// These functions have no I/O dependencies and are easily unit-testable

/// @brief Serialize JSON response to a string
/// @param response JSON response object
/// @param prefix Optional prefix to prepend (default: "")
/// @return Formatted string ready to be written to output
/// @note Generic JSON serialization - works for any JSON, not just JSON-RPC
fl::string formatJsonResponse(const fl::Json& response, const char* prefix = "");

// =============================================================================
// Generic I/O Functions (Templated for Testability)
// =============================================================================

/// @brief Read a line from a serial-like input source (blocking with optional timeout)
/// @tparam SerialIn Type providing available() and read() methods
/// @param serial Serial input source
/// @param delimiter Line delimiter character (default: '\n')
/// @param timeoutMs Optional timeout in milliseconds (nullopt = wait forever)
/// @return Optional string containing the line (without delimiter), or nullopt if timeout
/// @note Follows Arduino's readStringUntil() style - blocks until complete line or timeout
template<typename SerialIn>
fl::optional<fl::string> readSerialLine(SerialIn& serial, char delimiter = '\n', fl::optional<u32> timeoutMs = fl::nullopt);

/// @brief Write a string with newline to a serial-like output
/// @tparam SerialOut Type providing println() method
/// @param serial Serial output destination
/// @param str String to write
template<typename SerialOut>
void writeSerialLine(SerialOut& serial, const fl::string& str);

// =============================================================================
// Serial Adapters (Using fl:: Functions)
// =============================================================================

/// @brief Serial adapter using fl:: input functions (fl::available, fl::read)
/// @note Works across all FastLED platforms (AVR, ESP32, STM32, host, etc.)
struct SerialReader {
    int available() const { return fl::available(); }
    int read() { return fl::read(); }
};

/// @brief Optimized readSerialLine for fl:: serial input
/// @param serial SerialReader adapter (unused, for API consistency)
/// @param delimiter Line delimiter character (default: '\n')
/// @param timeoutMs Optional timeout in milliseconds (nullopt = wait forever)
/// @return Optional string containing the line (without delimiter), or nullopt if timeout
/// @note Delegates to normalized fl::readLine() API with default skipChar='\r'
inline fl::optional<fl::string> readSerialLine(SerialReader& serial, char delimiter = '\n', fl::optional<u32> timeoutMs = fl::nullopt) {
    (void)serial;  // Unused, we call fl::readLine() directly
    return fl::readLine(delimiter, '\r', timeoutMs);
}

/// @brief Serial adapter using fl:: output functions (fl::println)
/// @note Works across all FastLED platforms
struct SerialWriter {
    void println(const char* str) { fl::println(str); }
};

// =============================================================================
// Remote Callback Factories (Creates RequestSource and ResponseSink)
// =============================================================================

/// @brief Create a JSON-RPC RequestSource that reads from fl:: serial input
/// @param prefix Optional prefix to strip from input lines (default: "")
/// @return RequestSource callback suitable for fl::Remote constructor
/// @note Composes transport layer (serial I/O) with protocol layer (JSON-RPC normalization)
///
/// Example:
/// @code
/// auto requestSource = fl::createSerialRequestSource();
/// auto responseSink = fl::createSerialResponseSink("REMOTE: ");
/// fl::Remote remote(requestSource, responseSink);
/// @endcode
inline fl::function<fl::optional<fl::Json>()>
createSerialRequestSource(const char* prefix = "") {
    return [prefix]() -> fl::optional<fl::Json> {
        // Transport layer: Read line from serial
        SerialReader serial;
        // Use 1ms timeout for non-blocking reads (async task runs every 10ms)
        auto line = readSerialLine(serial, '\n', 1);
        if (!line.has_value()) {
            return fl::nullopt;
        }

        // Use string_view for zero-copy prefix stripping and trimming
        fl::string_view view = line.value();

        // Strip prefix if present
        if (prefix && prefix[0] != '\0') {
            if (view.starts_with(prefix)) {
                view.remove_prefix(fl::strlen(prefix));
            }
        }

        // Trim leading whitespace
        while (!view.empty() && fl::isspace(view.front())) {
            view.remove_prefix(1);
        }

        // Trim trailing whitespace
        while (!view.empty() && fl::isspace(view.back())) {
            view.remove_suffix(1);
        }

        // Only parse if input starts with '{'
        if (view.empty() || view[0] != '{') {
            return fl::nullopt;
        }

        // Single copy when parsing JSON (unavoidable - JSON needs owned string)
        fl::string input(view);
        return fl::Json::parse(input);
    };
}

/// @brief Create a JSON-RPC ResponseSink that writes to fl:: serial output
/// @param prefix Optional prefix to prepend to responses (default: "REMOTE: ")
/// @return ResponseSink callback suitable for fl::Remote constructor
/// @note Composes protocol layer (schema filtering) with transport layer (serial I/O)
inline fl::function<void(const fl::Json&)>
createSerialResponseSink(const char* prefix = "REMOTE: ") {
    return [prefix](const fl::Json& response) {
        // Format and write to serial (no filtering needed - protocol uses flat structure)
        SerialWriter serial;
        fl::string formatted = formatJsonResponse(response, prefix);
        writeSerialLine(serial, formatted);
    };
}

/// @brief Create RequestSource and ResponseSink pair for serial I/O
/// @param responsePrefix Prefix for outgoing responses (default: "REMOTE: ")
/// @param requestPrefix Prefix to strip from incoming requests (default: "")
/// @return Pair of {RequestSource, ResponseSink} ready for fl::Remote
///
/// Example:
/// @code
/// auto [source, sink] = fl::transport::createSerialTransport();
/// fl::Remote remote(source, sink);
/// @endcode
inline fl::pair<fl::function<fl::optional<fl::Json>()>, fl::function<void(const fl::Json&)>>
createSerialTransport(const char* responsePrefix = "REMOTE: ", const char* requestPrefix = "") {
    return {createSerialRequestSource(requestPrefix), createSerialResponseSink(responsePrefix)};
}

// =============================================================================
// Template Implementations
// =============================================================================

template<typename SerialIn>
fl::optional<fl::string> readSerialLine(SerialIn& serial, char delimiter, fl::optional<u32> timeoutMs) {
    // Delegate to readSerialStringUntil with default skipChar='\r'
    return readSerialStringUntil(serial, delimiter, '\r', timeoutMs);
}

template<typename SerialIn>
fl::optional<fl::string> readSerialStringUntil(SerialIn& serial, char delimiter, char skipChar, fl::optional<u32> timeoutMs) {
    // Follows Arduino Serial.readStringUntil() API - blocks until delimiter found
    fl::sstream buffer;

    u32 startTime = fl::millis();

    // Read characters until we find delimiter or timeout
    while (true) {
        // Check timeout (only if timeout is set)
        if (timeoutMs.has_value()) {
            if (fl::millis() - startTime >= timeoutMs.value()) {
                // Timeout occurred - return nullopt
                return fl::nullopt;
            }
        }

        // Try to read next character
        int c = serial.read();

        // Handle -1 (no data available) like Arduino's timedRead():
        // Keep trying until timeout (or forever if no timeout set)
        if (c == -1) {
            // Yield briefly to prevent busy loop
            fl::delay(1, false);
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

        // Valid character - add to buffer
        buffer << static_cast<char>(c);
    }

    // Convert to string and trim whitespace
    fl::string result = buffer.str();
    result.trim();
    return result;
}

template<typename SerialOut>
void writeSerialLine(SerialOut& serial, const fl::string& str) {
    serial.println(str.c_str());
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
