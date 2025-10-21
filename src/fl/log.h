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

// =============================================================================
// Hardware Interface Logging Categories
// =============================================================================

/// @defgroup log_hw_interfaces Hardware Interface Logging
/// @{

/// @brief Serial Peripheral Interface (SPI) logging
/// Logs SPI configuration, initialization, and transfers
#ifdef FASTLED_LOG_SPI_ENABLED
    #define FL_LOG_SPI FL_WARN
#else
    #define FL_LOG_SPI(X) FL_DBG_NO_OP(X)
#endif

/// @brief Remote Control Module (RMT) logging (ESP32)
/// Logs RMT channel configuration, timing, and signal generation
#ifdef FASTLED_LOG_RMT_ENABLED
    #define FL_LOG_RMT FL_WARN
#else
    #define FL_LOG_RMT(X) FL_DBG_NO_OP(X)
#endif

/// @brief Parallel I/O (Parlio) logging (ESP32-P4)
/// Logs Parlio configuration, GPIO setup, and parallel transfers
#ifdef FASTLED_LOG_PARLIO_ENABLED
    #define FL_LOG_PARLIO FL_WARN
#else
    #define FL_LOG_PARLIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief I2S (Inter-IC Sound) logging
/// Logs I2S configuration, DMA setup, and audio/signal generation
#ifdef FASTLED_LOG_I2S_ENABLED
    #define FL_LOG_I2S FL_WARN
#else
    #define FL_LOG_I2S(X) FL_DBG_NO_OP(X)
#endif

/// @brief GPIO (General Purpose Input/Output) logging
/// Logs GPIO pin configuration and state changes
#ifdef FASTLED_LOG_GPIO_ENABLED
    #define FL_LOG_GPIO FL_WARN
#else
    #define FL_LOG_GPIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief PIO (Programmable Input/Output) logging (RP2040, RP2350)
/// Logs PIO program loading, state machine configuration, and execution
#ifdef FASTLED_LOG_PIO_ENABLED
    #define FL_LOG_PIO FL_WARN
#else
    #define FL_LOG_PIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief DMA (Direct Memory Access) logging
/// Logs DMA channel configuration, transfers, and interrupts
#ifdef FASTLED_LOG_DMA_ENABLED
    #define FL_LOG_DMA FL_WARN
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
    #define FL_LOG_TIMER FL_WARN
#else
    #define FL_LOG_TIMER(X) FL_DBG_NO_OP(X)
#endif

/// @brief Interrupt handler logging
/// Logs interrupt installation, execution, and timing
#ifdef FASTLED_LOG_INTERRUPT_ENABLED
    #define FL_LOG_INTERRUPT FL_WARN
#else
    #define FL_LOG_INTERRUPT(X) FL_DBG_NO_OP(X)
#endif

/// @brief Memory allocation and management logging
/// Logs memory allocation, deallocation, and heap status
#ifdef FASTLED_LOG_MEMORY_ENABLED
    #define FL_LOG_MEMORY FL_WARN
#else
    #define FL_LOG_MEMORY(X) FL_DBG_NO_OP(X)
#endif

/// @brief Clock/frequency management logging
/// Logs clock source selection, frequency changes, and prescaler adjustments
#ifdef FASTLED_LOG_CLOCK_ENABLED
    #define FL_LOG_CLOCK FL_WARN
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
    #define FL_LOG_PROTOCOL FL_WARN
#else
    #define FL_LOG_PROTOCOL(X) FL_DBG_NO_OP(X)
#endif

/// @brief Color space and color processing logging
/// Logs color conversions, scaling, and gamma correction
#ifdef FASTLED_LOG_COLOR_ENABLED
    #define FL_LOG_COLOR FL_WARN
#else
    #define FL_LOG_COLOR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Audio processing logging
/// Logs audio input configuration, processing, and reactive effects
#ifdef FASTLED_LOG_AUDIO_ENABLED
    #define FL_LOG_AUDIO FL_WARN
#else
    #define FL_LOG_AUDIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief Power management logging
/// Logs power calculations, voltage/current limits, and throttling
#ifdef FASTLED_LOG_POWER_ENABLED
    #define FL_LOG_POWER FL_WARN
#else
    #define FL_LOG_POWER(X) FL_DBG_NO_OP(X)
#endif

/// @brief Animation/effect logging
/// Logs effect initialization, timing, and state transitions
#ifdef FASTLED_LOG_EFFECT_ENABLED
    #define FL_LOG_EFFECT FL_WARN
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
    #define FL_LOG_ESP32 FL_WARN
#else
    #define FL_LOG_ESP32(X) FL_DBG_NO_OP(X)
#endif

/// @brief ARM-based platform logging (STM32, NRF52, etc.)
/// Logs ARM peripheral setup and configuration
#ifdef FASTLED_LOG_ARM_ENABLED
    #define FL_LOG_ARM FL_WARN
#else
    #define FL_LOG_ARM(X) FL_DBG_NO_OP(X)
#endif

/// @brief AVR-based platform logging (Arduino, ATmega, ATtiny)
/// Logs AVR-specific setup and interrupt handling
#ifdef FASTLED_LOG_AVR_ENABLED
    #define FL_LOG_AVR FL_WARN
#else
    #define FL_LOG_AVR(X) FL_DBG_NO_OP(X)
#endif

/// @brief RP2040/RP2350-specific logging
/// Logs Raspberry Pi Pico platform configuration
#ifdef FASTLED_LOG_RP_ENABLED
    #define FL_LOG_RP FL_WARN
#else
    #define FL_LOG_RP(X) FL_DBG_NO_OP(X)
#endif

/// @brief WASM/Emscripten-specific logging
/// Logs web-based platform configuration
#ifdef FASTLED_LOG_WASM_ENABLED
    #define FL_LOG_WASM FL_WARN
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
    #define FL_LOG_ENGINE FL_WARN
#else
    #define FL_LOG_ENGINE(X) FL_DBG_NO_OP(X)
#endif

/// @brief Animation update events logging
/// Logs frame updates, animation timing, and refresh cycles
#ifdef FASTLED_LOG_UPDATE_ENABLED
    #define FL_LOG_UPDATE FL_WARN
#else
    #define FL_LOG_UPDATE(X) FL_DBG_NO_OP(X)
#endif

/// @brief Strip/controller events logging
/// Logs LED strip management, controller operations, and state
#ifdef FASTLED_LOG_STRIP_ENABLED
    #define FL_LOG_STRIP FL_WARN
#else
    #define FL_LOG_STRIP(X) FL_DBG_NO_OP(X)
#endif

/// @brief Timing and sync events logging
/// Logs sync points, frame timing, and synchronization between components
#ifdef FASTLED_LOG_SYNC_ENABLED
    #define FL_LOG_SYNC FL_WARN
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
    #define FL_LOG_JS FL_WARN
#else
    #define FL_LOG_JS(X) FL_DBG_NO_OP(X)
#endif

/// @brief Web API logging
/// Logs HTTP/WebSocket communication and web API calls
#ifdef FASTLED_LOG_API_ENABLED
    #define FL_LOG_API FL_WARN
#else
    #define FL_LOG_API(X) FL_DBG_NO_OP(X)
#endif

/// @brief Rendering/Canvas logging
/// Logs web canvas operations and rendering updates
#ifdef FASTLED_LOG_RENDER_ENABLED
    #define FL_LOG_RENDER FL_WARN
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
