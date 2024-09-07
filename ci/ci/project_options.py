# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"
ESP32_IDF_5_1_LATEST = "https://github.com/pioarduino/platform-espressif32.git#develop"
# Top of trunk.
# ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32"

# Old fork that we were using
# ESP32_IDF_5_1 = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"


CUSTOM_PROJECT_OPTIONS = {
    "esp32dev": [f"platform={ESP32_IDF_5_1}"],
    # "esp01": f"platform={ESP32_IDF_5_1}",
    "esp32-c2-devkitm-1": [f"platform={ESP32_IDF_5_1_LATEST}"],
    "esp32-c3-devkitm-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-c6-devkitc-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-s3-devkitc-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-h2-devkitm-1": [f"platform={ESP32_IDF_5_1_LATEST}"],
    "adafruit_feather_nrf52840_sense": ["platform=nordicnrf52"],
    "rpipico": [
        "platform=https://github.com/maxgerhardt/platform-raspberrypi.git",
        "platform_packages=framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
        "framework=arduino",
        "board_build.core=earlephilhower",
        "board_build.filesystem_size=0.5m",
    ],
    "rpipico2": [
        "platform=https://github.com/maxgerhardt/platform-raspberrypi.git",
        "platform_packages=framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
        "framework=arduino",
        "board_build.core=earlephilhower",
        "board_build.filesystem_size=0.5m",
    ],
    "uno_r4_wifi": ["platform=renesas-ra", "board=uno_r4_wifi"],
    "nano_every": ["platform=atmelmegaavr"],
}
