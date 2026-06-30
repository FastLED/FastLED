// AutoResearchPlatform.h - Platform-specific defaults for AutoResearch example
//
// Encapsulates all platform #ifdefs so AutoResearch.ino stays clean.

#pragma once

#include "platforms/is_platform.h"

namespace autoresearch {

// Default TX pin for each platform
//
// NOTE: Avoid GPIO0/1/3 on classic ESP32 — GPIO1 is UART0 TX (console output),
// GPIO3 is UART0 RX, and GPIO0 is a boot-strap pin. Toggling GPIO1 from
// AutoResearch's GPIO baseline test (testRxChannel) silences the console and
// causes FastLED #3452 (RPC silence). Use GPIO 18/19 which are general-purpose
// and don't collide with peripheral fixed-function muxing on the WROOM module.
constexpr int defaultTxPin() {
#if defined(FL_IS_STUB)
    return 1;  // Stub: TX and RX same pin — NativeRxDevice monitors TX output directly
#elif defined(FL_IS_ESP_32S3)
    return 1;  // ESP32-S3: Connect GPIO 1 (TX) to GPIO 2 (RX) with jumper wire
#elif defined(FL_IS_ESP_32S2)
    return 1;  // ESP32-S2
#elif defined(FL_IS_ESP_32C6)
    return 1;  // ESP32-C6 (RISC-V)
#elif defined(FL_IS_ESP_32C3)
    return 1;  // ESP32-C3 (RISC-V)
#elif defined(FL_IS_ESP_32P4)
    return 5;  // ESP32-P4: Connect GPIO 5 (TX) to GPIO 6 (RX) with jumper wire
#elif defined(FL_IS_TEENSY_4X)
    return 1;  // Teensy 4.x: any GPIO works for TX
#elif defined(FL_IS_ESP_32DEV)
    return 18;  // ESP32 (classic): avoid GPIO0/1/3 (boot strap + UART0). See #3452.
#else
    return 1;  // Other variants
#endif
}

// Default RX pin for each platform
//
// NOTE: See defaultTxPin() above for the classic-ESP32 GPIO0/1/3 caveat — the
// default RX pin must also avoid those. GPIO 19 is a safe partner for GPIO 18.
constexpr int defaultRxPin() {
#if defined(FL_IS_STUB)
    return 1;  // Stub: same pin as TX — internal loopback
#elif defined(FL_IS_ESP_32S3)
    return 2;  // ESP32-S3: jumper wire from GPIO 1
#elif defined(FL_IS_ESP_32S2)
    return 0;
#elif defined(FL_IS_ESP_32C6)
    return 0;
#elif defined(FL_IS_ESP_32C3)
    return 0;
#elif defined(FL_IS_ESP_32P4)
    return 6;  // ESP32-P4: jumper wire from GPIO 5
#elif defined(FL_IS_TEENSY_4X)
    return 2;  // Teensy 4.x: FlexPWM-capable pin, jumper wire from pin 1
#elif defined(FL_IS_ESP_32DEV)
    return 19;  // ESP32 (classic): partner pin to GPIO 18 above. See #3452.
#else
    return 0;  // Other variants
#endif
}

// Human-readable chip/platform name
constexpr const char* chipName() {
#if defined(FL_IS_STUB)
    return "Stub/Native";
#elif defined(FL_IS_ESP_32C6)
    return "ESP32-C6 (RISC-V)";
#elif defined(FL_IS_ESP_32S3)
    return "ESP32-S3 (Xtensa)";
#elif defined(FL_IS_ESP_32C3)
    return "ESP32-C3 (RISC-V)";
#elif defined(FL_IS_ESP_32DEV)
    return "ESP32 (Xtensa)";
#elif defined(FL_IS_ESP_32P4)
    return "ESP32-P4 (dual-core RISC-V)";
#elif defined(FL_IS_TEENSY_4X)
    return "Teensy 4.x (Cortex-M7)";
#else
    return "Unknown platform";
#endif
}

} // namespace autoresearch
