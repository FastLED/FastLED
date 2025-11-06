/**
 * @file nmi_multispi.cpp
 * @brief Implementation of Level 7 NMI configuration API for multi-SPI
 *
 * This file implements the user-facing API for configuring and controlling
 * Level 7 NMI-driven multi-SPI parallel output on ESP32/ESP32-S3.
 *
 * Architecture:
 *   1. initMultiSPI() - Configure FastPinsWithClock and allocate timer interrupt
 *   2. startTransmission() - Start NMI-driven transmission
 *   3. isTransmissionComplete() - Poll transmission status
 *   4. shutdown() - Stop timer and cleanup
 *
 * Timer Configuration:
 *   - Uses TIMER_GROUP_0, TIMER_0 for NMI generation
 *   - Timer configured in auto-reload mode at target frequency
 *   - Interrupt allocated at Level 7 (ESP_INTR_FLAG_LEVEL7)
 *   - ESP-IDF calls xt_nmi symbol (our ASM wrapper in nmi_wrapper.S)
 *
 * IMPORTANT NOTES:
 *   - ESP-IDF v5.2.1 has known bug preventing Level 7 NMI allocation
 *   - Use ESP-IDF v5.0 or v5.1 for reliable operation
 *   - See: https://github.com/espressif/esp-idf/issues/13629
 *
 * @see src/platforms/esp/32/nmi_multispi.h for API documentation
 * @see src/platforms/esp/32/nmi_wrapper.S for ASM entry point
 * @see src/platforms/esp/32/nmi_handler.cpp for C++ handler
 * @see src/platforms/esp/32/XTENSA_INTERRUPTS.md for implementation guide
 *
 * Copyright (c) 2025 FastLED
 * Licensed under the MIT License
 */

#include "nmi_multispi.h"

// Only compile on ESP32 platforms
#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32C6) || defined(ESP32H2)

#include "FastLED.h"
#include "platforms/esp/32/fast_pins_esp32.h"

// ESP-IDF includes for timer and interrupt management
#include "driver/timer.h"
#include "esp_intr_alloc.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_periph.h"

namespace fl {
namespace nmi {

//=============================================================================
// External Global State (defined in nmi_handler.cpp)
//=============================================================================

// Multi-SPI controller (8 data pins + 1 clock pin)
extern FastPinsWithClock<8> g_nmi_spi;

// Transmission buffer pointer
extern volatile const uint8_t* g_nmi_buffer;

// Current transmission index
extern volatile size_t g_nmi_index;

// Total bytes to transmit
extern volatile size_t g_nmi_count;

// Transmission active flag
extern volatile bool g_nmi_active;

// Diagnostic counters (optional)
extern volatile uint32_t g_nmi_count_invocations;
extern volatile uint32_t g_nmi_max_cycles;

//=============================================================================
// Internal State (private to this file)
//=============================================================================

/**
 * @brief Timer interrupt handle
 *
 * Stored to allow cleanup during shutdown().
 * NULL if timer not allocated.
 */
static intr_handle_t s_timer_handle = nullptr;

/**
 * @brief Initialization status flag
 *
 * True if initMultiSPI() has been called successfully.
 * Used to prevent double-initialization.
 */
static bool s_initialized = false;

/**
 * @brief Configured timer frequency in Hz
 *
 * Stored for reference and diagnostics.
 */
static uint32_t s_frequency = 0;

//=============================================================================
// Helper Functions (internal)
//=============================================================================

/**
 * @brief Configure hardware timer for NMI generation
 *
 * Configures TIMER_GROUP_0, TIMER_0 to generate periodic interrupts at the
 * specified frequency. Timer is configured in auto-reload mode for continuous
 * operation.
 *
 * Timer configuration:
 *   - Clock source: APB_CLK (80 MHz on ESP32/ESP32-S3)
 *   - Divider: 80 (creates 1 MHz tick rate)
 *   - Counter direction: Up
 *   - Auto-reload: Enabled
 *   - Alarm value: (1000000 / frequency) - 1
 *
 * @param frequency Target interrupt frequency in Hz (e.g., 800000 for WS2812)
 * @return true if timer configured successfully, false on error
 */
static bool configureTimer(uint32_t frequency) {
    // Validate frequency range (1 kHz - 40 MHz)
    // Upper limit: ESP32 @ 240 MHz can sustain ~40 MHz ISR rate
    // Lower limit: Arbitrary (no practical use case < 1 kHz)
    if (frequency < 1000 || frequency > 40000000) {
        FL_WARN("NMI: Invalid frequency " << frequency << " Hz (range: 1 kHz - 40 MHz)");
        return false;
    }

    // Timer group and timer ID
    const timer_group_t timer_group = TIMER_GROUP_0;
    const timer_idx_t timer_idx = TIMER_0;

    // Timer configuration structure
    timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,        // Enable alarm (interrupt on match)
        .counter_en = TIMER_PAUSE,         // Start paused (enable after setup)
        .intr_type = TIMER_INTR_LEVEL,     // Level interrupt (not edge)
        .counter_dir = TIMER_COUNT_UP,     // Count up
        .auto_reload = TIMER_AUTORELOAD_EN, // Auto-reload on alarm
        .divider = 80                      // APB_CLK (80 MHz) / 80 = 1 MHz tick
    };

