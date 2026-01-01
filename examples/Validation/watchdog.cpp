// examples/Validation/watchdog.cpp
//
// ESP32-C6 Watchdog Timer Implementation
//
// Provides a 5-second proof-of-life watchdog that automatically monitors
// the Arduino loop() task. No manual feeding required - the ESP32 framework
// handles watchdog feeding automatically as long as loop() keeps executing.

#include "watchdog.h"
#include <FastLED.h>
#include "fl/log.h"

#ifdef FL_IS_ESP_32C6

#include "esp_task_wdt.h"
#include "soc/usb_serial_jtag_reg.h"
#include "esp_system.h"

namespace validation {

void setup_watchdog() {
    FL_WARN("\n[WATCHDOG] Configuring ESP32-C6 custom 5-second watchdog");

    // Deinitialize default watchdog first to clear any existing configuration
    esp_task_wdt_deinit();

    // Configure 5-second watchdog with reset on timeout
    // idle_core_mask = (1 << 0) monitors the main loop task on core 0
    esp_task_wdt_config_t config = {
        .timeout_ms = 5000,           // 5 second timeout
        .idle_core_mask = (1 << 0),   // Monitor idle task on core 0 (main loop)
        .trigger_panic = true         // Trigger panic and reset on timeout
    };

    esp_err_t err = esp_task_wdt_init(&config);
    if (err != ESP_OK) {
        FL_WARN("[WATCHDOG] Failed to initialize (error: " << err << ")");
        return;
    }

    FL_WARN("[WATCHDOG] ✓ 5-second watchdog active with reset on timeout");
    FL_WARN("[WATCHDOG] ℹ️  Automatically monitors loop() execution - no manual feeding needed");
}

} // namespace validation

// ESP32 panic hook to perform safe USB disconnect before reset
// This runs when the watchdog triggers a panic
extern "C" void esp_panic_handler_reconfigure_wdts(void) {
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

#else // Non-ESP32-C6 platforms

namespace validation {

void setup_watchdog() {
    // No-op on non-ESP32-C6 platforms
}

} // namespace validation

#endif // FL_IS_ESP_32C6
