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
