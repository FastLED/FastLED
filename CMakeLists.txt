# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

file(GLOB FastLED_SRCS "src/*.cpp")
file(GLOB FastLED_SRCS "src/fl/**/*.cpp")
file(GLOB FastLED_SRCS "src/sensors/**/*.cpp")
file(GLOB FastLED_SRCS "src/fx/**/*.cpp")

file(GLOB ESP32_SRCS "src/platforms/esp/32/*.cpp")
file(GLOB ESP32_THIRD_PARTY "src/third_party/*.c")
file(GLOB ESP32_THIRD_PARTY "src/third_party/*.cpp")
list(APPEND FastLED_SRCS ${ESP32_SRCS})

idf_component_register(SRCS ${FastLED_SRCS} ${ESP32_LED_STRIP_COMPONENT_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32 esp_driver_rmt driver)

project(FastLED)
