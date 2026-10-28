// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "platforms/esp/8266/fastpin_esp8266.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "platforms/esp/8266/fastspi_esp8266.h"
#endif

#include "platforms/esp/8266/clockless_esp8266.h"
#include "platforms/esp/8266/clockless_block_esp8266.h"

// The shared GPIO bit-bang driver backs `Bus::BIT_BANG`, which is the ESP8266
// `DefaultBus` for both chipset families (see `fl/channels/bus.h`). Including
// its `BusTraits<Bus::BIT_BANG>` specialization here is what makes
// `fl::Channel<Bus::AUTO, ...>` / `FastLED.add<Bus::AUTO, ...>()` resolve.
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers

#ifdef FASTLED_ESP8266_UART
#include "platforms/esp/8266/fastled_esp8266_uart.h"
#endif
