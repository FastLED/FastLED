"""CDC ON BOOT warning detection utility.

Detects when ARDUINO_USB_CDC_ON_BOOT=1 is enabled in the build configuration
and issues a yellow warning that the device may appear to hang if not connected
to USB.

This is important for ESP32-S3, ESP32-C6, ESP32-P4 and similar boards that use
native USB CDC for serial output. When CDC ON BOOT is enabled, the device waits
for a USB CDC connection during startup, which can make it appear unresponsive
if not plugged into a USB host.
"""

import re
from pathlib import Path

# Pattern to match ARDUINO_USB_CDC_ON_BOOT=1 in build flags or INI files.
# Matches: -DARDUINO_USB_CDC_ON_BOOT=1, -D ARDUINO_USB_CDC_ON_BOOT=1, etc.
_CDC_ON_BOOT_ENABLED_RE = re.compile(r"ARDUINO_USB_CDC_ON_BOOT\s*=\s*1\b")

_WARNING_LINES = [
    "⚠️  CDC ON BOOT is enabled (ARDUINO_USB_CDC_ON_BOOT=1).",
    "   The device may appear to hang on boot if not connected to USB.",
    "   If the device seems unresponsive, ensure it is plugged into a USB port.",
]


def _yellow(text: str) -> str:
    """Wrap text in ANSI yellow."""
    return f"\033[33m{text}\033[0m"


def check_cdc_on_boot_in_file(ini_path: Path) -> bool:
    """Check whether a platformio.ini (or similar) file enables CDC ON BOOT.

    Args:
        ini_path: Path to the platformio.ini file to inspect.

    Returns:
        True if ARDUINO_USB_CDC_ON_BOOT=1 is found in the file.
    """
    try:
        content = ini_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return False
    return bool(_CDC_ON_BOOT_ENABLED_RE.search(content))


def check_cdc_on_boot_in_flags(build_flags: list[str] | None) -> bool:
    """Check whether a list of build flags enables CDC ON BOOT.

    Args:
        build_flags: List of compiler flags (e.g. ["-DARDUINO_USB_CDC_ON_BOOT=1"]).

    Returns:
        True if ARDUINO_USB_CDC_ON_BOOT=1 is found among the flags.
    """
    if not build_flags:
        return False
    for flag in build_flags:
        if _CDC_ON_BOOT_ENABLED_RE.search(flag):
            return True
    return False


def print_cdc_on_boot_warning() -> None:
    """Print the CDC ON BOOT warning banner in yellow."""
    print()
    for line in _WARNING_LINES:
        print(_yellow(line))
    print()


def warn_if_cdc_on_boot(
    ini_path: Path | None = None,
    build_flags: list[str] | None = None,
) -> bool:
    """Check for CDC ON BOOT and print a warning if enabled.

    Accepts either a platformio.ini path, a list of build flags, or both.
    If either source indicates CDC ON BOOT is enabled, a yellow warning is
    printed to the console.

    Args:
        ini_path: Optional path to a platformio.ini file.
        build_flags: Optional list of build flags to inspect.

    Returns:
        True if a warning was printed, False otherwise.
    """
    detected = False
    if ini_path is not None:
        detected = detected or check_cdc_on_boot_in_file(ini_path)
    if build_flags is not None:
        detected = detected or check_cdc_on_boot_in_flags(build_flags)
    if detected:
        print_cdc_on_boot_warning()
    return detected
