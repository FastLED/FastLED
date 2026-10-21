// Arduino-compatible Serial API for FastLED
// Provides a familiar interface for serial I/O across all platforms

#pragma once

#include "fl/stl/has_include.h"  // IWYU pragma: keep

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstdio.h"  // for fl::serial_begin
#include "fl/stl/stdio.h"  // for fl::snprintf
#include "fl/stl/type_traits.h"  // for enable_if and is_multi_byte_integer
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Arduino-compatible Serial class for cross-platform serial I/O
 *
 * This class provides the familiar Arduino Serial API for all FastLED platforms.
 * It delegates to platform-specific implementations under the hood.
 *
 * USAGE:
 * @code
 * fl::Serial.begin(115200);
 * fl::Serial.println("Hello, FastLED!");
 *
 * if (fl::Serial.available()) {
 *     char c = fl::Serial.read();
 *     fl::Serial.print("Received: ");
 *     fl::Serial.println(c);
 * }
 * @endcode
 *
 * COMPATIBILITY:
 * - Compatible with Arduino Serial API
 * - Works on all FastLED platforms (AVR, ESP32, STM32, host, etc.)
 * - Zero overhead - compiles to direct platform calls
 */
class SerialPort {
public:
    /**
     * @brief Initialize serial communication
     * @param baudRate Baud rate (e.g., 9600, 115200)
     *
     * Note: On some platforms (host), baud rate is ignored.
     * On embedded platforms, this configures the UART hardware.
     *
     * Inline so that, on AVR, the strong reference to Arduino's `Serial`
     * made by fl::serial_begin() lands in the caller's object (#4725).
     */
    void begin(u32 baudRate = 115200) FL_NO_EXCEPT { fl::serial_begin(baudRate); }

    /**
     * @brief Close serial communication
     *
     * Note: On many platforms, this is a no-op as serial is always available.
     */
    void end() FL_NO_EXCEPT;

    /**
     * @brief Check how many bytes are available to read
     * @return Number of bytes available in receive buffer
     */
    int available() FL_NO_EXCEPT;

    /**
     * @brief Read next byte from serial input
     * @return Byte value (0-255) if available, -1 if no data
     *
     * This is a non-blocking read. If no data is available, returns -1.
     */
    int read() FL_NO_EXCEPT;

    /**
     * @brief Peek at next byte without removing it from buffer
     * @return Byte value (0-255) if available, -1 if no data
     *
     * Note: Not all platforms support peek(). May return -1 always.
     */
    int peek() FL_NO_EXCEPT;

    /**
     * @brief Write single byte to serial output
     * @param byte Byte to write
     * @return Number of bytes written (1 on success, 0 on failure)
     */
    size_t write(u8 byte) FL_NO_EXCEPT;

    /**
     * @brief Write buffer to serial output
     * @param buffer Pointer to data buffer
     * @param size Number of bytes to write
     * @return Number of bytes written
     */
    size_t write(const u8* buffer, size_t size) FL_NO_EXCEPT;

    /**
     * @brief Print string to serial output
     * @param str Null-terminated string
     * @return Number of bytes written
     */
    size_t print(const char* str) FL_NO_EXCEPT;

    /**
     * @brief Print integer to serial output
     * @param value Integer value to print
     * @return Number of bytes written
     */
    size_t print(int value) FL_NO_EXCEPT;

    /**
     * @brief Print long integer to serial output
     * @param value Long integer value to print
     * @return Number of bytes written
     */
    size_t print(long value) FL_NO_EXCEPT;

    /**
     * @brief Print generic unsigned integer via SFINAE template
     * Handles u32, unsigned long, and other unsigned multi-byte integers
     * Uses enable_if to avoid duplicate overload issues on platforms where
     * unsigned long == u32
     */
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value &&
                          !fl::is_signed<T>::value,
                          size_t>::type
    print(T value) FL_NO_EXCEPT;

    /**
     * @brief Print string with newline to serial output
     * @param str Null-terminated string
     * @return Number of bytes written
     */
    size_t println(const char* str) FL_NO_EXCEPT;

    /**
     * @brief Print newline only
     * @return Number of bytes written
     */
    size_t println() FL_NO_EXCEPT;

    /**
     * @brief Print integer with newline to serial output
     * @param value Integer value to print
     * @return Number of bytes written
     */
    size_t println(int value) FL_NO_EXCEPT;

    /**
     * @brief Print long integer with newline to serial output
     * @param value Long integer value to print
     * @return Number of bytes written
     */
    size_t println(long value) FL_NO_EXCEPT;

    /**
     * @brief Print generic unsigned integer with newline via SFINAE template
     * Handles u32, unsigned long, and other unsigned multi-byte integers
     * Uses enable_if to avoid duplicate overload issues on platforms where
     * unsigned long == u32
     */
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value &&
                          !fl::is_signed<T>::value,
                          size_t>::type
    println(T value) FL_NO_EXCEPT;

