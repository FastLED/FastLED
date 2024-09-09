# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

file(GLOB FastLED_SRCS "src/*.cpp")
list(REMOVE_ITEM FastLED_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/stub_main.cpp")

file(GLOB ESP32_SRCS "src/platforms/esp/32/*.cpp")
file(GLOB ESP32_LED_STRIP_COMPONENT_SRCS "src/platforms/esp/32/led_strip/*.cpp")
list(APPEND FastLED_SRCS ${ESP32_SRCS})

idf_component_register(SRCS ${FastLED_SRCS} ${ESP32_LED_STRIP_COMPONENT_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino)

project(FastLED)
