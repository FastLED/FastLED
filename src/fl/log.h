/// @file log.h
/// @brief Category-based runtime logging system for FastLED subsystems
///
/// # FastLED Logging System
///
/// This header provides a **hybrid compile-time + runtime** selective logging system
/// for structured diagnostics of FastLED subsystems (SPI, RMT, VIDEO, I2S, LCD, UART, TIMING).
///
/// ## Design Principles
///
/// ### 1. Zero Overhead When Disabled
/// When a category is NOT enabled at compile-time, logging statements are completely
/// stripped by the preprocessor and optimizer - no code, no data, zero runtime cost.
///
/// Example:
/// ```cpp
/// // Without -DFASTLED_LOG_SPI_ENABLED:
/// FL_LOG_SPI("msg");  // → preprocessor expansion → FL_DBG_NO_OP(...) → eliminated by optimizer
/// // Result: 0 bytes code, 0 bytes data, 0 cycles runtime
/// ```
///
/// ### 2. Runtime Toggle When Enabled
/// When a category IS enabled at compile-time, you can toggle it on/off at runtime
/// without recompilation. A single bitfield tracks the runtime state of all 7 categories.
///
/// Example:
/// ```cpp
/// // With -DFASTLED_LOG_SPI_ENABLED:
/// FL_LOG_SPI("msg");  // → runtime check: if (LogState::isEnabled(SPI)) FL_WARN(...)
///
/// // In your sketch:
/// fl::LogState::enable(fl::LogCategory::SPI);   // Turn on
/// fl::LogState::disable(fl::LogCategory::SPI);  // Turn off (no recompilation!)
/// ```
///
/// ### 3. Platform-Aware Output
/// All logs use FL_WARN internally, which respects:
/// - Platform-specific output (Serial on Arduino, printf on ESP32, etc.)
/// - Memory constraints (SKETCH_HAS_LOTS_OF_MEMORY)
/// - Automatic file:line prefix formatting
/// - Stream-style formatting with type conversions
///
/// ### 4. Minimal Runtime State
/// Runtime state is stored in a single `uint8_t` bitfield (4 bytes when 8+ categories).
/// No dynamic allocation, no complex state management, safe for multi-threaded use.
///
/// ## Quick Start
///
/// ### 1. Build with logging enabled
///
/// Add to `build_flags` in platformio.ini or compiler settings:
/// ```ini
/// # Single category
/// build_flags = -DFASTLED_LOG_SPI_ENABLED
///
/// # Multiple categories
/// build_flags =
///     -DFASTLED_LOG_SPI_ENABLED
///     -DFASTLED_LOG_I2S_ENABLED
///     -DFASTLED_LOG_TIMING_ENABLED
///
/// # All categories (debug builds)
/// build_flags = -DFASTLED_LOG_ALL_ENABLED
/// ```
///
/// Or in Arduino IDE: Sketch → Preferences → Add to build flags
///
/// ### 2. Use in sketches
/// ```cpp
/// #include "FastLED.h"
///
/// void setup() {
///     fl::LogState::enable(fl::LogCategory::SPI);
/// }
///
/// void loop() {
///     FL_LOG_SPI("Debug: " << value);  // Outputs only when enabled
/// }
/// ```
///
/// ### 3. Toggle at runtime
/// ```cpp
/// if (Serial.available() && Serial.read() == 's') {
///     if (fl::LogState::isEnabled(fl::LogCategory::SPI)) {
///         fl::LogState::disable(fl::LogCategory::SPI);
///     } else {
///         fl::LogState::enable(fl::LogCategory::SPI);
///     }
/// }
/// ```
///
/// ## Categories
///
/// | Category | Macro | Use Case |
/// |----------|-------|----------|
/// | SPI | `FL_LOG_SPI` | Serial Peripheral Interface initialization, transfers, timing |
/// | RMT | `FL_LOG_RMT` | ESP32 RMT peripheral pulse generation and configuration |
/// | VIDEO | `FL_LOG_VIDEO` | Video/framebuffer updates, memory allocation, scaling |
/// | I2S | `FL_LOG_I2S` | I2S streaming, buffer management, DMA status |
/// | LCD | `FL_LOG_LCD` | LCD command sequences, parallel interface timing |
/// | UART | `FL_LOG_UART` | Serial communication, connection status, data flow |
/// | TIMING | `FL_LOG_TIMING` | Frame timing, ISR latency, performance bottlenecks |
///
/// ## Implementation Details
///
/// ### How It Works: Hybrid Compile + Runtime
///
/// #### When NOT compiled in (e.g., no -DFASTLED_LOG_SPI_ENABLED):
/// ```cpp
/// // In log.h:
/// #else
///     #define FL_LOG_SPI(X) FL_DBG_NO_OP(X)
/// #endif
///
/// // In your code:
/// FL_LOG_SPI("msg" << x);
/// ↓ preprocessor expands to:
/// FL_DBG_NO_OP("msg" << x);
/// ↓ which is:
/// do { if (false) { fl::FakeStrStream() << "msg" << x; } } while(0)
/// ↓ optimizer removes dead code:
/// (empty)
/// ```
/// **Result:** Zero bytes code, zero runtime overhead.
///
/// #### When compiled in (e.g., -DFASTLED_LOG_SPI_ENABLED):
/// ```cpp
/// // In log.h:
/// #ifdef FASTLED_LOG_SPI_ENABLED
///     #define FL_LOG_SPI(X) do { \
///         if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { \
///             FL_WARN(X); \
///         } \
///     } while(0)
/// #endif
///
/// // In your code:
/// FL_LOG_SPI("msg" << x);
/// ↓ preprocessor expands to:
/// do {
///     if (fl::LogState::isEnabled(fl::LogCategory::SPI)) {
///         FL_WARN("msg" << x);  // Outputs to Serial/printf/etc
///     }
/// } while(0)
/// ```
/// **Result:** Runtime check only (~4-6 bytes code), toggleable via `LogState::enable/disable`.
///
/// ### Runtime State Management
///
/// State is stored in a single `uint8_t` bitfield:
/// ```cpp
/// // In log.cpp:
/// fl::u8 LogState::sEnabledCategories = 0x00;  // All disabled by default
///
/// // Bit layout (one bit per category):
/// // Bit 0: SPI      (0x01)
/// // Bit 1: RMT      (0x02)
/// // Bit 2: VIDEO    (0x04)
/// // Bit 3: I2S      (0x08)
/// // Bit 4: LCD      (0x10)
/// // Bit 5: UART     (0x20)
/// // Bit 6: TIMING   (0x40)
/// // Bit 7: unused   (0x80)
/// ```
///
/// Enable/disable operations use simple bitwise AND/OR:
/// ```cpp
/// // Enable SPI (category = 0):
/// sEnabledCategories |= (1u << 0);   // Set bit 0 → 0x01
///
/// // Check if SPI is enabled:
/// return (sEnabledCategories & (1u << 0)) != 0;
///
/// // Disable SPI:
/// sEnabledCategories &= ~(1u << 0);  // Clear bit 0
/// ```
///
/// ### Memory Usage
///
/// | Item | Size | Notes |
/// |------|------|-------|
/// | Runtime state (all 7 categories) | 4 bytes | Single `uint8_t` bitfield, static storage |
/// | Disabled logs (compile-time) | 0 bytes | Completely stripped by optimizer |
/// | Per-log statement (disabled) | 0 bytes | No code generated |
/// | Per-log statement (enabled but off) | ~4-6 bytes | Conditional branch |
/// | Per-log statement (enabled and on) | ~4-6 bytes | Conditional branch + FL_WARN call |
///
/// ### Thread Safety
///
/// On platforms with atomic support (ESP32, modern compilers):
/// - Single `uint8_t` reads/writes are atomic
/// - Enable/disable operations are race-free
/// - Safe for multi-threaded use without locks
///
/// On platforms without atomic support:
/// - No special synchronization needed (single byte is atomic by design)
/// - ISR-safe on all platforms
///
/// ## Comparison with Alternatives
///
/// ### FL_DBG (General Debug)
/// - Purpose: General-purpose debug output
/// - Scope: Affects all debug statements
/// - Toggle: Compile-time only (FASTLED_FORCE_DBG)
/// - Memory: ~5KB when enabled
/// - Use: Ad-hoc debugging during development
///
/// ### FL_WARN (Always-On Warnings)
/// - Purpose: Critical runtime warnings
/// - Scope: Always enabled (respects SKETCH_HAS_LOTS_OF_MEMORY)
/// - Toggle: Not toggleable
/// - Memory: Variable (~1-2KB)
/// - Use: Persistent error reporting
///
/// ### FL_LOG_* (Category Logging)
/// - Purpose: Structured subsystem diagnostics
/// - Scope: Selective by category
/// - Toggle: Runtime (when compiled in)
/// - Memory: ~4 bytes for state, zero when disabled
/// - Use: Detailed subsystem debugging without recompilation
///
/// ## Build Flags Reference
///
/// ### Enable individual categories
/// ```ini
/// # In platformio.ini:
/// build_flags =
///     -DFASTLED_LOG_SPI_ENABLED      # SPI logging
///     -DFASTLED_LOG_RMT_ENABLED      # RMT logging
///     -DFASTLED_LOG_VIDEO_ENABLED    # VIDEO logging
///     -DFASTLED_LOG_I2S_ENABLED      # I2S logging
///     -DFASTLED_LOG_LCD_ENABLED      # LCD logging
///     -DFASTLED_LOG_UART_ENABLED     # UART logging
///     -DFASTLED_LOG_TIMING_ENABLED   # TIMING logging
/// ```
///
/// ### Enable all categories
/// ```ini
/// # In platformio.ini:
/// build_flags = -DFASTLED_LOG_ALL_ENABLED      # Expands to all above flags
/// ```
///
/// ### Backward compatibility
/// ```ini
/// # Old naming still supported:
/// build_flags = -DFASTLED_DBG_SPI_ENABLED      # Maps to FL_LOG_SPI
/// ```
///
/// ## Usage Examples
///
/// ### Example 1: Basic logging in setup
/// ```cpp
/// #include "FastLED.h"
///
/// #define NUM_LEDS 60
/// CRGB leds[NUM_LEDS];
///
/// void setup() {
///     Serial.begin(115200);
///     FastLED.addLeds<NEOPIXEL, 5>(leds, NUM_LEDS);
///
///     // Enable SPI diagnostics
///     fl::LogState::enable(fl::LogCategory::SPI);
/// }
///
/// void loop() {
///     fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
///     FastLED.show();
/// }
/// ```
///
/// ### Example 2: Custom driver with logging
/// ```cpp
/// #include "fl/log.h"
///
/// class MyI2SDriver {
/// public:
///     bool initialize(uint32_t sample_rate, size_t buffer_size) {
///         FL_LOG_I2S("Initializing I2S driver");
///         FL_LOG_I2S("  Sample rate: " << sample_rate << " Hz");
///         FL_LOG_I2S("  Buffer size: " << buffer_size << " bytes");
///
///         // Your initialization code...
///
///         FL_LOG_I2S("I2S ready");
///         return true;
///     }
///
///     void transmit(const fl::span<const uint8_t> data) {
///         FL_LOG_I2S("Transmitting " << data.size() << " bytes");
///         // Your transmission code...
///     }
/// };
/// ```
///
/// ### Example 3: Runtime toggle via serial
/// ```cpp
/// void handleSerialCommand(char cmd) {
///     fl::LogCategory cat;
///     const char* name = nullptr;
///
///     switch (cmd) {
///         case 's': cat = fl::LogCategory::SPI;    name = "SPI"; break;
///         case 'r': cat = fl::LogCategory::RMT;    name = "RMT"; break;
///         case 'v': cat = fl::LogCategory::VIDEO;  name = "VIDEO"; break;
///         case 'i': cat = fl::LogCategory::I2S;    name = "I2S"; break;
///         default: return;
///     }
///
///     if (fl::LogState::isEnabled(cat)) {
///         fl::LogState::disable(cat);
///         Serial.print(name); Serial.println(" logging OFF");
///     } else {
///         fl::LogState::enable(cat);
///         Serial.print(name); Serial.println(" logging ON");
///     }
/// }
/// ```
///
/// ### Example 4: Bulk control
/// ```cpp
/// // Enable all categories for debugging
/// fl::LogState::enableAll();
///
/// // ... test your code ...
///
/// // Disable all for normal operation
/// fl::LogState::disableAll();
/// ```
///
/// ## Platform Support
///
/// Logging works on all FastLED platforms:
/// - **AVR (Arduino Uno, Leonardo, etc.)**: Serial output, respects memory constraints
/// - **ESP32 (all variants)**: Serial or printf output, atomic-safe on RISC-V cores
/// - **Teensy**: Serial output, full support
/// - **STM32**: Serial output, full support
/// - **WASM/Web**: Console output, full support
/// - **Host (Linux/macOS/Windows)**: stdout/stderr, full support
///
/// ## Future Extensions
///
/// Possible enhancements (not currently implemented):
/// - Per-category filtering based on patterns
/// - Log level filtering (ERROR, WARN, INFO, DEBUG)
/// - Buffered logging for efficiency
/// - Remote logging support
/// - Conditional rate-limiting (avoid spam)
///
/// ## See Also
///
/// - `fl/dbg.h`: General debug output (includes this header)
/// - `fl/warn.h`: Always-on warnings
/// - `fl/trace.h`: Platform tracing utilities
/// - `examples/RuntimeLogging/RuntimeLogging.ino`: Interactive demo sketch

