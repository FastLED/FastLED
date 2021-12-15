#pragma once

// macros and platform define are not available in older platform versions
// so set some defaults to avoid build errors
#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#ifndef CONFIG_IDF_TARGET_ESP32
#define CONFIG_IDF_TARGET_ESP32 1
#endif
#endif

#include "fastpin_esp32.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#include "clockless_i2s_esp32.h"
#else
#include "clockless_rmt_esp32.h"
#endif

// #include "clockless_block_esp32.h"
