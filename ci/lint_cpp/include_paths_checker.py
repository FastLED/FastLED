#!/usr/bin/env python3
"""
Lint check: Enforce include path style in FastLED.

Within src/fl/** and src/platforms/**, all #include "..." statements must use
paths relative to src/. This ensures consistent include paths across the codebase
and proper IDE support.

Valid includes:
  - #include "fl/foo.h"              - starts with fl/
  - #include "platforms/bar.h"       - starts with platforms/
  - #include <system_header>         - system headers are fine
  - #include "FastLED.h"             - top-level src files are fine
  - #include "lib8tion/foo.h"        - starts with lib8tion/
  - #include "fx/foo.h"              - starts with fx/
  - #include "sensors/foo.h"         - starts with sensors/
  - #include "third_party/foo.h"     - starts with third_party/
  - #include "hardware/irq.h"        - external SDK headers are allowed
  - #include "freertos/FreeRTOS.h"   - external SDK headers are allowed

Invalid includes:
  - #include "../foo.h"              - parent-relative paths
  - #include "./foo.h"               - current-dir relative
  - #include "avr/foo.h"             - inside platforms/, should be "platforms/avr/foo.h"
  - #include "esp/foo.h"             - inside platforms/, should be "platforms/esp/foo.h"
  - #include "arm/foo.h"             - inside platforms/, should be "platforms/arm/foo.h"
  - #include "stub/foo.h"            - inside platforms/, should be "platforms/stub/foo.h"
  - #include "wasm/foo.h"            - inside platforms/, should be "platforms/wasm/foo.h"

The key distinction is between:
  - External SDK headers (e.g., hardware/, driver/, freertos/) - ALLOWED
  - FastLED platform subdirectories (e.g., avr/, esp/, arm/) - MUST use platforms/ prefix

Exception mechanism:
  Add "// ok include path" comment to suppress the check on a specific line.
"""

# pyright: reportUnknownMemberType=false
import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# Valid top-level prefixes for include paths (relative to src/)
VALID_PREFIXES = (
    "fl/",
    "platforms/",
    "lib8tion/",
    "fx/",
    "sensors/",
    "third_party/",
)

# External SDK header prefixes - these are not FastLED code, so they're allowed
# These headers come from platform SDKs (ESP-IDF, Pico SDK, FreeRTOS, Arduino, etc.)
# Note: Some prefixes like "avr/" and "arm/" are ambiguous - they could be SDK headers
# or FastLED platform subdirs. We handle this by checking file extensions.
EXTERNAL_SDK_PREFIXES = (
    # ESP-IDF SDK headers
    "driver/",
    "esp_",
    "esp32/",
    "freertos/",
    "hal/",
    "soc/",
    "rom/",
    "sdkconfig",
    "xtensa/",
    "riscv/",
    # Raspberry Pi Pico SDK headers
    "hardware/",
    "pico/",
    "boards/",
    "cmsis/",
    "class/",
    "tusb_",
    # Arduino headers
    "Arduino.h",
    "Wire.h",
    "SPI.h",
    "pins_arduino.h",
    # GCC system headers (but NOT avr/ - handled separately due to ambiguity)
    "util/",
    # ARM/Cortex system headers (but NOT arm/ - handled separately due to ambiguity)
    "core_",
    "cmsis_",
    # NRF SDK headers
    "nrf_",
    "nrf52",
    "nrfx",
    # STM32 HAL headers
    "stm32",
    # Teensy SDK headers
    "core_pins.h",
    "usb_",
    "kinetis.h",
    "imxrt.h",
    # RP2040/RP2350 SDK headers
    "RP2040.h",
    "RP2350.h",
    # Renesas SDK headers
    "bsp_",
    "r_",
    "fsp/",
)

# Ambiguous prefixes that could be either SDK headers or FastLED platform dirs
# For these, we check file extensions to disambiguate:
# - SDK headers: typically .h only (e.g., <avr/io.h>, <arm/math.h>)
# - FastLED code: may use .hpp, .cpp or have FastLED-specific filenames
AMBIGUOUS_PREFIXES = (
    "avr/",  # Could be <avr/io.h> (SDK) or "platforms/avr/clockless.h" (FastLED)
    "arm/",  # Could be <arm/math.h> (SDK) or "platforms/arm/fastpin.h" (FastLED)
)

