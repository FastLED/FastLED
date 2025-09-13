# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files
file(GLOB FastLED_SRCS "src/*.cpp")
file(GLOB FastLED_FL_SRCS "src/fl/*.cpp")
file(GLOB FastLED_SENSORS_SRCS "src/sensors/*.cpp")
file(GLOB FastLED_PLATFORM_ARDUINO_SRCS "src/platforms/arduino/*.cpp")
file(GLOB FastLED_FX_SRCS "src/fx/*.cpp" "src/fx/**/*.cpp")

file(GLOB ESP32_SRCS "src/platforms/esp/32/*.cpp" "src/platforms/esp/32/rmt_5/*.cpp")
file(GLOB ESP32_THIRD_PARTY_SRCS "src/third_party/**/src/*.c" "src/third_party/**/src/*.cpp")
file(GLOB ESP32_LED_STRIP_SRCS "src/third_party/espressif/led_strip/src/*.c")

# Combine all source files into a single list
list(APPEND FastLED_SRCS ${FastLED_FL_SRCS} ${FastLED_SENSORS_SRCS} ${FastLED_FX_SRCS} ${ESP32_SRCS} ${ESP32_THIRD_PARTY_SRCS} ${ESP32_LED_STRIP_SRCS} ${FastLED_PLATFORM_ARDUINO_SRCS})

# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "src" "src/third_party/espressif/led_strip/src"
                       REQUIRES arduino-esp32 esp_driver_rmt esp_lcd driver)

project(FastLED)
