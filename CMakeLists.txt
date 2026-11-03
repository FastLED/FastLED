# FastLED
# https://github.com/FastLED/FastLED
# MIT License

cmake_minimum_required(VERSION 3.5)

# Collect all source files
file(GLOB_RECURSE FastLED_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

set(FASTLED_REQUIRES esp_http_server driver esp_wifi app_update esp_timer mbedtls)

# BUILD_COMPONENTS is populated after dependency expansion. During IDF's
# earlier requirements pass, only the registered component targets exist.
# Check both phases, including the component-manager namespace spelling.
if(COMMAND idf_build_get_property)
    idf_build_get_property(FASTLED_BUILD_COMPONENTS BUILD_COMPONENTS)
    idf_build_get_property(FASTLED_COMPONENT_TARGETS __COMPONENT_TARGETS)
else()
    # IDF 3.3 exposes component inventories as ordinary variables.
    set(FASTLED_BUILD_COMPONENTS ${BUILD_COMPONENTS} ${COMPONENTS})
    set(FASTLED_COMPONENT_TARGETS "")
endif()
foreach(FASTLED_ARDUINO_COMPONENT arduino-esp32 espressif__arduino-esp32)
    list(FIND FASTLED_BUILD_COMPONENTS "${FASTLED_ARDUINO_COMPONENT}" FASTLED_ARDUINO_INDEX)
    list(FIND FASTLED_COMPONENT_TARGETS "___idf_${FASTLED_ARDUINO_COMPONENT}" FASTLED_ARDUINO_TARGET_INDEX)
    if(FASTLED_ARDUINO_INDEX GREATER_EQUAL 0 OR FASTLED_ARDUINO_TARGET_INDEX GREATER_EQUAL 0)
        list(APPEND FASTLED_REQUIRES "${FASTLED_ARDUINO_COMPONENT}")
        break()
    endif()
endforeach()

# These components did not exist in IDF 3.3. Older SDKs provide the peripheral
# headers through driver instead of the split esp_driver_* components.
if(IDF_VERSION_MAJOR GREATER 4 OR
   (IDF_VERSION_MAJOR EQUAL 4 AND IDF_VERSION_MINOR GREATER_EQUAL 1))
    list(APPEND FASTLED_REQUIRES esp_netif)
endif()
if(IDF_VERSION_MAJOR GREATER 4 OR
   (IDF_VERSION_MAJOR EQUAL 4 AND IDF_VERSION_MINOR GREATER_EQUAL 4))
    list(APPEND FASTLED_REQUIRES esp_lcd)
endif()
if(IDF_VERSION_MAJOR GREATER 5 OR
   (IDF_VERSION_MAJOR EQUAL 5 AND IDF_VERSION_MINOR GREATER_EQUAL 1))
    list(APPEND FASTLED_REQUIRES esp_mm)
endif()
if(IDF_VERSION_MAJOR GREATER_EQUAL 5)
    list(APPEND FASTLED_REQUIRES esp_adc)
endif()
if(IDF_VERSION_MAJOR GREATER 5 OR
   (IDF_VERSION_MAJOR EQUAL 5 AND IDF_VERSION_MINOR GREATER_EQUAL 3))
    list(APPEND FASTLED_REQUIRES esp_driver_i2s esp_driver_mcpwm esp_driver_rmt)
endif()

# esp_rom is a standalone component only from IDF 4.x onward (esp_rom_sys.h /
# esp_rom_delay_us). On IDF 3.x, platform_time_esp32.cpp.hpp instead uses
# rom/ets_sys.h (ets_delay_us), which ships as part of the legacy target
# component and has no separate REQUIRES entry.
if(NOT DEFINED IDF_VERSION_MAJOR OR IDF_VERSION_MAJOR GREATER_EQUAL 4)
    list(APPEND FASTLED_REQUIRES esp_rom)
endif()

# Register the component with ESP-IDF
idf_component_register(SRCS ${FastLED_SRCS}
                       INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
                       REQUIRES ${FASTLED_REQUIRES})

project(FastLED)
