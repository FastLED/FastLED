# dataclasses

import json
from dataclasses import dataclass

# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"
ESP32_IDF_5_1_LATEST = "https://github.com/pioarduino/platform-espressif32.git#develop"
# Top of trunk.
# ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32"

# Old fork that we were using
# ESP32_IDF_5_1 = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"


@dataclass
class Board:
    board_name: str
    platform: str | None = None
    platform_needs_install: bool = False
    platform_packages: str | None = None
    framework: str | None = None
    board_build_core: str | None = None
    board_build_filesystem_size: str | None = None

    def to_dictionary(self) -> dict[str, list[str]]:
        out: dict[str, list[str]] = {}
        options = out.setdefault(self.board_name, [])
        if self.platform:
            options.append(f"platform={self.platform}")
        if self.platform_needs_install:
            options.append("platform_needs_install=true")
        if self.platform_packages:
            options.append(f"platform_packages={self.platform_packages}")
        if self.framework:
            options.append(f"framework={self.framework}")
        if self.board_build_core:
            options.append(f"board_build.core={self.board_build_core}")
        if self.board_build_filesystem_size:
            options.append(
                f"board_build.filesystem_size={self.board_build_filesystem_size}"
            )
        return out

    def __repr__(self) -> str:
        json_str = json.dumps(self.to_dictionary(), indent=4, sort_keys=True)
        return json_str


ESP32DEV = Board(
    board_name="esp32dev",
    platform=ESP32_IDF_5_1,
)

# ESP01 = Board(
#     board_name="esp01",
#     platform=ESP32_IDF_5_1,
# )

ESP32_C2_DEVKITM_1 = Board(
    board_name="esp32-c2-devkitm-1",
    platform_needs_install=True,  # Install platform package to get the boards
    platform=ESP32_IDF_5_1_LATEST,
)

ESP32_C3_DEVKITM_1 = Board(
    board_name="esp32-c3-devkitm-1",
    platform=ESP32_IDF_5_1,
)

ESP32_C6_DEVKITC_1 = Board(
    board_name="esp32-c6-devkitc-1",
    platform=ESP32_IDF_5_1,
)

ESP32_S3_DEVKITC_1 = Board(
    board_name="esp32-s3-devkitc-1",
    platform=ESP32_IDF_5_1,
)

ESP32_H2_DEVKITM_1 = Board(
    board_name="esp32-h2-devkitm-1",
    platform_needs_install=True,  # Install platform package to get the boards
    platform=ESP32_IDF_5_1_LATEST,
)

ADA_FEATHER_NRF52840_SENSE = Board(
    board_name="adafruit_feather_nrf52840_sense",
    platform="nordicnrf52",
)

RPI_PICO = Board(
    board_name="rpipico",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

RPI_PICO2 = Board(
    board_name="rpipico2",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

UNO_R4_WIFI = Board(
    board_name="uno_r4_wifi",
    platform="renesas-ra",
)

NANO_EVERY = Board(
    board_name="nano_every",
    platform="atmelmegaavr",
)

ALL: list[Board] = [
    ESP32DEV,
    # ESP01,
    ESP32_C2_DEVKITM_1,
    ESP32_C3_DEVKITM_1,
    ESP32_C6_DEVKITC_1,
    ESP32_S3_DEVKITC_1,
    ESP32_H2_DEVKITM_1,
    ADA_FEATHER_NRF52840_SENSE,
    RPI_PICO,
    RPI_PICO2,
    UNO_R4_WIFI,
    NANO_EVERY,
]

_BOARD_MAP: dict[str, Board] = {board.board_name: board for board in ALL}


def get_board(board_name: str, no_project_options: bool = False) -> Board:
    if no_project_options:
        return Board(board_name=board_name)
    if board_name not in _BOARD_MAP:
        # empty board without any special overrides, assume platformio will know what to do with it.
        return Board(board_name=board_name)
    else:
        return _BOARD_MAP[board_name]