    // Initialize timer with configuration
    esp_err_t ret = timer_init(timer_group, timer_idx, &config);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer init failed: " << ret);
        return false;
    }

    // Set counter value to 0 (start from beginning)
    ret = timer_set_counter_value(timer_group, timer_idx, 0);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer set counter failed: " << ret);
        return false;
    }

    // Calculate alarm value (ticks per interrupt)
    // tick_rate = 1 MHz = 1,000,000 ticks/second
    // alarm_value = (ticks/second) / (interrupts/second) - 1
    // Example: 800 kHz → (1,000,000 / 800,000) - 1 = 0 (minimum alarm)
    // Example: 13.2 MHz → (1,000,000 / 13,200,000) - 1 = 0 (minimum alarm)
    //
    // PROBLEM: For frequencies > 1 MHz, alarm_value < 1 (not possible!)
    // SOLUTION: Use higher divider for high frequencies
    //
    // For now, support up to 1 MHz (WS2812, SK6812)
    // For higher speeds, need divider=1 or divider=2
    uint64_t alarm_value = (1000000 / frequency);
    if (alarm_value == 0) {
        // Frequency too high for divider=80
        // Reconfigure with lower divider
        FL_WARN("NMI: Frequency " << frequency << " Hz too high for divider=80, using divider=8");

        // Reconfigure with divider=8 (10 MHz tick rate)
        config.divider = 8;
        ret = timer_init(timer_group, timer_idx, &config);
        if (ret != ESP_OK) {
            FL_WARN("NMI: Timer reinit failed: " << ret);
            return false;
        }

        // Recalculate alarm value with 10 MHz tick rate
        alarm_value = (10000000 / frequency);
        if (alarm_value == 0) {
            FL_WARN("NMI: Frequency " << frequency << " Hz too high even for divider=8");
            return false;
        }
    }

    // Subtract 1 (timer fires when counter reaches alarm value)
    if (alarm_value > 0) {
        alarm_value--;
    }

    // Set alarm value (when to trigger interrupt)
    ret = timer_set_alarm_value(timer_group, timer_idx, alarm_value);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer set alarm failed: " << ret);
        return false;
    }

    // Enable alarm (interrupt generation)
    ret = timer_set_alarm(timer_group, timer_idx, TIMER_ALARM_EN);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer enable alarm failed: " << ret);
        return false;
    }

    FL_DBG("NMI: Timer configured at " << frequency << " Hz (alarm=" << alarm_value << ")");
    return true;
}

