#!/usr/bin/env python3
"""Checker to verify files using FL_IS_* macros include the appropriate is_*.h header.

Each file using FL_IS_* macros in preprocessor conditionals must directly include
the appropriate is_*.h detection header or is_platform.h (the root dispatcher).
"""

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


PLATFORMS_ROOT = PROJECT_ROOT / "src" / "platforms"

# FL_IS_* prefix → required is_*.h header filename.
# Sorted by longest prefix first for matching.
FL_IS_PREFIX_TO_HEADER: dict[str, str] = {
    "FL_IS_AVR": "is_avr.h",
    "FL_IS_ESP": "is_esp.h",
    "FL_IS_ARM": "is_arm.h",
    "FL_IS_STM32": "is_stm32.h",
    "FL_IS_TEENSY": "is_teensy.h",
    "FL_IS_SAMD": "is_samd.h",
    "FL_IS_SAM": "is_sam.h",
    "FL_IS_RP": "is_rp.h",
    "FL_IS_NRF52": "is_nrf52.h",
    "FL_IS_RENESAS": "is_renesas.h",
    "FL_IS_SILABS": "is_silabs.h",
    "FL_IS_APOLLO3": "is_apollo3.h",
    "FL_IS_POSIX": "is_posix.h",
    "FL_IS_WIN": "is_win.h",
    "FL_IS_WASM": "is_wasm.h",
    "FL_IS_STUB": "is_stub.h",
}

# Pre-sorted list of prefixes, longest first (for longest-prefix matching)
_SORTED_PREFIXES = sorted(FL_IS_PREFIX_TO_HEADER.keys(), key=len, reverse=True)

# Regex to find FL_IS_* macro names
_FL_IS_RE = re.compile(r"\bFL_IS_\w+")

# Regex to match preprocessor conditional directives
_PREPROC_CONDITIONAL_RE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b")

# Regex to extract include path
_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"](.+?)[>"]')


def _get_required_header(fl_is_macro: str) -> str | None:
    """Map an FL_IS_* macro to its required is_*.h header via longest-prefix match."""
    for prefix in _SORTED_PREFIXES:
        if fl_is_macro.startswith(prefix):
            return FL_IS_PREFIX_TO_HEADER[prefix]
    return None


def _extract_fl_is_macros_on_preprocessor_lines(lines: list[str]) -> set[str]:
    """Extract all FL_IS_* macros used on preprocessor conditional lines."""
    macros: set[str] = set()
    in_block_comment = False

    for line in lines:
        if "/*" in line:
            in_block_comment = True
        if "*/" in line:
            in_block_comment = False
            continue
        if in_block_comment:
            continue

        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        if not _PREPROC_CONDITIONAL_RE.match(stripped):
            continue

        code_part = line.split("//")[0]
        for m in _FL_IS_RE.findall(code_part):
            macros.add(m)

    return macros


def _extract_included_filenames(lines: list[str]) -> set[str]:
    """Extract the basename of every #include'd file."""
    includes: set[str] = set()
    for line in lines:
        m = _INCLUDE_RE.match(line)
        if m:
            inc_path = m.group(1)
            inc_name = inc_path.rsplit("/", 1)[-1]
            includes.add(inc_name)
    return includes


class IsHeaderIncludeChecker(FileContentChecker):
    """Checker that verifies FL_IS_* macro users include the right is_*.h header."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(PLATFORMS_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        file_name = Path(file_path).name

        # Skip is_*.h detection headers — they define FL_IS_* macros
        if file_name.startswith("is_"):
            return False

        # Skip compile_test files
        if "compile_test" in file_name.lower():
            return False

        # Skip core_detection.h — it defines FL_IS_STM32_* core macros
        if "core_detection" in file_name.lower():
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        fl_is_macros = _extract_fl_is_macros_on_preprocessor_lines(file_content.lines)
        if not fl_is_macros:
            return []

        included_names = _extract_included_filenames(file_content.lines)
        has_is_platform = "is_platform.h" in included_names

        # Determine which headers are required
        required_headers: dict[
            str, set[str]
        ] = {}  # header → set of FL_IS_* macros needing it
        for macro in fl_is_macros:
            header = _get_required_header(macro)
            if header:
                required_headers.setdefault(header, set()).add(macro)

        violations: list[tuple[int, str]] = []
        for header, macros in sorted(required_headers.items()):
            if header in included_names or has_is_platform:
                continue
            macros_str = ", ".join(sorted(macros))
            message = (
                f"File uses {macros_str} but does not include '{header}' "
                f"or 'is_platform.h'. Add: #include \"{header}\""
            )
            violations.append((0, message))

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run is-header include checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = IsHeaderIncludeChecker()
    run_checker_standalone(
        checker,
        [str(PLATFORMS_ROOT)],
        "FL_IS_* macros have required is_*.h includes",
    )


if __name__ == "__main__":
    main()
