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
  )

file(GLOB ESP32_SRCS "src/platforms/esp/32/*.cpp")
file(GLOB ESP32_LED_STRIP_COMPONENT_SRCS "src/platforms/esp/32/led_strip/*.cpp")
list(APPEND FastLED_SRCS ${ESP32_SRCS})

idf_component_register(SRCS ${FastLED_SRCS} ${ESP32_LED_STRIP_COMPONENT_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino)

project(FastLED)
