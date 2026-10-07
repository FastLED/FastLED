#pragma once

// IWYU pragma: private

/// @file platforms/esp/32/pin_probe_esp32.hpp
/// ESP32 pin-probe table for fl/system/pin_probe.h.
///
/// A probe drives pins OUTPUT and leaves them in GPIO INPUT, which detaches
/// whatever peripheral the IO MUX had routed there. Beyond
/// FASTLED_UNUSABLE_PIN_MASK (the library-wide rule) it must skip:
///   * the console pins: native USB D-/D+ always, UART0 TXD/RXD unless
///     Serial is native USB (ARDUINO_USB_CDC_ON_BOOT); and
///   * flash/PSRAM pins not already in the library mask, only when the
///     module actually uses them (PSRAM present, package, octal sdkconfig).
/// Strapping pins are sampled at reset only and are deliberately NOT here.
/// -1 = not applicable. Pin numbers from the Espressif chip datasheets'
/// pin-overview / IO MUX tables (FastLED/datasheets espressif/soc/<chip>).
/// ci/tests/test_autoresearch_pin_discovery_safety.py pins this table.

#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/core/fastpin_esp32.h"  // _FL_VALID_PIN_MASK, FASTLED_UNUSABLE_PIN_MASK
#include "platforms/esp/esp_version.h"
#include "fl/system/pin_probe.h"
#include "fl/stl/noexcept.h"
#include "esp_heap_caps.h"  // IWYU pragma: keep
#include "sdkconfig.h"  // IWYU pragma: keep (CONFIG_SPIRAM_MODE_OCT, CONFIG_ESPTOOLPY_OCT_FLASH)
#if !ESP_IDF_VERSION_4_OR_HIGHER || defined(FL_IS_ESP_32DEV)
#include "soc/efuse_reg.h"  // IWYU pragma: keep
#include "soc/soc.h"  // IWYU pragma: keep
#endif

