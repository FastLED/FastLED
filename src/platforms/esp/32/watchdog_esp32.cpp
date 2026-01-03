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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF version detection (handles legacy platforms automatically)
// Provides: ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_5_OR_HIGHER, etc.
#include "platforms/esp/esp_version.h"

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

// Forward declaration of shutdown handler (ESP-IDF v5.0+)
#if ESP_IDF_VERSION_5_OR_HIGHER
static void watchdog_shutdown_handler(void);
#endif

void watchdog_setup(uint32_t timeout_ms,
                    watchdog_callback_t callback,
                    void* user_data) {
    FL_DBG("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog");

    // Store user callback and data for panic handler (defined in init.cpp)
    detail::s_user_callback = callback;
    detail::s_user_data = user_data;

#if ESP_IDF_VERSION_5_OR_HIGHER
    // ESP-IDF v5.0+: Register shutdown handler for safe USB disconnect before reset
    // This is the proper ESP-IDF API for pre-reset cleanup
    // (panic handler override no longer possible - panic API made private)
    esp_register_shutdown_handler(watchdog_shutdown_handler);
#endif
    // ESP-IDF v4.x and earlier: Uses weak symbol override below (esp_panic_handler_reconfigure_wdts)

    // Deinitialize default watchdog first to clear any existing configuration
    // ONLY if FreeRTOS scheduler is running (esp_task_wdt_deinit uses FreeRTOS APIs)
    // xTaskGetSchedulerState returns: 0=suspended, 1=not started, 2=running
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        esp_task_wdt_deinit();
    }

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

// Common USB disconnect logic shared between v4 and v5
static void perform_safe_usb_disconnect(const char* handler_name) {
    // Call user callback first if provided (from watchdog setup)
    if (fl::detail::s_user_callback != nullptr) {
        fl::detail::s_user_callback(fl::detail::s_user_data);
    }

    // Print handler message
    FL_DBG("\n[" << handler_name << "] System reset detected - performing safe USB disconnect");

#if HAS_USB_SERIAL_JTAG
    // Force USB disconnect to prevent phantom device on Windows
    // This only applies to chips with native USB Serial JTAG (S3, C3, C6, H2)
    // Clear D+ pullup to signal USB disconnect
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);

    // Pull D+ low to ensure Windows detects disconnect
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);

    // Give Windows time to detect disconnect (150ms minimum)
    // Note: This delay is safe in reset handler context
    fl::delayMicroseconds(150000);  // 150ms = 150000 microseconds

    FL_DBG("[" << handler_name << "] ✓ USB disconnected - proceeding with reset");
#else
    // Original ESP32, ESP32-S2: No native USB Serial JTAG
    // Uses external USB-to-UART chip (CP2102, CH340, etc.) which auto-resets
    FL_DBG("[" << handler_name << "] No USB Serial JTAG hardware - using default reset behavior");
#endif
}

#if ESP_IDF_VERSION_5_OR_HIGHER
// ESP-IDF v5.0+ shutdown handler (uses official shutdown handler API)
// This runs when the system is shutting down (watchdog, panic, or esp_restart)
//
// IMPORTANT: ESP-IDF v5.0+ made panic handler functions private, so we use
// the official shutdown handler API (esp_register_shutdown_handler).
// See: esp-idf/components/esp_system/include/esp_system.h
static void watchdog_shutdown_handler(void) {
    perform_safe_usb_disconnect("SHUTDOWN v5");
}
#else
// ESP-IDF v4.x and earlier panic handler override (weak symbol override)
// This runs when the watchdog triggers a panic or any system panic occurs
//
// IMPORTANT: ESP-IDF v4.x and earlier allow overriding weak symbols in panic handler.
// This function signature must match ESP-IDF v4.x expectations.
// See: esp-idf v4.x components/esp_system/panic.c
extern "C" void esp_panic_handler_reconfigure_wdts(void) {
    perform_safe_usb_disconnect("PANIC v4");
}
#endif

#endif // ESP32 (compiler builtin guard)
