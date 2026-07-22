# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files
file(GLOB_RECURSE FastLED_SRCS "src/*.cpp")

set(FASTLED_REQUIRES arduino-esp32 esp_driver_i2s esp_driver_mcpwm esp_driver_rmt
                      esp_http_server esp_lcd driver esp_mm esp_wifi esp_netif
                      app_update esp_timer)

# esp_rom is a standalone component only from IDF 4.x onward (esp_rom_sys.h /
# esp_rom_delay_us). On IDF 3.x, platform_time_esp32.cpp.hpp instead uses
# rom/ets_sys.h (ets_delay_us), which ships as part of the legacy target
# component and has no separate REQUIRES entry.
if(NOT DEFINED IDF_VERSION_MAJOR OR IDF_VERSION_MAJOR GREATER_EQUAL 4)
    list(APPEND FASTLED_REQUIRES esp_rom)
endif()

# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES ${FASTLED_REQUIRES})

project(FastLED)