#pragma once

#include "fl/warn.h"
#include "fl/int.h"

namespace fl {

/// Category tags for selective runtime logging
///
/// Each category represents a different FastLED subsystem that can be logged independently.
/// Use these with `LogState::enable()`, `LogState::disable()`, and `LogState::isEnabled()`.
///
/// Example:
/// ```cpp
/// fl::LogState::enable(fl::LogCategory::SPI);      // Turn on SPI logging
/// fl::LogState::disable(fl::LogCategory::RMT);     // Turn off RMT logging
/// if (fl::LogState::isEnabled(fl::LogCategory::I2S)) { /* ... */ }
/// ```
enum class LogCategory : fl::u8 {
    SPI = 0,      ///< Serial Peripheral Interface: bus initialization, register writes, transfers
    RMT,          ///< ESP32 RMT peripheral: pulse generation, timing configuration
    VIDEO,        ///< Video/framebuffer: frame updates, memory allocation, scaling operations
    I2S,          ///< I2S audio/data streaming: buffer management, DMA status
    LCD,          ///< Parallel LCD/displays: command sequences, pixel transfers, timing
    UART,         ///< Serial/UART operations: connection status, data flow, baud rate
    TIMING,       ///< Timing/performance diagnostics: frame timing, ISR latency, bottlenecks
    END           ///< Marker for total count (must be last)
};

/// Runtime logging state management
///
/// This class manages which categories are enabled/disabled at runtime. Categories that are
/// not enabled at compile-time (via -DFASTLED_LOG_<CATEGORY>_ENABLED) cannot be toggled,
/// but you can still control categories that ARE compiled in.
///
/// All state is stored in a single static `uint8_t` bitfield with one bit per category.
/// Default state is all categories disabled (sEnabledCategories = 0x00).
///
/// Thread-safety: Safe on all platforms. Single-byte operations are atomic, and no complex
/// synchronization is needed. ISR-safe on all platforms.
///
/// Memory: 4 bytes total for all 7 categories (stored in `sEnabledCategories`).
///
/// Example:
/// ```cpp
/// // Enable specific categories
/// fl::LogState::enable(fl::LogCategory::SPI);
/// fl::LogState::enable(fl::LogCategory::TIMING);
///
/// // Check status
/// if (fl::LogState::isEnabled(fl::LogCategory::SPI)) {
///     // SPI logging is currently on
/// }
///
/// // Disable specific categories
/// fl::LogState::disable(fl::LogCategory::SPI);
///
/// // Bulk operations
/// fl::LogState::enableAll();    // Turn on all compiled-in categories
/// fl::LogState::disableAll();   // Turn off all categories
/// ```
class LogState {
public:
    /// Enable logging for a specific category
    ///
    /// If the category is not compiled in (no -DFASTLED_LOG_<CATEGORY>_ENABLED flag),
    /// this function does nothing (the log statements won't generate code anyway).
    ///
    /// If the category IS compiled in, this sets the runtime flag so that
    /// FL_LOG_<CATEGORY> statements will output to FL_WARN.
    ///
    /// @param category The category to enable (one of LogCategory enum values)
    ///
    /// Example:
    /// ```cpp
    /// fl::LogState::enable(fl::LogCategory::SPI);   // Turn on SPI logging
    /// ```
    static void enable(LogCategory category) {
        if (static_cast<fl::u8>(category) < static_cast<fl::u8>(LogCategory::END)) {
            sEnabledCategories |= (1u << static_cast<fl::u8>(category));
        }
    }

