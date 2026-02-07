#!/usr/bin/env python3
"""Checker to detect native platform defines and suggest FL_IS_* equivalents.

Two-phase scanning:
  1. Fast scan: skip files that don't contain any preprocessor conditionals
  2. Slow scan: line-by-line parse with comment tracking, only flags defines
     on #if/#ifdef/#ifndef/#elif lines.
"""

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


PLATFORMS_ROOT = PROJECT_ROOT / "src" / "platforms"

# Mapping of native compiler defines to their FL_IS_* equivalents.
# Each native define maps to the most specific FL_IS_* group it belongs to.
# Source: src/platforms/avr/is_avr.h
NATIVE_TO_MODERN_DEFINES: dict[str, str] = {
    # ── General AVR ──────────────────────────────────────────────────
    "__AVR__": "FL_IS_AVR",
    # ── ATmega328P family (Arduino UNO/Nano) → FL_IS_AVR_ATMEGA_328P
    "__AVR_ATmega328P__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega328PB__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega328__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega168P__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega168__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega8__": "FL_IS_AVR_ATMEGA_328P",
    "__AVR_ATmega8A__": "FL_IS_AVR_ATMEGA_328P",
    # ── ATmega family (classic Timer1-based) → FL_IS_AVR_ATMEGA ─────
    "__AVR_ATmega2560__": "FL_IS_AVR_ATMEGA_2560",
    "__AVR_ATmega1280__": "FL_IS_AVR_ATMEGA_2560",
    "__AVR_ATmega32U4__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega1284__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega1284P__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega644P__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega644__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega32__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega16__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega128__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega64__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega32U2__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega16U2__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega8U2__": "FL_IS_AVR_ATMEGA",
    "__AVR_AT90USB1286__": "FL_IS_AVR_ATMEGA",
    "__AVR_AT90USB646__": "FL_IS_AVR_ATMEGA",
    "__AVR_AT90USB162__": "FL_IS_AVR_ATMEGA",
    "__AVR_AT90USB82__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega128RFA1__": "FL_IS_AVR_ATMEGA",
    "__AVR_ATmega256RFR2__": "FL_IS_AVR_ATMEGA",
    # ── megaAVR 0-series/1-series → FL_IS_AVR_MEGAAVR ───────────────
    "__AVR_ATmega4809__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega4808__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega3209__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega3208__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega1609__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega1608__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega809__": "FL_IS_AVR_MEGAAVR",
    "__AVR_ATmega808__": "FL_IS_AVR_MEGAAVR",
    # ── ATtiny family → FL_IS_AVR_ATTINY ─────────────────────────────
    # Classic ATtiny
    "__AVR_ATtiny13__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny13A__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny24__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny44__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny84__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny25__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny45__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny85__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny48__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny87__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny88__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny167__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny261__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny441__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny841__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny861__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny2313__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny2313A__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtiny4313__": "FL_IS_AVR_ATTINY",
    "__AVR_ATtinyX41__": "FL_IS_AVR_ATTINY",
    # Modern tinyAVR (0-series/1-series) with PORT_t registers
    "__AVR_ATtiny202__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny204__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny212__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny214__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny402__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny404__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny406__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny407__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny412__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny414__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny416__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny417__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny1604__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny1616__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny3216__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtiny3217__": "FL_IS_AVR_ATTINY_MODERN",
    # Generic tinyAVR wildcard defines
    "__AVR_ATtinyxy4__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtinyxy6__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtinyxy7__": "FL_IS_AVR_ATTINY_MODERN",
    "__AVR_ATtinyxy2__": "FL_IS_AVR_ATTINY_MODERN",
    # Arduino board defines for ATtiny-class boards
    "ARDUINO_AVR_DIGISPARK": "FL_IS_AVR_ATTINY",
    "ARDUINO_AVR_DIGISPARKPRO": "FL_IS_AVR_ATTINY",
    "IS_BEAN": "FL_IS_AVR_ATTINY",
}

# Regex matching preprocessor conditional directives:
#   #if, #ifdef, #ifndef, #elif (with optional whitespace after #)
_PREPROCESSOR_CONDITIONAL_RE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b")

# Fast-scan pattern: does the raw text contain any preprocessor conditional keyword?
# This is intentionally loose — false positives are fine, false negatives are not.
_FAST_HAS_CONDITIONAL_RE = re.compile(r"#\s*(?:if|ifdef|ifndef|elif)\b")


def _fast_scan_dominated(content: str) -> bool:
    """Return True if the file *might* contain a flaggable violation.

    This is the fast path. It may return True for files that ultimately have
    zero violations (false positive), but it must never return False for a
    file that has real violations (no false negatives).

    The check is: file contains at least one preprocessor conditional AND
    at least one of the native defines we care about.
    """
    if not _FAST_HAS_CONDITIONAL_RE.search(content):
        return False
    for native_define in NATIVE_TO_MODERN_DEFINES:
        if native_define in content:
            return True
    return False


class NativePlatformDefinesChecker(FileContentChecker):
    """Checker class for native platform defines in platform-specific directories."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for native platform defines."""
        # Check files in src/platforms directory and its subdirectories
        if not file_path.startswith(str(PLATFORMS_ROOT)):
            return False

        # Check file extension - .h, .cpp, .hpp files
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        path_obj = Path(file_path)
        file_name = path_obj.name

        # Skip the is_*.h detection headers - they MUST use native defines
        # These files convert native compiler defines into FL_IS_* defines
        if path_obj.name.startswith("is_"):
            return False

        # Skip core detection headers - they detect which Arduino core is in use
        if "core_detection" in file_name.lower():
            return False

        # Skip dispatch headers at src/platforms/*.h root level
        if path_obj.parent == PLATFORMS_ROOT:
            return False

        # Skip platform-level dispatch headers

        # Pattern 1: fastpin_*.h — all fastpin files are pin dispatch headers
        if file_name.startswith("fastpin_"):
            return False

        # Pattern 2: led_sysdefs_<platform>.h
        if file_name.startswith("led_sysdefs_") and file_name.count("_") <= 2:
            return False

        # Pattern 3: Explicit dispatcher files
        if "dispatch" in file_name.lower() or file_name == "fastpin_legacy.h":
            return False

        # Pattern 4: Compile test files
        if "compile_test" in file_name.lower():
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for native platform defines in preprocessor conditionals.

        Uses two-phase scanning:
          1. Fast scan — bail immediately if file can't possibly have violations.
          2. Slow scan — precise line-by-line parse with comment tracking.
        """
        # --- Phase 1: fast scan ---
        if not _fast_scan_dominated(file_content.content):
            return []

        # --- Phase 2: slow scan ---
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            stripped = line.strip()

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Only check preprocessor conditional directives
            if not _PREPROCESSOR_CONDITIONAL_RE.match(stripped):
                continue

            # Remove trailing comment before checking
            code_part = line.split("//")[0]

            for native_define, modern_define in NATIVE_TO_MODERN_DEFINES.items():
                pattern = r"\b" + re.escape(native_define) + r"\b"
                if re.search(pattern, code_part):
                    message = (
                        f"Native platform define '{native_define}' found in "
                        f"preprocessor conditional. "
                        f"Use '{modern_define}' instead."
                    )
                    violations.append((line_number, message))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run native platform defines checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = NativePlatformDefinesChecker()
    run_checker_standalone(
        checker,
        [str(PLATFORMS_ROOT)],
        "Found native platform defines in src/platforms",
    )


if __name__ == "__main__":
    main()
