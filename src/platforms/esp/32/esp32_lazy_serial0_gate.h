#pragma once

// IWYU pragma: private

// Decides whether FastLED replaces the Arduino-ESP32 startup (main.cpp) so an
// image that never uses HardwareSerial does not link Serial0. See #4892/#4911.
//
// Stock main.cpp calls Serial0.setPins() before setup(). The core exposes no
// hook or setting to skip it from a library: NO_GLOBAL_SERIAL only works when
// the core itself is compiled with it. So FastLED puts weak copies of the
// startup symbols into the sketch's own objects and the linker never extracts
// main.o. If anything still extracts main.o, its strong symbols win and startup
// is exactly stock.
//
// Enabled only for an Arduino-ESP32 core build (never plain ESP-IDF, never any
// other platform) whose main.cpp matches FastLED's copy: 3.3.2-3.3.12. Native
// USB-on-boot builds (ARDUINO_USB_MODE=0 with CDC/MSC/DFU/USB on boot) keep the
// stock startup: it calls into the TinyUSB class objects, which FastLED's copy
// may not include.
// Opt out with -DFL_ESP32_STOCK_ARDUINO_MAIN=1.

// ok no namespace fl

#include "fl/stl/has_include.h"
#include "platforms/esp/is_esp.h"  // ok platform headers

#ifndef FL_ESP32_STOCK_ARDUINO_MAIN
#define FL_ESP32_STOCK_ARDUINO_MAIN 0
#endif

#define FL_ESP32_ARDUINO_LAZY_SERIAL0 0

#if defined(FL_IS_ESP32) && defined(ARDUINO_ARCH_ESP32) && !FL_ESP32_STOCK_ARDUINO_MAIN
#if FL_HAS_INCLUDE(<esp_arduino_version.h>) && FL_HAS_INCLUDE(<sdkconfig.h>)
// IWYU pragma: begin_keep
#include <sdkconfig.h>             // ok include
#include <esp_arduino_version.h>   // ok include
// IWYU pragma: end_keep
#if defined(CONFIG_AUTOSTART_ARDUINO) && CONFIG_AUTOSTART_ARDUINO && \
    !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL) && \
    !((ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MSC_ON_BOOT || \
       ARDUINO_USB_DFU_ON_BOOT || ARDUINO_USB_ON_BOOT) && !ARDUINO_USB_MODE) && \
    ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 2) && \
    ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(3, 3, 12)
#undef FL_ESP32_ARDUINO_LAZY_SERIAL0
#define FL_ESP32_ARDUINO_LAZY_SERIAL0 1
#endif
#endif
#endif
