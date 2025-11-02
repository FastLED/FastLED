/// @file platforms/esp/fastpin_esp.h
/// ESP platform fastpin dispatcher
///
/// Selects the appropriate fastpin implementation for ESP8266 or ESP32 variants.

// ok no namespace fl
#pragma once

#if defined(ESP32)
    #include "platforms/esp/32/core/fastpin_esp32.h"
#elif defined(ESP8266)
    #include "platforms/esp/8266/fastpin_esp8266.h"
#endif
