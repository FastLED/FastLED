// src/platforms/esp/32/watchdog_esp32.cpp
//
// ESP32 Watchdog Timer Implementation
//
// Provides a configurable proof-of-life watchdog that automatically monitors
// the Arduino loop() task. No manual feeding required - the ESP32 framework
// handles watchdog feeding automatically as long as loop() keeps executing.

#include "watchdog_esp32.h"

#ifdef FL_IS_ESP32

#include "fl/log.h"
#include "esp_task_wdt.h"
#include "soc/usb_serial_jtag_reg.h"
#include "esp_system.h"

namespace fl {

// Static storage for user callback and data
static watchdog_callback_t s_user_callback = nullptr;
static void* s_user_data = nullptr;

void watchdog_setup(uint32_t timeout_ms,
                    watchdog_callback_t callback,
                    void* user_data) {
    FL_WARN("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog");

    // Store user callback and data for ISR handler
    s_user_callback = callback;
    s_user_data = user_data;

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
        FL_WARN("[WATCHDOG] Failed to initialize (error: " << err << ")");
        return;
    }

    FL_WARN("[WATCHDOG] ✓ " << timeout_ms << "ms watchdog active with reset on timeout");
    if (callback != nullptr) {
        FL_WARN("[WATCHDOG] ℹ️  User callback registered");
    }
    FL_WARN("[WATCHDOG] ℹ️  Automatically monitors loop() execution - no manual feeding needed");
}

} // namespace fl

// ESP32 panic hook to perform safe USB disconnect before reset
// This runs when the watchdog triggers a panic
extern "C" void esp_panic_handler_reconfigure_wdts(void) {
    // Call user callback first if provided
    if (fl::s_user_callback != nullptr) {
        fl::s_user_callback(fl::s_user_data);
    }

    // Print watchdog fired message
    FL_WARN("\n[WATCHDOG FIRED] Watchdog timeout - performing safe reset");

    // Force USB disconnect to prevent phantom device on Windows
    // Clear D+ pullup to signal USB disconnect
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);

    // Pull D+ low to ensure Windows detects disconnect
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);

    // Give Windows time to detect disconnect (150ms minimum)
    // Note: This delay is safe in panic handler context
    delay(150);

    FL_WARN("[WATCHDOG FIRED] ✓ USB disconnected - proceeding with reset");
}

#endif // FL_IS_ESP32
