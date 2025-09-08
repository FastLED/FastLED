#pragma once

#include "platforms/esp/32/audio/devices/i2s.hpp"
#include "platforms/esp/32/audio/devices/null.hpp"

// Ensure FASTLED_ESP32_I2S_SUPPORTED is defined (included via i2s.hpp)
#ifndef FASTLED_ESP32_I2S_SUPPORTED
#error "FASTLED_ESP32_I2S_SUPPORTED should be defined by including i2s.hpp"
#endif

// Function implementation is in audio_impl.cpp
