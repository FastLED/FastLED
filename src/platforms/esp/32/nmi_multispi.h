/**
 * @file nmi_multispi.h
 * @brief Level 7 NMI configuration API for multi-SPI on ESP32/ESP32-S3
 *
 * This file provides a user-friendly API for configuring and controlling
 * Level 7 NMI-driven multi-SPI parallel output. It enables ultra-low latency
 * multi-SPI operation with WiFi active by using non-maskable interrupts.
 *
 * Features:
 *   - 8 parallel SPI data lines + 1 clock line
 *   - 13.2 MHz max speed per strip (105.6 Mbps total throughput)
 *   - <70ns jitter (within WS2812 ±150ns tolerance)
 *   - Zero WiFi interference (NMI preempts all lower priority interrupts)
 *   - 6% CPU usage @ 800 kHz (WS2812)
 *   - 90%+ CPU free for WiFi, application logic, FreeRTOS
 *
 * Usage Example:
 * @code
 *   #include "platforms/esp/32/nmi_multispi.h"
 *
 *   uint8_t clock_pin = 17;
 *   uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};
 *   uint8_t buffer[8 * 100 * 3] DRAM_ATTR;  // 8 strips × 100 LEDs × 3 bytes
 *
 *   void setup() {
 *       // Initialize NMI multi-SPI at 800 kHz (WS2812)
 *       if (!fl::nmi::initMultiSPI(clock_pin, data_pins, 800000)) {
 *           Serial.println("ERROR: Failed to initialize NMI multi-SPI");
 *           return;
 *       }
 *
 *       // Fill buffer with LED data
 *       fillBufferWithColors(buffer, sizeof(buffer));
 *
 *       // Start transmission
 *       fl::nmi::startTransmission(buffer, sizeof(buffer));
 *   }
 *
 *   void loop() {
 *       if (fl::nmi::isTransmissionComplete()) {
 *           Serial.println("Transmission complete!");
 *           delay(1000);
 *           fl::nmi::startTransmission(buffer, sizeof(buffer));
 *       }
 *   }
 * @endcode
 *
 * IMPORTANT NOTES:
 *   - Transmission buffer MUST be in DRAM (use DRAM_ATTR)
 *   - All 9 pins (8 data + 1 clock) must be on same GPIO bank
 *   - ESP-IDF v5.0-v5.1 recommended (v5.2.1 has known NMI bugs)
 *   - QEMU ESP32-S3 supports Level 7 NMI (for testing)
 *   - Cannot use breakpoints or step-through debugging in NMI handler
 *
 * @see src/platforms/esp/32/XTENSA_INTERRUPTS.md for implementation details
 * @see examples/FastPinsNMI/ for complete working example
 *
 * Copyright (c) 2025 FastLED
 * Licensed under the MIT License
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

// Only compile on ESP32 platforms
#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32C6) || defined(ESP32H2)

namespace fl {
namespace nmi {

/**
 * @brief Initialize Level 7 NMI for multi-SPI parallel output
 *
 * This function configures the FastPinsWithClock controller and allocates a
 * hardware timer interrupt at Level 7 (NMI priority) for ultra-low latency
 * multi-SPI transmission.
 *
 * Configuration steps:
 *   1. Configure FastPinsWithClock with 8 data pins + 1 clock pin
 *   2. Validate all 9 pins are on same GPIO bank (required for atomic writes)
 *   3. Allocate hardware timer (TIMER_GROUP_0, TIMER_0)
 *   4. Configure timer frequency based on target bit rate
 *   5. Register timer interrupt at Level 7 (ESP_INTR_FLAG_LEVEL7)
 *   6. ESP-IDF will call xt_nmi symbol (our ASM wrapper)
 *
 * @param clock_pin GPIO pin number for clock line (must be on same bank as data pins)
 * @param data_pins Array of 8 GPIO pin numbers for parallel data lines
 * @param frequency Timer frequency in Hz (e.g., 800000 for WS2812, 13200000 for max speed)
 *
 * @return true if initialization succeeded, false on error
 *
 * @note Call this function once during setup()
 * @note Do NOT call this function multiple times without calling shutdown() first
 * @note All 9 pins must be on same GPIO bank (ESP32: 0-31 or 32-63)
 * @note frequency should match your LED protocol:
 *         - WS2812: 800000 Hz (800 kHz)
 *         - SK6812: 800000 Hz (800 kHz)
 *         - APA102: Up to 13200000 Hz (13.2 MHz max)
 *
 * Failure conditions:
 *   - Pins on different GPIO banks (validation fails)
 *   - Timer allocation fails (no available timers)
 *   - Interrupt allocation fails (ESP-IDF bug in v5.2.1)
 *   - Frequency out of valid range (1 kHz - 40 MHz)
 *
 * @warning ESP-IDF v5.2.1 has known bug preventing Level 7 NMI allocation
 *          Use ESP-IDF v5.0 or v5.1 for reliable operation
 *          See: https://github.com/espressif/esp-idf/issues/13629
 */
