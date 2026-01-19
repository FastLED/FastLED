#pragma once

#define FTL_CSTDIO_H_INCLUDED

#include "fl/stl/stdint.h"

#ifdef FASTLED_TESTING
#include "fl/stl/function.h"
#endif

namespace fl {

// =============================================================================
// Global Log Level Control
// =============================================================================
// Runtime-configurable log level that can dynamically shut off logging.
// This affects all FL_PRINT, FL_DBG, FL_WARN, FL_INFO, FL_ERROR macros
// when they flow through fl::print/fl::println.

/// Log level constants - higher values include more output
enum LogLevel : uint8_t {
    LOG_LEVEL_NONE  = 0,  ///< No logging (completely silent)
    LOG_LEVEL_ERROR = 1,  ///< Only errors
    LOG_LEVEL_WARN  = 2,  ///< Errors and warnings
    LOG_LEVEL_INFO  = 3,  ///< Errors, warnings, and info
    LOG_LEVEL_DEBUG = 4,  ///< All logging including debug (default)
};

/// Get the current global log level
/// @return Current log level (0-4)
uint8_t getLogLevel();

/// Set the global log level
/// @param level New log level (use LogLevel enum values)
/// @note Setting to LOG_LEVEL_NONE disables all logging output
void setLogLevel(uint8_t level);

// =============================================================================
// RAII Scoped Log Control
// =============================================================================

/// @brief RAII class to temporarily disable all logging output
///
/// Creates a scope where all fl::print/fl::println output is suppressed.
/// When the object is destroyed, the previous log level is restored.
///
/// Usage:
/// @code
/// void runValidation() {
///     {
///         fl::ScopedLogDisable guard;  // Suppress logging
///         // ... validation code (FL_DBG, FL_WARN, fl::println suppressed) ...
///     }  // Logging restored when guard goes out of scope
///
///     // Or with fl::optional for manual control:
///     fl::optional<fl::ScopedLogDisable> guard;
///     guard.emplace();  // Disable logging
///     // ... work ...
///     guard.reset();    // Re-enable logging
///     Serial.println("JSON result");  // Raw Serial.print bypasses fl:: logging
/// }
/// @endcode
class ScopedLogDisable {
public:
    /// Constructor - saves current log level and disables logging
    ScopedLogDisable() : mPreviousLevel(getLogLevel()) {
        setLogLevel(LOG_LEVEL_NONE);
    }

    /// Destructor - restores previous log level
    ~ScopedLogDisable() {
        setLogLevel(mPreviousLevel);
    }

    // Non-copyable (prevent accidental copies that would corrupt state)
    ScopedLogDisable(const ScopedLogDisable&) = delete;
    ScopedLogDisable& operator=(const ScopedLogDisable&) = delete;

    // Move-only (allow transfer of ownership)
    ScopedLogDisable(ScopedLogDisable&& other) noexcept
        : mPreviousLevel(other.mPreviousLevel) {
        // Mark other as "moved-from" by setting to current level (no-op restore)
        other.mPreviousLevel = LOG_LEVEL_NONE;
    }
    ScopedLogDisable& operator=(ScopedLogDisable&&) = delete;

private:
    uint8_t mPreviousLevel;
};

// =============================================================================
// Low-Level Print Functions
// =============================================================================
// These functions avoid printf/sprintf dependencies and use the most efficient
// output method for each platform.

// Print a string without newline
void print(const char* str);

// Print a string with newline  
#ifndef FL_DBG_PRINTLN_DECLARED
void println(const char* str);
#else
// Declaration already exists from fl/dbg.h
#endif

// Low-level input functions that provide Serial-style read functionality
// These use the most efficient input method for each platform

// Returns the number of bytes available to read from input stream
// Returns 0 if no data is available
int available();

// Reads the next byte from input stream
// Returns the byte value (0-255) if data is available
// Returns -1 if no data is available
int read();

#ifdef FASTLED_TESTING

// Testing function handler types
using print_handler_t = fl::function<void(const char*)>;
using println_handler_t = fl::function<void(const char*)>;
using available_handler_t = fl::function<int()>;
using read_handler_t = fl::function<int()>;

// Inject function handlers for testing
void inject_print_handler(const print_handler_t& handler);
void inject_println_handler(const println_handler_t& handler);
void inject_available_handler(const available_handler_t& handler);
void inject_read_handler(const read_handler_t& handler);

// Clear all injected handlers (restores default behavior)
void clear_io_handlers();

// Clear individual handlers
void clear_print_handler();
void clear_println_handler();
void clear_available_handler();
void clear_read_handler();

#endif // FASTLED_TESTING

} // namespace fl