    /// Disable logging for a specific category
    ///
    /// If the category is not compiled in, this function does nothing.
    ///
    /// If the category IS compiled in, this clears the runtime flag so that
    /// FL_LOG_<CATEGORY> statements will not output.
    ///
    /// @param category The category to disable
    ///
    /// Example:
    /// ```cpp
    /// fl::LogState::disable(fl::LogCategory::SPI);   // Turn off SPI logging
    /// ```
    static void disable(LogCategory category) {
        if (static_cast<fl::u8>(category) < static_cast<fl::u8>(LogCategory::END)) {
            sEnabledCategories &= ~(1u << static_cast<fl::u8>(category));
        }
    }

    /// Enable all categories that are compiled in
    ///
    /// This sets all 8 bits in sEnabledCategories, which enables logging for all
    /// categories that have compile-time support. Categories without compile-time
    /// support still won't generate logs (they're stripped at compile-time).
    ///
    /// Useful for debug builds or when you want to see all diagnostics:
    /// ```cpp
    /// fl::LogState::enableAll();   // Turn on all available logging
    /// ```
    static void enableAll() {
        sEnabledCategories = 0xFF;  // All bits set
    }

    /// Disable all categories
    ///
    /// This clears all bits in sEnabledCategories, turning off logging for all categories.
    /// This is the most efficient state for production code.
    ///
    /// Example:
    /// ```cpp
    /// fl::LogState::disableAll();  // Production mode: no logging
    /// ```
    static void disableAll() {
        sEnabledCategories = 0x00;
    }