# FastLED platform subdirectories - these MUST use the "platforms/" prefix
# when included from within src/fl/ or src/platforms/
FASTLED_PLATFORM_SUBDIRS = (
    "adafruit/",
    "apollo3/",
    "arm/",
    "avr/",
    "esp/",
    "shared/",
    "stub/",
    "wasm/",
    "posix/",
)

# Likely typos of valid FastLED prefixes that should be flagged
# These are misspellings or variations that look like they're trying to
# include FastLED code but got the prefix wrong
TYPO_PREFIXES = {
    "ftl/": "fl/stl/",  # Typo: "ftl/" should be "fl/stl/"
    "fL/": "fl/",  # Case typo
    "FL/": "fl/",  # Case typo
    "Fl/": "fl/",  # Case typo
    "platform/": "platforms/",  # Singular instead of plural
    "Platform/": "platforms/",  # Case + singular
    "PLATFORMS/": "platforms/",  # All caps
}

# Pattern to extract include path from #include "..." statements
INCLUDE_PATTERN = re.compile(r'#include\s+"([^"]+)"')


def is_top_level_include(include_path: str) -> bool:
    """Check if include path is a top-level file (no directory separator).

    Top-level includes like "FastLED.h", "pixeltypes.h" are valid since
    they refer to files directly in src/.
    """
    return "/" not in include_path and "\\" not in include_path


def looks_like_fastled_code(include_path: str) -> bool:
    """Check if the include path looks like FastLED code (not SDK code).

    FastLED code characteristics:
    - Uses .hpp extension (SDK headers almost never use this)
    - Uses .cpp extension (should never be included)
    - Has FastLED-specific naming patterns
    """
    # .hpp is almost exclusively used by FastLED, not SDK headers
    if include_path.endswith(".hpp"):
        return True

    # .cpp files should never be included as headers
    if include_path.endswith(".cpp"):
        return True

    # Common FastLED naming patterns
    fastled_patterns = (
        "clockless",
        "fastpin",
        "fastspi",
        "led_sysdefs",
        "compile_test",
        "spi_output",
        "is_",  # Platform detection headers like is_avr.h, is_arm.h
    )
    filename = include_path.split("/")[-1]
    for pattern in fastled_patterns:
        if pattern in filename.lower():
            return True

    return False


def is_external_sdk_header(include_path: str) -> bool:
    """Check if the include path is from an external SDK.

    External SDK headers (like ESP-IDF, Pico SDK, FreeRTOS, Arduino)
    are allowed to be included with their original paths.
    """
    # First check if it's definitely an SDK header
    for prefix in EXTERNAL_SDK_PREFIXES:
        if include_path.startswith(prefix):
            return True

    # For ambiguous prefixes (avr/, arm/), check if it looks like FastLED code
    for prefix in AMBIGUOUS_PREFIXES:
        if include_path.startswith(prefix):
            # If it looks like FastLED code, it's NOT an SDK header
            if looks_like_fastled_code(include_path):
                return False
            # Otherwise, assume it's an SDK header (e.g., <avr/io.h>)
            return True

    return False


def is_fastled_platform_relative(include_path: str) -> bool:
    """Check if the include path looks like a relative include to FastLED platform code.

    These are paths that should use "platforms/" prefix but don't.
    For example: "esp/foo.h" should be "platforms/esp/foo.h"

    For ambiguous prefixes like avr/ and arm/, we only flag them as FastLED
    platform code if they look like FastLED code (e.g., .hpp files, specific
    FastLED naming patterns).
    """
    # Check unambiguous FastLED platform subdirs
    for subdir in FASTLED_PLATFORM_SUBDIRS:
        if include_path.startswith(subdir):
            return True

    # For ambiguous prefixes, check if it looks like FastLED code
    for prefix in AMBIGUOUS_PREFIXES:
        if include_path.startswith(prefix):
            if looks_like_fastled_code(include_path):
                return True

    return False


