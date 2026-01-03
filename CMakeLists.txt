# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files
file(GLOB FastLED_SRCS "src/**/*.cpp")



# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32 esp_driver_rmt esp_lcd driver esp_mm esp_wifi esp_netif)

project(FastLED)
