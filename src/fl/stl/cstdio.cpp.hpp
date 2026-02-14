#include "cstdio.h"

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"
#include "fl/stl/strstream.h"  // For sstream

// Platform-specific I/O function declarations
// Each platform provides implementations in their .cpp.hpp files
#include "platforms/io.h"

// Forward declare delay to avoid Arduino conflict
namespace fl {
    void delay(u32 ms, bool run_async);
}

// =============================================================================
// Global Log Level Storage and API
// =============================================================================

namespace fl {

// Default log level is DEBUG (all logging enabled)
static u8 gLogLevel = LOG_LEVEL_DEBUG;

u8 getLogLevel() {
    return gLogLevel;
}

void setLogLevel(u8 level) {
    gLogLevel = level;
}

} // namespace fl

namespace fl {

// =============================================================================
// Print Functions
// =============================================================================

#ifdef FASTLED_TESTING
// Static storage for injected handlers using lazy initialization to avoid global constructors
static print_handler_t& get_print_handler() {
    static print_handler_t handler;
    return handler;
}

static println_handler_t& get_println_handler() {
    static println_handler_t handler;
    return handler;
}

static available_handler_t& get_available_handler() {
    static available_handler_t handler;
    return handler;
}

static read_handler_t& get_read_handler() {
    static read_handler_t handler;
    return handler;
}
#endif

void print(const char* str) {
    if (!str) return;
    // Check global log level - if NONE, suppress all output
    if (gLogLevel == LOG_LEVEL_NONE) return;

#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_print_handler()) {
        get_print_handler()(str);
        return;
    }
#endif

    // Delegate to platform implementation
    platforms::print(str);
}

void println(const char* str) {
    if (!str) return;
    // Check global log level - if NONE, suppress all output
    if (gLogLevel == LOG_LEVEL_NONE) return;

#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_println_handler()) {
        get_println_handler()(str);
        return;
    }
#endif

    // Delegate to platform implementation
    platforms::println(str);
}

int available() {
#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_available_handler()) {
        return get_available_handler()();
    }
#endif

    // Delegate to platform implementation
    return platforms::available();
}

int peek() {
    return platforms::peek();
}

int read() {
#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_read_handler()) {
        return get_read_handler()();
    }
#endif

    // Delegate to platform implementation
    return platforms::read();
}

bool readStringUntil(sstream& out, char delimiter, char skipChar, fl::optional<u32> timeoutMs) {
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

        // Valid character - add to output stream
        out << static_cast<char>(c);
    }

    // Successfully read until delimiter
    return true;
}

fl::optional<fl::string> readLine(char delimiter, char skipChar, fl::optional<u32> timeoutMs) {
    // Delegate to readStringUntil for efficient character accumulation
    sstream buffer;
    if (!readStringUntil(buffer, delimiter, skipChar, timeoutMs)) {
        return fl::nullopt;  // Timeout occurred
    }

    // Convert to string and trim whitespace (trim() returns new StrN, convert to string)
    fl::string result = buffer.str();
    return fl::string(result.trim());
}

bool flush(u32 timeoutMs) {
    return platforms::flush(timeoutMs);
}

size_t write_bytes(const u8* buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    return platforms::write_bytes(buffer, size);
}

void serial_begin(u32 baudRate) {
    platforms::begin(baudRate);
}

bool serial_ready() {
    return platforms::serial_ready();
}

// =============================================================================
// Timing Functions
// =============================================================================
// Note: fl::millis() is provided by fl/stl/chrono.h
// Note: fl::delay() is provided by fl/delay.h

#ifdef FASTLED_TESTING

// Inject function handlers for testing
void inject_print_handler(const print_handler_t& handler) {
    get_print_handler() = handler;
}

void inject_println_handler(const println_handler_t& handler) {
    get_println_handler() = handler;
}

void inject_available_handler(const available_handler_t& handler) {
    get_available_handler() = handler;
}

void inject_read_handler(const read_handler_t& handler) {
    get_read_handler() = handler;
}

// Clear all injected handlers (restores default behavior)
void clear_io_handlers() {
    get_print_handler() = print_handler_t{};
    get_println_handler() = println_handler_t{};
    get_available_handler() = available_handler_t{};
    get_read_handler() = read_handler_t{};
}

// Clear individual handlers
void clear_print_handler() {
    get_print_handler() = print_handler_t{};
}

void clear_println_handler() {
    get_println_handler() = println_handler_t{};
}

void clear_available_handler() {
    get_available_handler() = available_handler_t{};
}

void clear_read_handler() {
    get_read_handler() = read_handler_t{};
}

// Force early initialization of function-level statics to avoid
// static initialization order issues on macOS. The fl::function
// objects may allocate memory during construction, and if the first
// access happens during a memory allocation callback, we get recursion.
namespace cstdio_init {
    void init_io_handlers() {
        // Touch each handler to force initialization during static construction
        (void)get_print_handler();
        (void)get_println_handler();
        (void)get_available_handler();
        (void)get_read_handler();
    }
}

FL_INIT(cstdio_init_wrapper, cstdio_init::init_io_handlers)

#endif // FASTLED_TESTING

} // namespace fl
