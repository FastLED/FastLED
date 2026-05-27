# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files (unity-build entry points live in src/fl/build/).
# Use GLOB_RECURSE because file(GLOB) does not walk subdirectories and `**`
# is not a recursive wildcard in CMake -- the previous non-recursive pattern
# returned an empty list, leaving the component CONFIG_ONLY.
file(GLOB_RECURSE FastLED_SRCS "src/*.cpp")



# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32 esp_driver_rmt esp_lcd driver esp_mm esp_wifi esp_netif)

project(FastLED)