/**
 * @brief Allocate Level 7 NMI interrupt for timer
 *
 * Allocates a Level 7 (NMI) interrupt for TIMER_GROUP_0, TIMER_0.
 * ESP-IDF will call the xt_nmi symbol (our ASM wrapper) when the interrupt fires.
 *
 * CRITICAL: handler parameter MUST be NULL!
 * When handler is NULL, ESP-IDF looks for the xt_nmi symbol in your code.
 * Our ASM wrapper (nmi_wrapper.S) provides this symbol.
 *
 * @return true if interrupt allocated successfully, false on error
 */
static bool allocateInterrupt() {
    // Timer interrupt source (TIMER_GROUP_0, TIMER_0)
    const int intr_source = ETS_TG0_T0_LEVEL_INTR_SOURCE;

    // Interrupt flags
    // ESP_INTR_FLAG_NMI: Level 7 (NMI) priority
    // ESP_INTR_FLAG_IRAM: Interrupt runs from IRAM (required for NMI)
    const int intr_flags = ESP_INTR_FLAG_NMI | ESP_INTR_FLAG_IRAM;

    // Allocate interrupt
    // handler = NULL: ESP-IDF will call xt_nmi symbol (our ASM wrapper)
    // arg = NULL: No argument needed (uses global state)
    // ret_handle = &s_timer_handle: Store handle for cleanup
    esp_err_t ret = esp_intr_alloc(intr_source,
                                   intr_flags,
                                   nullptr,  // handler = NULL (use xt_nmi symbol)
                                   nullptr,  // arg = NULL
                                   &s_timer_handle);

    if (ret != ESP_OK) {
        FL_WARN("NMI: Interrupt allocation failed: " << ret);
        FL_WARN("NMI: Are you using ESP-IDF v5.2.1? Known bug prevents Level 7 NMI");
        FL_WARN("NMI: Try ESP-IDF v5.0 or v5.1 instead");
        FL_WARN("NMI: See: https://github.com/espressif/esp-idf/issues/13629");
        s_timer_handle = nullptr;
        return false;
    }

    FL_DBG("NMI: Level 7 interrupt allocated successfully");
    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool initMultiSPI(uint8_t clock_pin, const uint8_t* data_pins, uint32_t frequency) {
    // Prevent double-initialization
    if (s_initialized) {
        FL_WARN("NMI: Already initialized, call shutdown() first");
        return false;
    }

    // Validate parameters
    if (data_pins == nullptr) {
        FL_WARN("NMI: data_pins is NULL");
        return false;
    }

    FL_DBG("NMI: Initializing multi-SPI at " << frequency << " Hz");

    // Configure FastPinsWithClock (8 data pins + 1 clock pin)
    // This validates all 9 pins are on the same GPIO bank
    g_nmi_spi.setPins(clock_pin,
                      data_pins[0], data_pins[1], data_pins[2], data_pins[3],
                      data_pins[4], data_pins[5], data_pins[6], data_pins[7]);

    // Configure hardware timer
    if (!configureTimer(frequency)) {
        FL_WARN("NMI: Timer configuration failed");
        return false;
    }

    // Allocate Level 7 NMI interrupt
    if (!allocateInterrupt()) {
        FL_WARN("NMI: Interrupt allocation failed");

        // Cleanup timer
        timer_deinit(TIMER_GROUP_0, TIMER_0);
        return false;
    }

    // Enable timer interrupt (start generating NMI)
    esp_err_t ret = timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer enable interrupt failed: " << ret);

        // Cleanup
        if (s_timer_handle != nullptr) {
            esp_intr_free(s_timer_handle);
            s_timer_handle = nullptr;
        }
        timer_deinit(TIMER_GROUP_0, TIMER_0);
        return false;
    }

    // Start timer (begin counting)
    ret = timer_start(TIMER_GROUP_0, TIMER_0);
    if (ret != ESP_OK) {
        FL_WARN("NMI: Timer start failed: " << ret);

        // Cleanup
        timer_disable_intr(TIMER_GROUP_0, TIMER_0);
        if (s_timer_handle != nullptr) {
            esp_intr_free(s_timer_handle);
            s_timer_handle = nullptr;
        }
        timer_deinit(TIMER_GROUP_0, TIMER_0);
        return false;
    }

    // Initialize global state
    g_nmi_buffer = nullptr;
    g_nmi_index = 0;
    g_nmi_count = 0;
    g_nmi_active = false;
    g_nmi_count_invocations = 0;
    g_nmi_max_cycles = 0;

    // Mark initialized
    s_initialized = true;
    s_frequency = frequency;

    FL_DBG("NMI: Multi-SPI initialized successfully");
    return true;
}

