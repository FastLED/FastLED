#pragma once

#include "fastpin_esp32.h"

#ifndef FASTLED_ESP32_C2
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(ESP32C2)
#define FASTLED_ESP32_C2 1
#else
#define FASTLED_ESP32_C2 0
#endif
#endif

#ifndef FASTLED_ESP32_H2
#if defined(CONFIG_IDF_TARGET_ESP32H2) || defined(ESP32H2)
#define FASTLED_ESP32_H2 1
#else
#define FASTLED_ESP32_H2 0
#endif
#endif

#ifndef FASTLED_ESP32_CLOCKLESS_BITBANG
#define FASTLED_ESP32_CLOCKLESS_BITBANG (FASTLED_ESP32_C2)
#endif

#if FASTLED_ESP32_C2
#include "clockless_block_esp32.h"
#endif  // FASTLED_ESP32_C2

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#include "clockless_i2s_esp32.h"
#elif FASTLED_ESP32_CLOCKLESS_BITBANG
#include "clockless_block_esp32.h"
#else
#include "clockless_rmt_esp32.h"
#endif
