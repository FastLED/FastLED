import os

# Project initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = (
    os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "1") == "1"
)

# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"
ESP32_IDF_5_1_LATEST = "https://github.com/pioarduino/platform-espressif32.git#develop"
# Top of trunk.
# ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32"

# Old fork that we were using
# ESP32_IDF_5_1 = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"


EXAMPLES = [
    "Apa102HD",
    "Blink",
    "ColorPalette",
    "ColorTemperature",
    "Cylon",
    "DemoReel100",
    "Fire2012",
    "FirstLight",
    "Multiple/MultipleStripsInOneArray",
    "Multiple/ArrayOfLedArrays",
    "Noise",
    "NoisePlayground",
    "NoisePlusPalette",
    "Pacifica",
    "Pride2015",
    "RGBCalibrate",
    "RGBSetDemo",
    "TwinkleFox",
    "XYMatrix",
]

# Default boards to compile for. You can use boards not defined here but
# if the board isn't part of the officially supported platformio boards then
# you will need to add the board to the ~/.platformio/platforms directory.
# prior to running this script. This happens automatically as of 2024-08-20
# with the github workflow scripts.
BOARDS = [
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32-c3-devkitm-1",
    # "esp32-c6-devkitc-1",
    # "esp32-c2-devkitm-1",
    "esp32-s3-devkitc-1",
    "yun",
    "digix",
    "teensy30",
    "teensy41",
]

OTHER_BOARDS = [
    "adafruit_feather_nrf52840_sense",
    "rpipico",
    "rpipico2",
    "uno_r4_wifi",
    "nano_every",
]


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