def is_valid_include_path(include_path: str) -> bool:
    """Check if include path follows the required style.

    Args:
        include_path: The path from #include "path"

    Returns:
        True if the path is valid (starts with valid prefix, is top-level,
        or is an external SDK header)
    """
    # Top-level includes are fine (e.g., "FastLED.h")
    if is_top_level_include(include_path):
        return True

    # External SDK headers are allowed (e.g., "hardware/irq.h", "freertos/FreeRTOS.h")
    if is_external_sdk_header(include_path):
        return True

    # Check if path starts with a valid FastLED prefix
    for prefix in VALID_PREFIXES:
        if include_path.startswith(prefix):
            return True

    return False


def is_relative_path(include_path: str) -> bool:
    """Check if the path is a relative path (starts with ./ or ../)."""
    return include_path.startswith("./") or include_path.startswith("../")


def get_typo_suggestion(include_path: str) -> str | None:
    """Check if the include path starts with a likely typo of a valid prefix.

    Returns the correct prefix if a typo is detected, None otherwise.
    """
    for typo, correct in TYPO_PREFIXES.items():
        if include_path.startswith(typo):
            return correct
    return None


class IncludePathsChecker(FileContentChecker):
    """Checker for include path style enforcement."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process files in src/fl/** and src/platforms/**."""
        # Normalize path separators for cross-platform compatibility
        normalized_path = file_path.replace("\\", "/")

        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".c")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # Only check files in src/fl/ and src/platforms/
        if "/src/fl/" in normalized_path or "/src/platforms/" in normalized_path:
            return True

        return False

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for invalid include paths."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            # Skip if in multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comments
            if stripped.startswith("//"):
                continue

            # Split line to separate code from inline comments
            code_part = line.split("//")[0]
            comment_part = line.split("//", 1)[1] if "//" in line else ""

            # Check if exception comment is present
            has_exception = "ok include path" in comment_part.lower()

            # Find #include "..." statements
            match = INCLUDE_PATTERN.search(code_part)
            if match and not has_exception:
                include_path = match.group(1)

                # Skip system-style includes (should use <> not "" but handle anyway)
                if include_path.startswith("<"):
                    continue

                # Check if the include path is valid
                if not is_valid_include_path(include_path):
                    if is_relative_path(include_path):
                        msg = (
                            f'Invalid include path: #include "{include_path}" - '
                            f"Relative paths (../ or ./) are not allowed. "
                            f"Use paths relative to src/ instead "
                            f'(e.g., "fl/foo.h" or "platforms/bar.h"). '
                            f"Add '// ok include path' comment to suppress."
                        )
                        violations.append((line_number, msg))
                    elif is_fastled_platform_relative(include_path):
                        # This is a FastLED platform include without "platforms/" prefix
                        msg = (
                            f'Invalid include path: #include "{include_path}" - '
                            f'FastLED platform includes must use "platforms/" prefix. '
                            f'Use #include "platforms/{include_path}" instead. '
                            f"Add '// ok include path' comment to suppress."
                        )
                        violations.append((line_number, msg))
                    else:
                        # Check for likely typos of valid prefixes
                        typo_correction = get_typo_suggestion(include_path)
                        if typo_correction:
                            typo_prefix = include_path.split("/")[0] + "/"
                            rest_of_path = include_path[len(typo_prefix) :]
                            corrected_path = typo_correction + rest_of_path
                            msg = (
                                f'Invalid include path: #include "{include_path}" - '
                                f'"{typo_prefix}" looks like a typo. '
                                f'Use #include "{corrected_path}" instead. '
                                f"Add '// ok include path' comment to suppress."
                            )
                            violations.append((line_number, msg))
                        # Note: If it's not a relative path, not a known FastLED platform dir,
                        # and not a known typo, we assume it's an external SDK header we don't
                        # know about, so we skip it

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run include paths checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = IncludePathsChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src" / "fl"),
            str(PROJECT_ROOT / "src" / "platforms"),
        ],
        "Found invalid include paths",
        extensions=[".cpp", ".h", ".hpp", ".c"],
    )


if __name__ == "__main__":
    main()