bool startTransmission(const uint8_t* buffer, size_t length) {
    // Check if initialized
    if (!s_initialized) {
        FL_WARN("NMI: Not initialized, call initMultiSPI() first");
        return false;
    }

    // Check if already transmitting
    if (g_nmi_active) {
        FL_WARN("NMI: Transmission already active");
        return false;
    }

    // Validate parameters
    if (buffer == nullptr || length == 0) {
        FL_WARN("NMI: Invalid buffer or length");
        return false;
    }

    // Check if buffer is in DRAM (rough heuristic)
    // DRAM address range on ESP32: 0x3FF00000 - 0x40000000 (data RAM)
    // Flash address range: 0x40000000 - 0x40400000 (instruction cache)
    // This is a best-effort check, not foolproof
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
    bool likely_dram = (addr >= 0x3FF00000 && addr < 0x40000000) ||
                       (addr >= 0x3F400000 && addr < 0x3F500000);  // Also check external RAM

    if (!likely_dram) {
        FL_WARN("NMI: Buffer may not be in DRAM (address: 0x" << (void*)buffer << ")");
        FL_WARN("NMI: Use DRAM_ATTR for global buffers or stack variables");
        // Don't fail, just warn (might be false positive)
    }

    FL_DBG("NMI: Starting transmission (" << length << " bytes)");

    // Set up transmission state
    g_nmi_buffer = buffer;
    g_nmi_index = 0;
    g_nmi_count = length;
    g_nmi_active = true;

    // Timer is already running (started in initMultiSPI)
    // NMI handler will begin transmitting on next timer interrupt

    return true;
}

bool isTransmissionComplete() {
    // If not initialized, consider "complete" (never started)
    if (!s_initialized) {
        return true;
    }

    // Check active flag
    // NMI handler sets this to false when transmission completes
    return !g_nmi_active;
}

void shutdown() {
    // If not initialized, nothing to do
    if (!s_initialized) {
        return;
    }

    FL_DBG("NMI: Shutting down");

    // Stop timer (no more interrupts)
    timer_pause(TIMER_GROUP_0, TIMER_0);

    // Disable timer interrupt
    timer_disable_intr(TIMER_GROUP_0, TIMER_0);

    // Free interrupt handle
    if (s_timer_handle != nullptr) {
        esp_intr_free(s_timer_handle);
        s_timer_handle = nullptr;
    }

    // Deinitialize timer
    timer_deinit(TIMER_GROUP_0, TIMER_0);

    // Clear global state
    g_nmi_buffer = nullptr;
    g_nmi_index = 0;
    g_nmi_count = 0;
    g_nmi_active = false;

    // Mark uninitialized
    s_initialized = false;
    s_frequency = 0;

    FL_DBG("NMI: Shutdown complete");
}

uint32_t getInvocationCount() {
    return g_nmi_count_invocations;
}

uint32_t getMaxExecutionCycles() {
    return g_nmi_max_cycles;
}

} // namespace nmi
} // namespace fl

#endif // ESP32 platforms
