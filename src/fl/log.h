#pragma once

#include "fl/dbg.h"
#include "fl/warn.h"

/// @file fl/log.h
/// @brief Centralized logging categories for FastLED hardware interfaces and subsystems
///
/// This file provides category-specific logging macros for different FastLED subsystems.
/// Each category can be independently enabled/disabled via preprocessor defines.
///
/// Usage:
///   - Enable category debugging: Define FASTLED_LOG_<CATEGORY>_ENABLED before including this file
///   - Use the macro: FL_LOG_<CATEGORY>("message" << value)
///   - Categories persist in release builds (using FL_WARN level)
///
/// Example:
///   #define FASTLED_LOG_SPI_ENABLED
///   #include "fl/log.h"
///
///   FL_LOG_SPI("Initializing SPI bus " << bus_id);

namespace fl {

// =============================================================================
// Logging Category Enumeration and Runtime State Management
// =============================================================================

/// Runtime logging category enumeration
///
/// Used with LogState::enable/disable/isEnabled to control logging at runtime.
/// Each category corresponds to one bit in the LogState::sEnabledCategories bitfield.
///
/// Memory: 1 byte total for all 5 categories (stored in `sEnabledCategories`).
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
enum class LogCategory : fl::u8 {
    SPI = 0,      ///< Serial Peripheral Interface: bus initialization, register writes, transfers; also timing/performance diagnostics
    RMT,          ///< ESP32 RMT peripheral: pulse generation, timing configuration
    VIDEO,        ///< Video/framebuffer: frame updates, memory allocation, scaling operations
    I2S,          ///< I2S audio/data streaming: buffer management, DMA status
    LCD,          ///< Parallel LCD/displays: command sequences, pixel transfers, timing
    END           ///< Marker for total count (must be last)
};

/// Runtime logging state management
///
/// Provides methods to enable/disable logging categories at runtime without recompilation.
/// When a category is NOT enabled at compile-time (no -DFASTLED_LOG_<CATEGORY>_ENABLED flag),
/// logging statements are completely stripped by the preprocessor and optimizer.
///
/// When a category IS enabled at compile-time, you can toggle it on/off at runtime
/// using these methods. A single bitfield tracks the runtime state of all categories.
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
    ///   Bit 0: SPI   (1 << LogCategory::SPI)
    ///   Bit 1: RMT   (1 << LogCategory::RMT)
    ///   Bit 2: VIDEO (1 << LogCategory::VIDEO)
    ///   Bit 3: I2S   (1 << LogCategory::I2S)
    ///   Bit 4: LCD   (1 << LogCategory::LCD)
    ///   Bit 5-7: unused
    static fl::u8 sEnabledCategories;
};

}  // namespace fl

// =============================================================================
// Hardware Interface Logging Categories
// =============================================================================

/// @defgroup log_hw_interfaces Hardware Interface Logging
/// @{

/// @brief Serial Peripheral Interface (SPI) logging
/// Logs SPI configuration, initialization, and transfers
#ifdef FASTLED_LOG_SPI_ENABLED
    #define FL_LOG_SPI(X) do { if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_SPI(X) FL_DBG_NO_OP(X)
#endif

/// @brief Remote Control Module (RMT) logging (ESP32)
/// Logs RMT channel configuration, timing, and signal generation
#ifdef FASTLED_LOG_RMT_ENABLED
    #define FL_LOG_RMT(X) do { if (fl::LogState::isEnabled(fl::LogCategory::RMT)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_RMT(X) FL_DBG_NO_OP(X)
#endif

/// @brief Parallel I/O (Parlio) logging (ESP32-P4)
/// Logs Parlio configuration, GPIO setup, and parallel transfers
#ifdef FASTLED_LOG_PARLIO_ENABLED
    #define FL_LOG_PARLIO(X) do { if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_PARLIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief I2S (Inter-IC Sound) logging
/// Logs I2S configuration, DMA setup, and audio/signal generation
#ifdef FASTLED_LOG_I2S_ENABLED
    #define FL_LOG_I2S(X) do { if (fl::LogState::isEnabled(fl::LogCategory::I2S)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_I2S(X) FL_DBG_NO_OP(X)
#endif

