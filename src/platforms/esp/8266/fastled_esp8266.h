// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "platforms/esp/8266/fastpin_esp8266.h"

#include "platforms/esp/8266/clockless_esp8266.h"
#include "platforms/esp/8266/clockless_block_esp8266.h"

// The shared GPIO bit-bang driver backs `Bus::BIT_BANG`, which is the ESP8266
// `DefaultBus` for both chipset families (see `fl/channels/bus.h`). Including
// its `BusTraits<Bus::BIT_BANG>` specialization here is what makes
// `fl::Channel<Bus::AUTO, ...>` / `FastLED.add<Bus::AUTO, ...>()` resolve.
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers

// The UART1 (GPIO2) WS2812 engine is exposed through the channel API as
// `Bus::UART` (#4944); its `BusTraits<Bus::UART>` specialization lives in
// `uart_bus_traits.h`. The legacy `UARTController_ESP8266` template stays
// available. Neither inclusion links anything unless it is used.
#include "platforms/esp/8266/fastled_esp8266_uart.h"  // ok platform headers
#include "platforms/esp/8266/uart_bus_traits.h"       // ok platform headers