    /// Check if a category is currently enabled at runtime
    ///
    /// Returns true if both:
    /// 1. The category was compiled in (has -DFASTLED_LOG_<CATEGORY>_ENABLED flag)
    /// 2. The category is currently enabled via LogState::enable()
    ///
    /// @param category The category to check
    /// @return true if the category is enabled and will generate logs
    ///
    /// Example:
    /// ```cpp
    /// if (fl::LogState::isEnabled(fl::LogCategory::SPI)) {
    ///     Serial.println("SPI logging is ON");
    /// } else {
    ///     Serial.println("SPI logging is OFF");
    /// }
    /// ```
    static bool isEnabled(LogCategory category) {
        if (static_cast<fl::u8>(category) >= static_cast<fl::u8>(LogCategory::END)) {
            return false;
        }
        return (sEnabledCategories & (1u << static_cast<fl::u8>(category))) != 0;
    }

private:
    /// Runtime state: one bit per category (0 = disabled by default)
    ///
    /// Bit layout:
    ///   Bit 0: SPI    (1 << LogCategory::SPI)
    ///   Bit 1: RMT    (1 << LogCategory::RMT)
    ///   Bit 2: VIDEO  (1 << LogCategory::VIDEO)
    ///   Bit 3: I2S    (1 << LogCategory::I2S)
    ///   Bit 4: LCD    (1 << LogCategory::LCD)
    ///   Bit 5: UART   (1 << LogCategory::UART)
    ///   Bit 6: TIMING (1 << LogCategory::TIMING)
    ///   Bit 7: unused
    static fl::u8 sEnabledCategories;
};

} // namespace fl