bool initMultiSPI(uint8_t clock_pin, const uint8_t* data_pins, uint32_t frequency);

/**
 * @brief Start NMI-driven multi-SPI transmission
 *
 * This function initiates a multi-SPI transmission using the buffer provided.
 * The NMI handler will transmit bytes sequentially at the configured frequency
 * until all bytes are transmitted.
 *
 * Transmission flow:
 *   1. Store buffer pointer, length, and mark transmission active
 *   2. Start hardware timer (triggers NMI at configured frequency)
 *   3. NMI handler reads bytes and drives FastPinsWithClock
 *   4. When all bytes transmitted, transmission marked inactive
 *   5. Timer continues running (ready for next transmission)
 *
 * @param buffer Pointer to data buffer to transmit (MUST be in DRAM!)
 * @param length Number of bytes to transmit
 *
 * @return true if transmission started successfully, false if already active
 *
 * @note Buffer MUST be in DRAM, not flash (use DRAM_ATTR on global buffers)
 * @note Buffer MUST remain valid until transmission completes (do not free/modify)
 * @note Cannot start new transmission while previous is active (check isTransmissionComplete first)
 * @note Transmission is non-blocking (function returns immediately)
 * @note Use isTransmissionComplete() to check when transmission finishes
 *
 * DRAM buffer example:
 * @code
 *   // ✅ GOOD - Buffer in DRAM
 *   uint8_t buffer[1024] DRAM_ATTR;
 *   fl::nmi::startTransmission(buffer, sizeof(buffer));
 *
 *   // ❌ BAD - Buffer may be in flash (will crash!)
 *   uint8_t buffer[1024];  // Could be in flash
 *   fl::nmi::startTransmission(buffer, sizeof(buffer));
 *
 *   // ✅ GOOD - Stack buffer (always in DRAM)
 *   void myFunction() {
 *       uint8_t buffer[256];  // Stack variables are in DRAM
 *       fl::nmi::startTransmission(buffer, sizeof(buffer));
 *       while (!fl::nmi::isTransmissionComplete()) { delay(1); }
 *   }
 * @endcode
 *
 * @warning Do NOT free or modify buffer until transmission completes!
 * @warning Do NOT pass flash-based buffers (use DRAM_ATTR)!
 * @warning Check isTransmissionComplete() before starting new transmission!
 */
bool startTransmission(const uint8_t* buffer, size_t length);