/// @brief GPIO (General Purpose Input/Output) logging
/// Logs GPIO pin configuration and state changes
#ifdef FASTLED_LOG_GPIO_ENABLED
    #define FL_LOG_GPIO(X) do { if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_GPIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief PIO (Programmable Input/Output) logging (RP2040, RP2350)
/// Logs PIO program loading, state machine configuration, and execution
#ifdef FASTLED_LOG_PIO_ENABLED
    #define FL_LOG_PIO(X) do { if (fl::LogState::isEnabled(fl::LogCategory::SPI)) { FL_WARN(X); } } while(0)
#else
    #define FL_LOG_PIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief DMA (Direct Memory Access) logging
/// Logs DMA channel configuration, transfers, and interrupts
#ifdef FASTLED_LOG_DMA_ENABLED
    #define FL_LOG_DMA(X) FL_WARN(X)
#else
    #define FL_LOG_DMA(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// System-level Logging Categories
// =============================================================================

/// @defgroup log_system System-level Logging
/// @{

/// @brief Timer/Clock logging
/// Logs timer initialization, prescaler settings, and interrupt configuration
#ifdef FASTLED_LOG_TIMER_ENABLED
    #define FL_LOG_TIMER(X) FL_WARN(X)
#else
    #define FL_LOG_TIMER(X) FL_DBG_NO_OP(X)
#endif

/// @brief Interrupt handler logging
/// Logs interrupt installation, execution, and timing
#ifdef FASTLED_LOG_INTERRUPT_ENABLED
    #define FL_LOG_INTERRUPT(X) FL_WARN(X)
#else
    #define FL_LOG_INTERRUPT(X) FL_DBG_NO_OP(X)
#endif

/// @brief Memory allocation and management logging
/// Logs memory allocation, deallocation, and heap status
#ifdef FASTLED_LOG_MEMORY_ENABLED
    #define FL_LOG_MEMORY(X) FL_WARN(X)
#else
    #define FL_LOG_MEMORY(X) FL_DBG_NO_OP(X)
#endif

/// @brief Clock/frequency management logging
/// Logs clock source selection, frequency changes, and prescaler adjustments
#ifdef FASTLED_LOG_CLOCK_ENABLED
    #define FL_LOG_CLOCK(X) FL_WARN(X)
#else
    #define FL_LOG_CLOCK(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Feature-specific Logging Categories
// =============================================================================

/// @defgroup log_features Feature-specific Logging
/// @{

/// @brief LED communication protocol logging
/// Logs protocol selection, timing parameters, and pixel data transfers
#ifdef FASTLED_LOG_PROTOCOL_ENABLED
    #define FL_LOG_PROTOCOL(X) FL_WARN(X)
#else
    #define FL_LOG_PROTOCOL(X) FL_DBG_NO_OP(X)
#endif

/// @brief Color space and color processing logging
/// Logs color conversions, scaling, and gamma correction
#ifdef FASTLED_LOG_COLOR_ENABLED
    #define FL_LOG_COLOR(X) FL_WARN(X)
#else
    #define FL_LOG_COLOR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Audio processing logging
/// Logs audio input configuration, processing, and reactive effects
#ifdef FASTLED_LOG_AUDIO_ENABLED
    #define FL_LOG_AUDIO(X) FL_WARN(X)
#else
    #define FL_LOG_AUDIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief Power management logging
/// Logs power calculations, voltage/current limits, and throttling
#ifdef FASTLED_LOG_POWER_ENABLED
    #define FL_LOG_POWER(X) FL_WARN(X)
#else
    #define FL_LOG_POWER(X) FL_DBG_NO_OP(X)
#endif

/// @brief Animation/effect logging
/// Logs effect initialization, timing, and state transitions
#ifdef FASTLED_LOG_EFFECT_ENABLED
    #define FL_LOG_EFFECT(X) FL_WARN(X)
#else
    #define FL_LOG_EFFECT(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Platform-specific Logging Categories
// =============================================================================

/// @defgroup log_platform Platform-specific Logging
/// @{

/// @brief ESP32-specific logging
/// Logs ESP32 peripheral configuration and ESP-IDF integration
#ifdef FASTLED_LOG_ESP32_ENABLED
    #define FL_LOG_ESP32(X) FL_WARN(X)
