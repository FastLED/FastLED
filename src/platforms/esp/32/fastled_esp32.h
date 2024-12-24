#pragma once

#include "fastpin_esp32.h"

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
#include "fastspi_esp32.h"
#endif

#ifdef FASTLED_ESP32_I2S
#ifndef FASTLED_INTERNAL
// Be careful with including defs for internal code.
#include "clockless_i2s_esp32.h"
#endif
#else
#include "clockless_rmt_esp32.h"
#endif