//================================================================================
// Category-based logging macros
//================================================================================
///
/// These macros provide the actual logging statements. Each macro has two behaviors
/// depending on whether the category was compiled in:
///
/// 1. NOT compiled in (no -DFASTLED_LOG_<CATEGORY>_ENABLED):
///    → Expands to FL_DBG_NO_OP(...) → compiler optimization removes it entirely
///    → Result: zero overhead
///
/// 2. Compiled in (has -DFASTLED_LOG_<CATEGORY>_ENABLED):
///    → Expands to: do { if (LogState::isEnabled(...)) FL_WARN(...); } while(0)
///    → Result: runtime check, then output to FL_WARN if enabled
///
/// All macros use stream-style formatting (like FL_WARN):
/// ```cpp
/// FL_LOG_SPI("Value: " << hex << 0xFF << " status: " << status);
/// FL_LOG_I2S("Sent " << num_bytes << " bytes");
/// FL_LOG_TIMING("Frame took " << duration_us << " microseconds");
/// ```
///
/// To enable logging for a category, include the -D flag in your build:
/// ```bash
/// -DFASTLED_LOG_SPI_ENABLED
/// -DFASTLED_LOG_RMT_ENABLED
/// -DFASTLED_LOG_I2S_ENABLED
/// etc.
/// ```
///
/// Then in your code, toggle at runtime:
/// ```cpp
/// fl::LogState::enable(fl::LogCategory::SPI);    // Turn on
/// fl::LogState::disable(fl::LogCategory::SPI);   // Turn off
/// ```
///
/// For debug builds, enable all categories:
/// ```bash
/// -DFASTLED_LOG_ALL_ENABLED
/// ```
///================================================================================

