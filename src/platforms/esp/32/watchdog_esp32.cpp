// src/platforms/esp/32/watchdog_esp32.cpp
//
// ESP32 Watchdog Timer Implementation
//
// FIXES WINDOWS USB DISCONNECT ISSUE. If the watchdog fires normally then the USB line
// is not held low. This causes Windows to think the device is dead and won't reconnect.
// This implementation fixes this by overriding the panic handler to perform a safe
// USB disconnect sequence before reset.
//
// Provides a configurable proof-of-life watchdog that automatically monitors
// the Arduino loop() task. No manual feeding required - the ESP32 framework
// handles watchdog feeding automatically as long as loop() keeps executing.

#include "watchdog_esp32.h"

// Platform guard using compiler builtins
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include "fl/dbg.h"
#include "esp_task_wdt.h"
#include "esp_system.h"

// USB Serial JTAG registers - only available on ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
// Original ESP32, ESP32-S2 do not have USB Serial JTAG hardware
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
    #include "soc/usb_serial_jtag_reg.h"
    #define HAS_USB_SERIAL_JTAG 1
#else
    #define HAS_USB_SERIAL_JTAG 0
#endif

namespace fl {
namespace detail {

// Static storage for user callback and data
// Shared between watchdog setup and panic handler
static watchdog_callback_t s_user_callback = nullptr;
static void* s_user_data = nullptr;

} // namespace detail

void watchdog_setup(uint32_t timeout_ms,
                    watchdog_callback_t callback,
                    void* user_data) {
    FL_DBG("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog");

    // Store user callback and data for panic handler (defined in init.cpp)
    detail::s_user_callback = callback;
    detail::s_user_data = user_data;

    // Deinitialize default watchdog first to clear any existing configuration
    esp_task_wdt_deinit();

    // Configure watchdog with reset on timeout
    // idle_core_mask = (1 << 0) monitors the main loop task on core 0
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1 << 0),   // Monitor idle task on core 0 (main loop)
        .trigger_panic = true          // Trigger panic and reset on timeout
    };

    esp_err_t err = esp_task_wdt_init(&config);
    if (err != ESP_OK) {
        FL_DBG("[WATCHDOG] Failed to initialize (error: " << err << ")");
        return;
    }

    FL_DBG("[WATCHDOG] ✓ " << timeout_ms << "ms watchdog active with reset on timeout");
    if (callback != nullptr) {
        FL_DBG("[WATCHDOG] ℹ️  User callback registered");
    }
    FL_DBG("[WATCHDOG] ℹ️  Automatically monitors loop() execution - no manual feeding needed");
}

} // namespace fl

// ESP32 panic hook to perform safe USB disconnect before reset
// This runs when the watchdog triggers a panic or any system panic occurs
//
// IMPORTANT: This function overrides a weak symbol in ESP-IDF's panic handler.
// The function signature and behavior must match ESP-IDF expectations.
// See: esp-idf/components/esp_system/panic.c
extern "C" void esp_panic_handler_reconfigure_wdts(void) {
    // Call user callback first if provided (from watchdog setup)
    if (fl::detail::s_user_callback != nullptr) {
        fl::detail::s_user_callback(fl::detail::s_user_data);
    }

    // Print watchdog/panic message
    FL_DBG("\n[PANIC HANDLER] System panic detected - performing safe USB reset");

#if HAS_USB_SERIAL_JTAG
    // Force USB disconnect to prevent phantom device on Windows
    // This only applies to chips with native USB Serial JTAG (S3, C3, C6, H2)
    // Clear D+ pullup to signal USB disconnect
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);

    // Pull D+ low to ensure Windows detects disconnect
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);

    // Give Windows time to detect disconnect (150ms minimum)
    // Note: This delay is safe in panic handler context
    delay(150);

    FL_DBG("[PANIC HANDLER] ✓ USB disconnected - proceeding with reset");
#else
    // Original ESP32, ESP32-S2: No native USB Serial JTAG
    // Uses external USB-to-UART chip (CP2102, CH340, etc.) which auto-resets
    FL_DBG("[PANIC HANDLER] No USB Serial JTAG hardware - using default reset behavior");
#endif
}

#endif // ESP32 (compiler builtin guard)
