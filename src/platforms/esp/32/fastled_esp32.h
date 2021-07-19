#pragma once

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
