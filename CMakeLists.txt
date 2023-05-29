# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

set(FastLED_SRCS
  src/bitswap.cpp
  src/colorpalettes.cpp
  src/colorutils.cpp
  src/FastLED.cpp
  src/hsv2rgb.cpp
  src/lib8tion.cpp
  src/noise.cpp
  src/platforms.cpp
  src/power_mgt.cpp
  src/wiring.cpp
  src/platforms/esp/32/clockless_rmt_esp32.cpp
  )

idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino)

project(FastLED)