#else
    #define FL_LOG_ESP32(X) FL_DBG_NO_OP(X)
#endif

/// @brief ARM-based platform logging (STM32, NRF52, etc.)
/// Logs ARM peripheral setup and configuration
#ifdef FASTLED_LOG_ARM_ENABLED
    #define FL_LOG_ARM(X) FL_WARN(X)
#else
    #define FL_LOG_ARM(X) FL_DBG_NO_OP(X)
#endif

/// @brief AVR-based platform logging (Arduino, ATmega, ATtiny)
/// Logs AVR-specific setup and interrupt handling
#ifdef FASTLED_LOG_AVR_ENABLED
    #define FL_LOG_AVR(X) FL_WARN(X)
#else
    #define FL_LOG_AVR(X) FL_DBG_NO_OP(X)
#endif

/// @brief RP2040/RP2350-specific logging
/// Logs Raspberry Pi Pico platform configuration
#ifdef FASTLED_LOG_RP_ENABLED
    #define FL_LOG_RP(X) FL_WARN(X)
#else
    #define FL_LOG_RP(X) FL_DBG_NO_OP(X)
#endif

/// @brief WASM/Emscripten-specific logging
/// Logs web-based platform configuration
#ifdef FASTLED_LOG_WASM_ENABLED
    #define FL_LOG_WASM(X) FL_WARN(X)
#else
    #define FL_LOG_WASM(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Engine and Application-level Logging Categories
// =============================================================================

/// @defgroup log_engine Engine and Application Logging
/// @{

/// @brief Engine events logging
/// Logs FastLED engine lifecycle events, startup, shutdown, and state changes
#ifdef FASTLED_LOG_ENGINE_ENABLED
    #define FL_LOG_ENGINE(X) FL_WARN(X)
#else
    #define FL_LOG_ENGINE(X) FL_DBG_NO_OP(X)
#endif

/// @brief Animation update events logging
/// Logs frame updates, animation timing, and refresh cycles
#ifdef FASTLED_LOG_UPDATE_ENABLED
    #define FL_LOG_UPDATE(X) FL_WARN(X)
#else
    #define FL_LOG_UPDATE(X) FL_DBG_NO_OP(X)
#endif

/// @brief Strip/controller events logging
/// Logs LED strip management, controller operations, and state
#ifdef FASTLED_LOG_STRIP_ENABLED
    #define FL_LOG_STRIP(X) FL_WARN(X)
#else
    #define FL_LOG_STRIP(X) FL_DBG_NO_OP(X)
#endif

/// @brief Timing and sync events logging
/// Logs sync points, frame timing, and synchronization between components
#ifdef FASTLED_LOG_SYNC_ENABLED
    #define FL_LOG_SYNC(X) FL_WARN(X)
#else
    #define FL_LOG_SYNC(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Web/JavaScript Logging Categories
// =============================================================================

/// @defgroup log_web Web and JavaScript Logging
/// @{

/// @brief JavaScript binding logging
/// Logs JavaScript/WASM binding initialization and calls
#ifdef FASTLED_LOG_JS_ENABLED
    #define FL_LOG_JS(X) FL_WARN(X)
#else
    #define FL_LOG_JS(X) FL_DBG_NO_OP(X)
#endif

/// @brief Web API logging
/// Logs HTTP/WebSocket communication and web API calls
#ifdef FASTLED_LOG_API_ENABLED
    #define FL_LOG_API(X) FL_WARN(X)
#else
    #define FL_LOG_API(X) FL_DBG_NO_OP(X)
#endif

/// @brief Rendering/Canvas logging
/// Logs web canvas operations and rendering updates
#ifdef FASTLED_LOG_RENDER_ENABLED
    #define FL_LOG_RENDER(X) FL_WARN(X)
#else
    #define FL_LOG_RENDER(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Convenience macros for conditional logging
// =============================================================================

/// @brief Conditional logging wrapper
/// Usage: FL_LOG_IF(category, condition, message)
/// Note: Requires manually selecting the appropriate category macro
#ifndef FL_LOG_IF
#define FL_LOG_IF(CATEGORY, COND, MSG) \
    if (COND) \
        CATEGORY(MSG)
#endif
