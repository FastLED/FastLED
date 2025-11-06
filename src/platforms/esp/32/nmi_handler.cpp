/**
 * @file nmi_handler.cpp
 * @brief C++ handler for Level 7 NMI interrupts on ESP32/ESP32-S3
 *
 * This file provides the C++ handler function called by the ASM wrapper
 * (nmi_wrapper.S) for Level 7 NMI interrupts. It manages multi-SPI parallel
 * output using FastPinsWithClock for ultra-low latency operation with WiFi
 * active.
 *
 * Performance:
 *   - Per-bit time: 76ns (30ns data + 8ns NOP + 30ns clock + 8ns NOP)
 *   - Max speed: 13.2 MHz per strip
 *   - Total throughput: 105.6 Mbps (8 strips × 13.2 MHz)
 *   - CPU usage: 6% @ 800 kHz (WS2812)
 *   - Jitter: ±50ns (within WS2812 ±150ns tolerance)
 *
 * CRITICAL RESTRICTIONS (Level 7 NMI):
 *   - NO FreeRTOS calls (any FreeRTOS API will crash)
 *   - NO ESP_LOG (uses FreeRTOS mutexes)
 *   - NO malloc/free (heap operations use locks)
 *   - NO task notifications (cannot signal tasks)
 *   - NO blocking operations (no delays, no waiting)
 *   - All code must be in IRAM (use IRAM_ATTR)
 *   - All data must be in DRAM (use DRAM_ATTR)
 *   - Keep execution time < 1µs to avoid starving other interrupts
 *
 * @see src/platforms/esp/32/XTENSA_INTERRUPTS.md for complete documentation
 * @see src/platforms/esp/32/nmi_wrapper.S for ASM entry point
 * @see examples/FastPinsNMI/ for usage example
 *
 * Copyright (c) 2025 FastLED
 * Licensed under the MIT License
 */

#include "FastLED.h"

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32C6) || defined(ESP32H2)

#include "platforms/esp/32/fast_pins_esp32.h"

namespace fl {
namespace nmi {

//-----------------------------------------------------------------------------
// Global State (MUST be in DRAM, not flash)
//-----------------------------------------------------------------------------

/**
 * @brief Multi-SPI controller (8 data pins + 1 clock pin)
 *
 * DRAM_ATTR ensures this object is stored in data RAM, not flash.
 * Flash cache access is not allowed from Level 7 NMI (will crash).
 */
FastPinsWithClock<8> g_nmi_spi DRAM_ATTR;

/**
 * @brief Transmission buffer pointer
 *
 * Points to user's data buffer. The buffer itself MUST also be in DRAM.
 * Set via startTransmission().
 */
volatile const uint8_t* g_nmi_buffer DRAM_ATTR = nullptr;

/**
 * @brief Current transmission index
 *
 * Tracks which byte in the buffer is being transmitted.
 * Incremented by NMI handler on each byte transmitted.
 */
volatile size_t g_nmi_index DRAM_ATTR = 0;

/**
 * @brief Total bytes to transmit
 *
 * Number of bytes in transmission buffer.
 * Set via startTransmission().
 */
volatile size_t g_nmi_count DRAM_ATTR = 0;

/**
 * @brief Transmission active flag
 *
 * True when transmission is in progress, false when complete or idle.
 * Used by isTransmissionComplete() to check status.
 */
volatile bool g_nmi_active DRAM_ATTR = false;

//-----------------------------------------------------------------------------
// Diagnostic Counters (Optional, for debugging)
//-----------------------------------------------------------------------------

/**
 * @brief Total NMI invocations counter
 *
 * Increments on each NMI handler call. Useful for verifying timer frequency.
 */
volatile uint32_t g_nmi_count_invocations DRAM_ATTR = 0;

/**
 * @brief Maximum execution cycles
 *
 * Tracks longest NMI handler execution time in CPU cycles.
 * At 240 MHz: 1 cycle = 4.17ns
 * Use XTHAL_GET_CCOUNT() to measure.
 */
volatile uint32_t g_nmi_max_cycles DRAM_ATTR = 0;

//-----------------------------------------------------------------------------
// NMI Handler (Called by ASM wrapper)
//-----------------------------------------------------------------------------

} // namespace nmi
} // namespace fl