namespace fl {
namespace platforms {
namespace pin_probe_esp32 {

struct PinProbeLinkPins {
    int uart0_tx;
    int uart0_rx;
    int usb_dm;
    int usb_dp;
    int psram_cs;
    int psram_clk;
};
#if !ESP_IDF_VERSION_4_OR_HIGHER || defined(FL_IS_ESP_32DEV)
// ESP32 DS v5.2 Table 2-6: off-package PSRAM CE# = GPIO16, SCLK = GPIO17.
constexpr PinProbeLinkPins kLinkPins = {1, 3, -1, -1, 16, 17};
#elif defined(FL_IS_ESP_32C3)
constexpr PinProbeLinkPins kLinkPins = {21, 20, 18, 19, -1, -1};
#elif defined(FL_IS_ESP_32S2)
// SPICS1 (GPIO26) is the PSRAM chip select.
constexpr PinProbeLinkPins kLinkPins = {43, 44, 19, 20, 26, -1};
#elif defined(FL_IS_ESP_32S3)
// SPICS1 (GPIO26) is the PSRAM chip select.
constexpr PinProbeLinkPins kLinkPins = {43, 44, 19, 20, 26, -1};
#elif defined(FL_IS_ESP_32C5)
constexpr PinProbeLinkPins kLinkPins = {11, 12, 13, 14, -1, -1};
#elif defined(FL_IS_ESP_32C6)
constexpr PinProbeLinkPins kLinkPins = {16, 17, 12, 13, -1, -1};
#elif defined(FL_IS_ESP_32P4)
// USB Serial/JTAG on GPIO24/25 (USB1P1_N0/P0); PSRAM is on dedicated pads.
constexpr PinProbeLinkPins kLinkPins = {37, 38, 24, 25, -1, -1};
#elif defined(FL_IS_ESP_32H2)
constexpr PinProbeLinkPins kLinkPins = {24, 23, 26, 27, -1, -1};
#elif defined(FL_IS_ESP_32C2)
constexpr PinProbeLinkPins kLinkPins = {20, 19, -1, -1, -1, -1};
#else
constexpr PinProbeLinkPins kLinkPins = {-1, -1, -1, -1, -1, -1};
#endif

// PSRAM is present iff the heap has an SPIRAM region (same answer as
// Arduino's psramFound(), without needing the Arduino core or CONFIG_SPIRAM).
inline bool psramPresent() FL_NO_EXCEPT {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;
}

// Console pins. Native USB D-/D+ are always skipped: even when Serial is
// UART0, the USB-Serial/JTAG port is how the host deploys and resets the
// board, and detaching the PHY drops it until a power cycle.
inline u64 consolePinMask() FL_NO_EXCEPT {
    u64 mask = pinMaskBit(kLinkPins.usb_dm) | pinMaskBit(kLinkPins.usb_dp);
#if !(defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT)
    mask |= pinMaskBit(kLinkPins.uart0_tx) | pinMaskBit(kLinkPins.uart0_rx);
#endif
    return mask;
}

// Flash/PSRAM pins that depend on the module, not just the chip.
inline u64 memoryPinMask() FL_NO_EXCEPT {
    u64 mask = 0;
    bool memory_on_psram_pins = psramPresent();
#if !ESP_IDF_VERSION_4_OR_HIGHER || defined(FL_IS_ESP_32DEV)
    // ESP32 DS v5.2 Table 2-5: package codes 4-7 (U4WDH / PICO-D2, PICO-D4,
    // PICO-V3-02, D0WDR2-V3) wire in-package flash/PSRAM CS/CLK to
    // GPIO16/17 even without PSRAM. EFUSE_BLK0_RDATA3 bits [11:9] hold the
    // package code on every IDF (the field macro was renamed across IDFs).
    const u32 pkg = (REG_READ(EFUSE_BLK0_RDATA3_REG) >> 9) & 0x7;
    if (pkg >= 4) {
        memory_on_psram_pins = true;
    }
#endif
    if (memory_on_psram_pins) {
        mask |= pinMaskBit(kLinkPins.psram_cs) | pinMaskBit(kLinkPins.psram_clk);
    }
#if defined(FL_IS_ESP_32S3)
    // ESP32-S3 DS Table 2-4: GPIO33-37 carry SPIIO4-7/SPIDQS in octal (OPI)
    // flash or PSRAM mode.
#if defined(CONFIG_ESPTOOLPY_OCT_FLASH) && CONFIG_ESPTOOLPY_OCT_FLASH
    const bool octal = true;
#elif defined(CONFIG_SPIRAM_MODE_OCT) && CONFIG_SPIRAM_MODE_OCT
    const bool octal = psramPresent();
#else
    const bool octal = false;
#endif
    if (octal) {
        for (int p = 33; p <= 37; ++p) {
            mask |= pinMaskBit(p);
        }
    }
#endif
#if defined(FL_IS_ESP_32C5)
    // ESP32-C5 DS pin overview: GPIO19 is VDD_SPI, the flash power supply by
    // default; driving it browns out the flash. Kept out of the library mask
    // pending maintainer decision (FastPin<19> users, #4724).
    mask |= pinMaskBit(19);
#elif defined(FL_IS_ESP_32C6)
    // ESP32-C6 DS "Restrictions for GPIOs": GPIO27 is VDD_SPI, the flash power
    // supply by default; driving it browns out the flash. Kept out of the
    // library mask pending maintainer decision (FastPin<27> users, #4724).
    mask |= pinMaskBit(27);
#endif
    return mask;
}

}  // namespace pin_probe_esp32

inline const char* pinProbeSkipReason(int pin) FL_NO_EXCEPT {
    const u64 bit = pinMaskBit(pin);
    // Not a GPIO on this SoC (e.g. classic ESP32 24, 28-31): never
    // pinMode/digitalRead it, even as RX.
    if ((u64(SOC_GPIO_VALID_GPIO_MASK) & bit) == 0) {
        return "not-a-gpio";
    }
    // Flash / USB-JTAG pins (never pure strapping pins).
    if (u64(FASTLED_UNUSABLE_PIN_MASK) & bit) {
        return "reserved-by-FastLED";
    }
    if (pin_probe_esp32::consolePinMask() & bit) {
        return "console-link";
    }
    if (pin_probe_esp32::memoryPinMask() & bit) {
        return "flash-or-psram";
    }
    return nullptr;
}

inline const char* pinProbeDriveSkipReason(int pin) FL_NO_EXCEPT {
    const u64 bit = pinMaskBit(pin);
    if (u64(_FL_VALID_PIN_MASK) & bit) {
        return nullptr;
    }
    constexpr u64 kInputOnly =
        u64(SOC_GPIO_VALID_GPIO_MASK) & ~u64(SOC_GPIO_VALID_OUTPUT_GPIO_MASK);
    return (kInputOnly & bit) ? "input-only" : "not-output-capable";
}

}  // namespace platforms
}  // namespace fl