    /**
     * @brief Print formatted string to serial output (printf-style)
     * @tparam Args Variadic template arguments
     * @param format Printf-style format string
     * @param args Variable arguments
     * @return Number of bytes written
     *
     * Example:
     * @code
     * fl::Serial.printf("Value: %d, Hex: 0x%X\n", 42, 255);
     * @endcode
     *
     * Note: Maximum formatted string length is 256 characters.
     */
    template<typename... Args>
    size_t printf(const char* format, Args... args) FL_NO_EXCEPT;  // ok snprintf — method routes through fl::snprintf at line 284

    /**
     * @brief Wait for serial output to complete
     * @param timeoutMs Maximum time to wait in milliseconds (default: 1000ms)
     * @return true if flush completed, false if timeout
     *
     * Blocks until all buffered data is transmitted.
     * Note: On platforms without buffering, this returns immediately.
     */
    bool flush(u32 timeoutMs = 1000) FL_NO_EXCEPT;

    /**
     * @brief Check if serial port is ready for I/O
     * @return true if serial is initialized and ready
     *
     * On most platforms, this always returns true.
     */
    explicit operator bool() const FL_NO_EXCEPT;

    /**
     * @brief Set timeout for read operations
     * @param timeoutMs Timeout in milliseconds (default: 1000ms)
     *
     * This affects readString(), readStringUntil(), readBytes(), and readBytesUntil().
     */
    void setTimeout(u32 timeoutMs) FL_NO_EXCEPT;

    /**
     * @brief Read all available bytes into a string
     * @return String containing all available data (up to timeout)
     *
     * Reads characters from serial until no more data is available or timeout expires.
     */
    fl::string readString() FL_NO_EXCEPT;

    /**
     * @brief Read characters until delimiter is found
     * @param delimiter Character to stop reading at (not included in result)
     * @return String containing all characters up to delimiter
     *
     * Reads characters from serial until delimiter is found or timeout expires.
     * The delimiter character is discarded (not included in the returned string).
     */
    fl::string readStringUntil(char delimiter) FL_NO_EXCEPT;

    /**
     * @brief Read fixed number of bytes into buffer
     * @param buffer Pointer to destination buffer
     * @param length Number of bytes to read
     * @return Number of bytes actually read
     *
     * Reads up to length bytes from serial. May return fewer bytes if timeout occurs.
     */
    size_t readBytes(u8* buffer, size_t length) FL_NO_EXCEPT;

    /**
     * @brief Read bytes until delimiter is found
     * @param buffer Pointer to destination buffer
     * @param length Maximum number of bytes to read
     * @param delimiter Character to stop reading at (not included in result)
     * @return Number of bytes actually read (not including delimiter)
     *
     * Reads bytes from serial until delimiter is found, timeout occurs, or buffer is full.
     * The delimiter character is discarded (not included in the buffer).
     */
    size_t readBytesUntil(char delimiter, u8* buffer, size_t length) FL_NO_EXCEPT;

    /**
     * @brief Parse integer from serial input
     * @return Parsed integer value, or 0 if no valid integer found
     *
     * Skips non-numeric characters until a number is found, then parses it.
     * Stops at first non-numeric character after the number.
     */
    long parseInt() FL_NO_EXCEPT;

    /**
     * @brief Parse floating-point number from serial input
     * @return Parsed float value, or 0.0 if no valid float found
     *
     * Skips non-numeric characters until a number is found, then parses it.
     * Stops at first non-numeric character after the number.
     */
    float parseFloat() FL_NO_EXCEPT;

private:
    u32 mTimeoutMs = 1000;  // Default 1 second timeout
};

// ============================================================================
// Template Implementations
// ============================================================================



template<typename... Args>
inline size_t SerialPort::printf(const char* format, Args... args) FL_NO_EXCEPT {
    if (!format) {
        return 0;
    }

    // Format into a fixed-size buffer using fl::snprintf
    char buffer[256];  // Maximum formatted string length
    int len = fl::snprintf(buffer, sizeof(buffer), format, args...);

    if (len < 0) {
        return 0;  // Formatting error
    }

    // Delegate to print() method
    return print(buffer);
}

// Global Serial object (Arduino-compatible)
// FL_MAYBE_UNUSED allows compiler to elide if not used
extern FL_MAYBE_UNUSED SerialPort fl_serial;

} // namespace fl

// Define Serial as fl::fl_serial ONLY when Arduino is not available
// When Arduino.h exists, Serial remains the Arduino Serial object
// When Arduino.h doesn't exist, Serial is defined as fl::fl_serial for convenience
#if !FL_HAS_INCLUDE(<Arduino.h>)
#define Serial fl::fl_serial
#endif