/// SPI/HSPI operations: bus initialization, register writes, DMA transfers, timing
///
/// Use this to debug Serial Peripheral Interface issues on any platform.
/// Includes hardware initialization, pin configuration, frequency setup, and data transfers.
#ifdef FASTLED_LOG_SPI_ENABLED
    #define FL_LOG_SPI(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_SPI(X) FL_DBG_NO_OP(X)
#endif

/// ESP32 RMT (Remote Control Module) operations: pulse generation, timing, configuration
///
/// RMT is ESP32's hardware pulse generation peripheral, ideal for addressable LED drivers.
/// Use this to debug RMT timing, ISR behavior, memory allocation, and state changes.
#ifdef FASTLED_LOG_RMT_ENABLED
    #define FL_LOG_RMT(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::RMT)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_RMT(X) FL_DBG_NO_OP(X)
#endif

/// Video/framebuffer operations: updates, memory allocation, scaling, compositing
///
/// Use this to debug display rendering, frame timing, buffer management, and
/// visual glitches in video output or complex graphics.
#ifdef FASTLED_LOG_VIDEO_ENABLED
    #define FL_LOG_VIDEO(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::VIDEO)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_VIDEO(X) FL_DBG_NO_OP(X)
#endif

/// I2S (Inter-IC Sound) audio/data streaming: buffer management, DMA transfers, synchronization
///
/// I2S is commonly used for audio input/output and high-speed data transfer.
/// Use this to debug streaming issues, buffer underruns/overruns, and sample timing.
#ifdef FASTLED_LOG_I2S_ENABLED
    #define FL_LOG_I2S(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::I2S)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_I2S(X) FL_DBG_NO_OP(X)
#endif

/// LCD/parallel display operations: command sequences, pixel transfers, timing, refresh
///
/// Use this to debug parallel LCD driver issues, pixel corruption, timing violations,
/// or display initialization problems.
#ifdef FASTLED_LOG_LCD_ENABLED
    #define FL_LOG_LCD(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::LCD)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_LCD(X) FL_DBG_NO_OP(X)
#endif

/// Serial/UART operations: connection status, baud rate, data flow, transmission
///
/// Use this to debug serial communication, handshake issues, data corruption,
/// or protocol timing problems.
#ifdef FASTLED_LOG_UART_ENABLED
    #define FL_LOG_UART(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::UART)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_UART(X) FL_DBG_NO_OP(X)
#endif

/// Timing/performance diagnostics: frame timing, ISR latency, throughput, bottlenecks
///
/// Use this to profile performance bottlenecks, measure frame rates, detect timing violations,
/// or track ISR execution time. Useful for optimization and real-time constraint analysis.
#ifdef FASTLED_LOG_TIMING_ENABLED
    #define FL_LOG_TIMING(X) do { \
        if (fl::LogState::isEnabled(fl::LogCategory::TIMING)) { \
            FL_WARN(X); \
        } \
    } while(0)
#else
    #define FL_LOG_TIMING(X) FL_DBG_NO_OP(X)
#endif
