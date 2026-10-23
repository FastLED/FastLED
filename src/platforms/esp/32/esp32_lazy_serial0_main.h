#pragma once

// IWYU pragma: private

// Sketch-side half of the Arduino-ESP32 startup replacement (#4892). These
// weak definitions must sit in the sketch's objects, which the linker reads
// before the core archive; that is what keeps the core's main.o (and through
// it Serial0) out of the image. libFastLED.a is read after the core, so they
// cannot live there. Nothing a sketch uses is redeclared here.

// ok no namespace fl

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/32/esp32_lazy_serial0_gate.h"

#if FL_ESP32_ARDUINO_LAZY_SERIAL0
// IWYU pragma: begin_keep
#include <freertos/FreeRTOS.h>  // ok include
#include <freertos/task.h>      // ok include
// IWYU pragma: end_keep

extern "C" void fl_esp32_arduino_app_main(void) FL_NO_EXCEPT;

// Read by esp32-hal-misc.c (loop WDT) and esp32-hal-rmt.c.
FL_LINK_WEAK TaskHandle_t loopTaskHandle = nullptr;
FL_LINK_WEAK bool loopTaskWDTEnabled = false;

extern "C" FL_LINK_WEAK void app_main(void) FL_NO_EXCEPT {
    fl_esp32_arduino_app_main();
}
#endif
