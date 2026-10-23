#pragma once

// IWYU pragma: private

// Library half of the Arduino-ESP32 startup replacement (#4892). A copy of
// loopTask()/app_main() from cores/esp32/main.cpp (identical in 3.3.2-3.3.12)
// that differs only where it would name Serial0: the UART0 default pins are
// set when HardwareSerial is linked, and the HWCDC event still runs when it is
// not. Only the C HAL is included; Arduino objects are reached through
// address-only weak references, so nothing here extracts a core object.
//
// A weak redeclaration weakens every reference in the same object file, so no
// other code in this unity object may use the symbols declared weak below
// (enforced by ci/tests/test_esp32_lazy_serial0.py).

// ok no namespace fl

#include "platforms/esp/32/esp32_lazy_serial0_gate.h"

#if FL_ESP32_ARDUINO_LAZY_SERIAL0
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: begin_keep
#include <esp32-hal.h>          // ok include
#include <chip-debug-report.h>  // ok include
#include <esp_task_wdt.h>       // ok include
#include <soc/rtc.h>            // ok include
#include <soc/uart_pins.h>      // ok include
// IWYU pragma: end_keep

#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

// HardwareSerial.h's default UART0 pins; no variant overrides them, and they
// equal the SDK's U0RXD/U0TXD on every target.
#ifndef SOC_RX0
#define SOC_RX0 U0RXD_GPIO_NUM
#endif
#ifndef SOC_TX0
#define SOC_TX0 U0TXD_GPIO_NUM
#endif

void setup(void);
void loop(void);

extern TaskHandle_t loopTaskHandle;
extern bool loopTaskWDTEnabled;

// Sketch overrides (SET_LOOP_TASK_STACK_SIZE and friends). Stock defines weak
// defaults in main.o; a strong reference here would extract it.
FL_LINK_WEAK size_t getArduinoLoopTaskStackSize(void);
FL_LINK_WEAK bool shouldPrintChipDebugReport(void);
FL_LINK_WEAK fl::u64 getArduinoSetupWaitTime_ms(void);

// Defined in HardwareSerial.cpp: non-null exactly when Serial0 is linked,
// and HardwareSerial.cpp in turn links the UART HAL's uartSetPins.
FL_LINK_WEAK void serialEventRun(void);
extern "C" FL_LINK_WEAK bool uartSetPins(fl::u8 uart_num, fl::i8 rxPin, fl::i8 txPin,
                                         fl::i8 ctsPin, fl::i8 rtsPin);

#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT && SOC_USB_SERIAL_JTAG_SUPPORTED
// HWCDC.cpp's instance and its available(); the call passes the instance as
// `this`, exactly as the member call does.
#define FL_ESP32_LAZY_SERIAL0_HWCDC 1
FL_LINK_WEAK void HWCDCSerialEvent(void);
// Distinct name, same symbol: HWCDC.h may already declare the real object here.
extern "C" FL_LINK_WEAK char flHwcdcSerialObject __asm__("HWCDCSerial");
extern "C" FL_LINK_WEAK int flHwcdcAvailable(void* self) __asm__("_ZN5HWCDC9availableEv");
#else
#define FL_ESP32_LAZY_SERIAL0_HWCDC 0
#endif

namespace {

#if CONFIG_FREERTOS_UNICORE
void flYieldIfNecessary() FL_NO_EXCEPT {
    static fl::u64 lastYield = 0;
    fl::u64 now = millis();
    if ((now - lastYield) > 2000) {
        lastYield = now;
        vTaskDelay(5);
    }
}
#endif

bool flShouldPrintChipDebugReport() FL_NO_EXCEPT {
    return shouldPrintChipDebugReport ? shouldPrintChipDebugReport() : false;
}

void flSerialEventRun() FL_NO_EXCEPT {
    if (serialEventRun) {
        serialEventRun();  // Stock path; also dispatches the HWCDC event.
        return;
    }
#if FL_ESP32_LAZY_SERIAL0_HWCDC
    if (HWCDCSerialEvent && &flHwcdcSerialObject && flHwcdcAvailable &&
        flHwcdcAvailable(&flHwcdcSerialObject) > 0) {
        HWCDCSerialEvent();
    }
#endif
}

void flLoopTask(void*) FL_NO_EXCEPT {
    delay(getArduinoSetupWaitTime_ms ? getArduinoSetupWaitTime_ms() : 0);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    printBeforeSetupInfo();
#else
    if (flShouldPrintChipDebugReport()) {
        printBeforeSetupInfo();
    }
#endif
    // Stock: Serial0.setPins(gpioNumberToDigitalPin(SOC_RX0), ...TX0). setPins
    // maps back to GPIO numbers, so this is uartSetPins(0, SOC_RX0, SOC_TX0).
    // The parentheses keep io_pin_remap.h's macro from remapping them again.
    if (serialEventRun && uartSetPins) {
        (uartSetPins)(0, SOC_RX0, SOC_TX0, -1, -1);
    }
    setup();
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    printAfterSetupInfo();
#else
    if (flShouldPrintChipDebugReport()) {
        printAfterSetupInfo();
    }
#endif
    for (;;) {
#if CONFIG_FREERTOS_UNICORE
        flYieldIfNecessary();
#endif
        if (loopTaskWDTEnabled) {
            esp_task_wdt_reset();
        }
        loop();
        flSerialEventRun();
    }
}

}  // namespace

extern "C" void fl_esp32_arduino_app_main(void) FL_NO_EXCEPT {
#ifdef F_XTAL_MHZ
#if !CONFIG_IDF_TARGET_ESP32S2
    rtc_clk_xtal_freq_update((rtc_xtal_freq_t)F_XTAL_MHZ);
    rtc_clk_cpu_freq_set_xtal();
#endif
#endif
#ifdef F_CPU
    setCpuFrequencyMhz(F_CPU / 1000000);
#endif
    // Native USB-on-boot startup needs the USB class objects; the gate leaves
    // those builds on the stock main.cpp.
    loopTaskWDTEnabled = false;
    initArduino();
    xTaskCreateUniversal(flLoopTask, "loopTask",
                         getArduinoLoopTaskStackSize ? getArduinoLoopTaskStackSize()
                                                     : ARDUINO_LOOP_STACK_SIZE,
                         NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);
}

#endif  // FL_ESP32_ARDUINO_LAZY_SERIAL0
