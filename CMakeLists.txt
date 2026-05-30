# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files
file(GLOB_RECURSE FastLED_SRCS "src/*.cpp")



# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32 esp_driver_i2s esp_driver_mcpwm esp_driver_rmt
                                esp_http_server esp_lcd driver esp_mm esp_wifi esp_netif
                                app_update)

project(FastLED)
