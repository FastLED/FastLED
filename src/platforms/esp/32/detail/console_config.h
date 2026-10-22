#pragma once

// IWYU pragma: private
// ok no namespace fl - native console capability and backend selection.

#include "platforms/esp/is_esp.h"
#include "fl/stl/has_include.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0) && \
    FL_HAS_INCLUDE(<driver/usb_serial_jtag.h>) && \
    (defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C3) || \
     defined(FL_IS_ESP_32C5) || defined(FL_IS_ESP_32C6) || \
     defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32P4))
#define FL_ESP_HAS_USB_SERIAL_JTAG 1
#else
#define FL_ESP_HAS_USB_SERIAL_JTAG 0
#endif

#if defined(FL_ESP_USE_TINYUSB_CDC) && FL_ESP_USE_TINYUSB_CDC
#if (defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT) || \
    (defined(ARDUINO_USB_ON_BOOT) && ARDUINO_USB_ON_BOOT) || \
    (defined(ARDUINO_USB_MSC_ON_BOOT) && ARDUINO_USB_MSC_ON_BOOT) || \
    (defined(ARDUINO_USB_DFU_ON_BOOT) && ARDUINO_USB_DFU_ON_BOOT)
#error "Native TinyUSB requires Arduino USB startup disabled at build time"
#endif
#define FL_ESP_IO_USE_TINYUSB_CDC 1
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
#elif defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
// Arduino HWCDC installs its own ISR and RX queue, so even hardware JTAG
// requires exclusive native ownership. Select the transport explicitly.
#error "Native USB console requires Arduino USB startup disabled; select FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1 or FL_ESP_USE_TINYUSB_CDC=1 at build time"
#elif defined(FASTLED_ESP_FORCE_UART_SERIAL)
#define FL_ESP_IO_USE_TINYUSB_CDC 0
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
#elif defined(FASTLED_ESP_FORCE_USB_SERIAL_JTAG)
#if !FL_ESP_HAS_USB_SERIAL_JTAG
#error "Selected console requires an SDK and chip with USB Serial/JTAG support"
#endif
#define FL_ESP_IO_USE_TINYUSB_CDC 0
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 1
#else
#define FL_ESP_IO_USE_TINYUSB_CDC 0
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
#endif
