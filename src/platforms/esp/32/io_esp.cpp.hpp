#pragma once

// IWYU pragma: private
// ok no namespace fl - console implementation dispatch.

#include "platforms/esp/is_esp.h" // IWYU pragma: keep

#ifdef FL_IS_ESP32

#if defined(FL_ESP_USE_IDF_SERIAL) && !FL_ESP_USE_IDF_SERIAL
#error "ESP32 consoles use native SDK drivers; remove FL_ESP_USE_IDF_SERIAL=0."
#endif

#include "platforms/esp/32/io_esp_idf.hpp"

#endif // FL_IS_ESP32
