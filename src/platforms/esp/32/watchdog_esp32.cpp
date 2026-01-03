// src/platforms/esp/32/watchdog_esp32.cpp
//
// ESP32 Watchdog Timer Implementation - Trampoline Dispatcher
//
// FIXES WINDOWS USB DISCONNECT ISSUE. If the watchdog fires normally then the USB line
// is not held low. This causes Windows to think the device is dead and won't reconnect.
// This implementation fixes this by overriding the panic handler to perform a safe
// USB disconnect sequence before reset.
//
// Provides a configurable proof-of-life watchdog that automatically monitors
// the Arduino loop() task. No manual feeding required - the ESP32 framework
// handles watchdog feeding automatically as long as loop() keeps executing.
//
// This file dispatches to IDF version-specific implementations:
// - watchdog_esp32_idf4.hpp: ESP-IDF v4.x (weak symbol override)
// - watchdog_esp32_idf5.hpp: ESP-IDF v5.x (official shutdown handler API)

// ok no namespace fl (trampoline dispatcher - namespace defined in included implementations)

#include "watchdog_esp32.h"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include "platforms/esp/esp_version.h"

// Dispatch to IDF version-specific implementation
#if ESP_IDF_VERSION_5_OR_HIGHER
    #include "watchdog_esp32_idf5.hpp"
#else
    #include "watchdog_esp32_idf4.hpp"
#endif

#endif // ESP32