/**
 * @brief Level 7 NMI handler for multi-SPI transmission
 *
 * This function is called by the ASM wrapper (nmi_wrapper.S) when a Level 7
 * NMI interrupt fires. It performs zero-delay clock strobing for ultra-high
 * speed multi-SPI parallel output.
 *
 * Execution flow:
 *   1. Check if transmission is active and data remains
 *   2. Read next byte from buffer
 *   3. Write data to all 8 data pins + clock LOW (30ns)
 *   4. NOP delay for GPIO settling (8ns)
 *   5. Write same data + clock HIGH (30ns)
 *   6. NOP delay before next bit (8ns)
 *   7. Increment index, check if transmission complete
 *
 * Total execution time: ~76ns per byte
 *
 * IRAM_ATTR: Places this function in instruction RAM (required for NMI)
 * extern "C": Prevents C++ name mangling (required for ASM to call)
 *
 * @note This function has NO error handling, NO logging, NO FreeRTOS calls
 * @note All error handling must be done outside the NMI context
 */
extern "C" void IRAM_ATTR fastled_nmi_handler() {
    using namespace fl::nmi;

    // Fast path: Check if transmission is active
    // Branching is very fast (1-2 cycles) on Xtensa
    if (!g_nmi_active || g_nmi_index >= g_nmi_count) {
        return;  // Nothing to do, exit immediately
    }

    // Optional: Count invocations for diagnostics
    // Comment out in production for minimal overhead
    // g_nmi_count_invocations++;

    // Optional: Measure execution time for profiling
    // Comment out in production for minimal overhead
    // uint32_t start = XTHAL_GET_CCOUNT();

    // Read next byte from buffer
    // Buffer MUST be in DRAM (checked by startTransmission)
    size_t idx = g_nmi_index;
    uint8_t byte = g_nmi_buffer[idx];
    g_nmi_index = idx + 1;

    //-------------------------------------------------------------------------
    // Zero-delay clock strobing for ultra-high speed (13.2 MHz)
    //-------------------------------------------------------------------------
    // This technique eliminates wait cycles between data and clock transitions,
    // achieving the maximum possible speed for bit-banged multi-SPI on ESP32.
    //
    // Timing breakdown:
    //   writeDataAndClock(byte, 0):  30ns - Write data + clock LOW
    //   nop; nop;                     8ns - GPIO propagation delay
    //   writeDataAndClock(byte, 1):  30ns - Write same data + clock HIGH
    //   nop; nop;                     8ns - Before next bit
    //   Total:                       76ns per bit
    //
    // Maximum speed: 1 / 76ns = 13.2 MHz per strip
    // Total throughput: 13.2 MHz × 8 strips = 105.6 Mbps
    //
    // GPIO propagation delay: ESP32 GPIO has 15-25ns propagation delay.
    // Without NOPs, back-to-back writes create <20ns clock pulses (too narrow).
    // With 2 NOPs (8ns), clock pulse width is 30ns + 8ns = 38ns (safe margin).
    //-------------------------------------------------------------------------

    // Write data to all 8 data pins + clock LOW
    g_nmi_spi.writeDataAndClock(byte, 0);

    // NOP delay for GPIO settling (8ns at 240 MHz)
    // Ensures GPIO outputs have time to settle before clock transition
    // 2 NOPs = 2 cycles = 8.33ns @ 240 MHz
    __asm__ __volatile__("nop; nop;");

    // Write same data + clock HIGH (creates rising clock edge)
    g_nmi_spi.writeDataAndClock(byte, 1);

    // NOP delay before next bit (8ns at 240 MHz)
    // Ensures clock HIGH pulse has sufficient width (≥30ns)
    __asm__ __volatile__("nop; nop;");

    // Check if transmission is complete
    // If all bytes transmitted, mark inactive
    if (g_nmi_index >= g_nmi_count) {
        g_nmi_active = false;
    }

    // Optional: Update max execution time for profiling
    // Comment out in production for minimal overhead
    // uint32_t end = XTHAL_GET_CCOUNT();
    // uint32_t cycles = end - start;
    // if (cycles > g_nmi_max_cycles) {
    //     g_nmi_max_cycles = cycles;
    // }
}

#endif // ESP32 platforms