/**
 * @brief Check if NMI-driven transmission is complete
 *
 * This function checks the transmission status flag to determine if the
 * current transmission has finished. It is safe to call from any context
 * (main loop, tasks, other ISRs).
 *
 * @return true if transmission is complete (or never started), false if still transmitting
 *
 * @note This function is non-blocking (returns immediately)
 * @note Returns true if no transmission has been started yet
 * @note Returns true immediately after last byte is transmitted
 * @note Safe to call from main loop, tasks, or other ISRs
 *
 * Usage pattern:
 * @code
 *   // Start transmission
 *   fl::nmi::startTransmission(buffer, length);
 *
 *   // Wait for completion (blocking)
 *   while (!fl::nmi::isTransmissionComplete()) {
 *       delay(1);  // Yield to other tasks
 *   }
 *
 *   // Or check in loop (non-blocking)
 *   void loop() {
 *       if (fl::nmi::isTransmissionComplete()) {
 *           // Start next transmission
 *           fillBufferWithNewData(buffer);
 *           fl::nmi::startTransmission(buffer, length);
 *       }
 *   }
 * @endcode
 */
bool isTransmissionComplete();

/**
 * @brief Stop NMI timer and cleanup resources
 *
 * This function stops the hardware timer, deallocates the interrupt, and
 * resets all internal state. Call this function when you no longer need
 * NMI-driven multi-SPI (e.g., switching to different output mode).
 *
 * Cleanup steps:
 *   1. Stop hardware timer (no more NMI triggers)
 *   2. Wait for any in-progress transmission to complete
 *   3. Deallocate timer interrupt
 *   4. Reset FastPinsWithClock controller
 *   5. Clear all global state
 *
 * @note Safe to call even if initMultiSPI() was never called
 * @note Safe to call multiple times (idempotent)
 * @note Can call initMultiSPI() again after shutdown()
 * @note Does NOT restore pins to INPUT mode (pins remain in OUTPUT mode)
 *
 * Usage pattern:
 * @code
 *   void setup() {
 *       fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);
 *   }
 *
 *   void loop() {
 *       if (mode_changed) {
 *           fl::nmi::shutdown();  // Stop NMI transmission
 *           // Switch to different output mode (RMT, SPI, etc.)
 *       }
 *   }
 * @endcode
 */
void shutdown();

/**
 * @brief Get total NMI invocation count (diagnostics)
 *
 * Returns the total number of times the NMI handler has been called since
 * initialization. Useful for verifying timer frequency and debugging.
 *
 * @return Total NMI handler invocations since init
 *
 * @note This counter is incremented even if transmission is inactive
 * @note Counter may wrap around after 2^32 invocations (~50 days @ 1 kHz)
 * @note Only available if diagnostic counters are enabled in nmi_handler.cpp
 *
 * Usage:
 * @code
 *   uint32_t count1 = fl::nmi::getInvocationCount();
 *   delay(1000);  // Wait 1 second
 *   uint32_t count2 = fl::nmi::getInvocationCount();
 *   uint32_t invocations_per_second = count2 - count1;
 *   Serial.printf("NMI frequency: %lu Hz\n", invocations_per_second);
 * @endcode
 */
uint32_t getInvocationCount();

/**
 * @brief Get maximum NMI execution cycles (diagnostics)
 *
 * Returns the maximum number of CPU cycles any single NMI handler execution
 * has taken. At 240 MHz, 1 cycle = 4.17ns. Useful for profiling and ensuring
 * handler execution time is within acceptable bounds.
 *
 * @return Maximum CPU cycles for any NMI handler execution
 *
 * @note Only available if diagnostic counters are enabled in nmi_handler.cpp
 * @note Target: <100 cycles (~400ns @ 240 MHz) for good performance
 * @note If value > 250 cycles (1µs), handler is too slow
 *
 * Usage:
 * @code
 *   uint32_t max_cycles = fl::nmi::getMaxExecutionCycles();
 *   float max_ns = max_cycles * 4.17f;  // Convert to nanoseconds @ 240 MHz
 *   Serial.printf("Max NMI execution: %.1f ns (%lu cycles)\n", max_ns, max_cycles);
 * @endcode
 */
uint32_t getMaxExecutionCycles();

} // namespace nmi
} // namespace fl

#endif // ESP32 platforms
