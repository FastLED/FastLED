import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent
PLATFORMIO_INI = PROJECT_ROOT / "platformio.ini"

ESP32C6 = """
[platformio]
src_dir = dev ; target is ./dev/dev.ino

[env:dev]
; This is the espressif32 platform which is the 4.1 toolchain as of 2024-Aug-23rd
; platform = espressif32
; The following platform enables the espressif32 platform to use the 5.1 toolchain, simulating
; the new Arduino 2.3.1+ toolchain.

# Developement branch of the open source espressif32 platform
platform =  https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip

framework = arduino
board = esp32-c6-devkitc-1

upload_protocol = esptool

monitor_filters = 
	default
	esp32_exception_decoder  ; Decode exceptions so that they are human readable.

; Symlink in the FastLED library so that changes to the library are reflected in the project
; build immediatly.
lib_deps =
  FastLED=symlink://./

build_type = debug

build_flags =
	-DDEBUG
    -g
    -Og
    -DCORE_DEBUG_LEVEL=5
    -DLOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    ;-DFASTLED_RMT5=1
    -DFASTLED_ESP32_SPI_BULK_TRANSFER=1
    -DENABLE_ESP32_I2S_YVES_DRIVER=1
    
check_tool = clangtidy
"""

ESP32S3 = """
[platformio]
src_dir = dev ; target is ./dev/dev.ino

[env:dev]
; This is the espressif32 platform which is the 4.1 toolchain as of 2024-Aug-23rd
; platform = espressif32
; The following platform enables the espressif32 platform to use the 5.1 toolchain, simulating
; the new Arduino 2.3.1+ toolchain.

# Developement branch of the open source espressif32 platform
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip

framework = arduino
board = esp32-s3-devkitc-1

upload_protocol = esptool

monitor_filters = 
	default
	esp32_exception_decoder  ; Decode exceptions so that they are human readable.

; Symlink in the FastLED library so that changes to the library are reflected in the project
; build immediatly.
lib_deps =
  FastLED=symlink://./

build_type = debug

build_flags =
	-DDEBUG
    -g
    -Og
    -DCORE_DEBUG_LEVEL=5
    -DLOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    ;-DFASTLED_RMT5=1
    -DFASTLED_ESP32_SPI_BULK_TRANSFER=1
    -DENABLE_ESP32_I2S_YVES_DRIVER=1
    
check_tool = clangtidy
"""

_ALL = {"esp32c6": ESP32C6, "esp32s3": ESP32S3}


def prompt_user(msg: str) -> int:
    while True:
        try:
            return int(input(msg))
        except ValueError:
            print("Please enter a valid integer")
            continue


def main() -> None:
    print("This tool will update the platformio.ini file with the selected platform")
    print("Please select a platform:")
    print("[0]: Exit")
    for i, platform in enumerate(_ALL.keys()):
        print(f"[{i+1}]: {platform}")
    val = prompt_user("Enter a number: ")
    if val == 0:
        sys.exit(0)
    if val < 0 or val > len(_ALL):
        print("Invalid selection")
        sys.exit(1)
    platform = list(_ALL.keys())[val - 1]
    with PLATFORMIO_INI.open("w") as f:
        f.write(_ALL[platform])
    print(f"Selected platform: {platform}")

    sys.exit(1)


if __name__ == "__main__":
    main()
