# pyright: reportUnknownMemberType=false
"""Checker for banned standard library headers.

FastLED uses custom fl/ alternatives instead of standard library headers
to ensure consistent behavior across all platforms and reduce code size.
"""

from dataclasses import dataclass

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# ============================================================================
# CONFIGURATION CONSTANTS
# ============================================================================

ENABLE_PARANOID_GNU_HEADER_INSPECTION = False

if ENABLE_PARANOID_GNU_HEADER_INSPECTION:
    BANNED_HEADERS_ESP = ["esp32-hal.h"]
else:
    BANNED_HEADERS_ESP = []

BANNED_HEADERS_COMMON = [
    "pthread.h",
    "assert.h",
    "iostream",
    "stdio.h",
    "cstdio",
    "cstdlib",
    "vector",
    "list",
    "map",
    "set",
    "queue",
    "deque",
    "algorithm",
    "memory",
    "thread",
    "mutex",
    "chrono",
    "fstream",
    "sstream",
    "iomanip",
    "exception",
    "stdexcept",
    "typeinfo",
    "ctime",
    "cmath",
    "math.h",  # Ban math.h - use fl/math.h instead
    "complex",
    "valarray",
    "cfloat",
    "cassert",
    "cerrno",
    "cctype",
    "cwctype",
    "cstring",
    "cwchar",
    "cuchar",
    "cstdint",
    "stdint.h",
    "stddef.h",
    "cstddef",  # this certainly fails
    "string.h",  # Ban C string.h - use fl/str.h instead
    "type_traits",  # this certainly fails
    "new",  # Ban <new> except for placement new in inplacenew.h
]

BANNED_HEADERS_CORE = BANNED_HEADERS_COMMON + BANNED_HEADERS_ESP + ["Arduino.h"]

# Banned headers for platforms directory - no stdlib headers allowed in .h files
# Platform headers should use fl/ alternatives just like core FastLED code
BANNED_HEADERS_PLATFORMS = BANNED_HEADERS_COMMON


# ============================================================================
# HEADER RECOMMENDATIONS
# ============================================================================
# Suggestions for which fl/ alternative to use for each banned header

HEADER_RECOMMENDATIONS = {
    "pthread.h": "fl/stl/thread.h or fl/stl/mutex.h (depending on what you need)",
    "assert.h": "FL_CHECK or FL_ASSERT macros (check fl/compiler_control.h)",
    "iostream": "fl/stl/iostream.h or fl/str.h",
    "stdio.h": "fl/str.h for string operations",
    "cstdio": "fl/str.h for string operations",
    "cstdlib": "fl/stl/cstdlib.h or fl/string operations",
    "vector": "fl/stl/vector.h",
    "list": "fl/stl/list.h or fl/stl/vector.h",
    "map": "fl/stl/map.h (check fl/stl/hash_map.h for hash-based)",
    "set": "fl/stl/set.h",
    "queue": "fl/stl/queue.h or fl/stl/vector.h with manual queue semantics",
    "deque": "fl/stl/deque.h",
    "algorithm": "fl/stl/algorithm.h",
    "memory": "fl/stl/shared_ptr.h or fl/stl/unique_ptr.h",
    "thread": "fl/stl/thread.h",
    "mutex": "fl/stl/mutex.h",
    "chrono": "fl/time.h",
    "fstream": "fl/file.h or platform file operations",
    "sstream": "fl/stl/sstream.h",
    "iomanip": "fl/stl/iostream.h stream manipulators",
    "exception": "Use error codes or fl/stl/exception.h if available",
    "stdexcept": "Use error codes instead",
    "typeinfo": "Use fl/stl/type_traits.h or RTTI if unavoidable",
    "ctime": "fl/time.h",
    "cmath": "fl/math.h",
    "math.h": "fl/math.h",
    "complex": "Custom complex number class or fl/geometry.h",
    "valarray": "fl/stl/vector.h",
    "cfloat": "fl/numeric_limits.h or platform-specific headers",
    "cassert": "FL_CHECK macros from fl/compiler_control.h",
    "cerrno": "Error handling through return codes",
    "cctype": "Character classification (implement if needed)",
    "cwctype": "Wide character classification (implement if needed)",
    "cstring": "fl/str.h or fl/stl/cstring.h",
    "cwchar": "Wide character support (implement if needed)",
    "cuchar": "Character support (implement if needed)",
    "cstdint": "fl/stl/stdint.h or fl/stl/cstdint.h",
    "stdint.h": "fl/stl/stdint.h",
    "stddef.h": "fl/stl/stddef.h or fl/stl/cstddef.h",
    "cstddef": "fl/stl/cstddef.h",
    "string.h": "fl/str.h (or use extern declarations for memset/memcpy if only C functions needed)",
    "type_traits": "fl/stl/type_traits.h",
    "new": "Use stack allocation or custom allocators (placement new allowed in inplacenew.h)",
}


# ============================================================================
# EXCEPTION RULES
# ============================================================================
# All exceptions to banned header rules are defined here for easy maintenance.
# Optimized structure: Maps banned headers to list of HeaderException instances.
# This enables O(1) average-case lookup instead of O(n) iteration through all rules.


@dataclass(frozen=True)
class HeaderException:
    """Represents an exception to a banned header rule.

    Attributes:
        file_pattern: Glob pattern matching allowed file paths (e.g., "**/*.cpp", "inplacenew.h")
        reason: Human-readable explanation for why this exception exists
    """

    file_pattern: str
    reason: str


EXCEPTION_RULES: dict[str, list[HeaderException]] = {
    # Core placement new operator support
    "new": [
        HeaderException("inplacenew.h", "Placement new operator definition"),
        HeaderException(
            "platforms/**/new.h", "Platform-specific placement new wrapper"
        ),
    ],
    # Core system definitions and platform headers
    "Arduino.h": [
        HeaderException(
            "led_sysdefs.h", "Core system definitions need platform headers"
        ),
        HeaderException("**/stl/time.cpp", "Platform-specific time implementation"),
        HeaderException(
            "platforms/**/*.cpp", "Platform implementations can use Arduino.h"
        ),
        # platforms/ - Legacy headers that need Arduino.h (TODO: refactor these)
        HeaderException("**/fastpin_apollo3.h", "TODO: Refactor to remove dependency"),
        HeaderException(
            "**/clockless_apollo3.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException("**/fastspi_arm_sam.h", "TODO: Refactor to remove dependency"),
        HeaderException(
            "**/stm32_gpio_timer_helpers.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "**/led_sysdefs_rp_common.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException("**/io_teensy_lc.h", "TODO: Refactor to remove dependency"),
        HeaderException("**/audio_input.hpp", "TODO: Refactor to remove dependency"),
        HeaderException("**/io_esp_arduino.hpp", "TODO: Refactor to remove dependency"),
        HeaderException("third_party/**/*", "Third-party library needs Arduino.h"),
    ],
    # Threading and synchronization
    "pthread.h": [
        HeaderException("**/thread_local.h", "Platform threading abstraction"),
    ],
    "mutex": [
        HeaderException("**/stl/mutex.h", "Wrapper for platform mutex"),
        HeaderException("**/thread_stub_stl.h", "STL threading wrapper for tests"),
    ],
    "thread": [
        HeaderException("**/isr_stub.cpp", "Test ISR timing simulation"),
        HeaderException("**/isr_stub.hpp", "Test ISR timing simulation"),
        HeaderException("**/time_stub.cpp", "Test time simulation"),
        HeaderException("**/thread_stub_stl.h", "STL threading wrapper for tests"),
        HeaderException("wasm/**/timer.cpp", "WASM timer implementation"),
    ],
    # I/O and streams
    "iostream": [
        HeaderException("**/stub_main.cpp", "Testing entry point needs stdout"),
        HeaderException("**/isr_stub.cpp", "Test ISR timing simulation"),
        HeaderException("**/isr_stub.hpp", "Test ISR timing simulation"),
        HeaderException("**/time_stub.cpp", "Test time simulation"),
    ],
    # Algorithms and utilities
    "algorithm": [
        HeaderException("**/stl/mutex.h", "Dependency from <string_view> in <mutex>"),
        HeaderException("**/fs_stub.hpp", "Test filesystem implementation"),
    ],
    # Integer types
    "stdint.h": [
        HeaderException("**/stl/cstdint.h", "Limit macros: INT8_MAX, UINT64_MAX, etc."),
        HeaderException("**/stl/time.cpp", "Platform-specific time implementation"),
    ],
    # Time and timing
    "chrono": [
        HeaderException("**/stl/time.cpp", "Platform-specific time implementation"),
        HeaderException("**/isr_stub.cpp", "Test ISR timing simulation"),
        HeaderException("**/isr_stub.hpp", "Test ISR timing simulation"),
        HeaderException("**/time_stub.cpp", "Test time simulation"),
    ],
    # String operations
    "string.h": [
        HeaderException(
            "**/stl/str.cpp", "C string implementation (memcpy, strlen, etc.)"
        ),
        HeaderException("led_strip/**/*", "Third-party Espressif library"),
    ],
    "stdlib.h": [
        HeaderException(
            "**/stl/str.cpp", "C string implementation (malloc, free, etc.)"
        ),
        HeaderException("led_strip/**/*", "Third-party Espressif library"),
    ],
    "sys/cdefs.h": [
        HeaderException("led_strip/**/*", "Third-party Espressif library"),
    ],
    # Math operations
    "math.h": [
        HeaderException(
            "**/stl/math.cpp", "Platform math functions (sin, cos, sqrt, etc.)"
        ),
        HeaderException("**/audio_reactive.cpp", "Audio FFT and signal processing"),
        HeaderException(
            "**/colorutils.cpp", "Color interpolation and gamma correction"
        ),
        HeaderException("**/transform.cpp", "Matrix transformations"),
        HeaderException("**/xypath.cpp", "Path geometry calculations"),
        HeaderException("**/xypath_impls.cpp", "Path rendering algorithms"),
        HeaderException("**/xypath_renderer.cpp", "Path rendering algorithms"),
        HeaderException("**/beat_detector.cpp", "Audio beat detection FFT"),
        HeaderException("**/frame_interpolator.cpp", "Video frame interpolation"),
    ],
    "cmath": [
        HeaderException("wasm/**/timer.cpp", "WASM timer implementation"),
    ],
    # File operations
    "fstream": [
        HeaderException("**/fs_stub.hpp", "Test filesystem implementation"),
    ],
    "cstdio": [
        HeaderException("**/fs_stub.hpp", "Test filesystem implementation"),
        HeaderException("wasm/**/*.h", "WASM platform I/O implementation"),
    ],
    # WASM-specific headers
    "vector": [
        HeaderException("wasm/**/*.h", "WASM platform I/O implementation"),
    ],
    "stdio.h": [
        HeaderException("wasm/**/*.h", "WASM platform I/O implementation"),
    ],
}


class BannedHeadersChecker(FileContentChecker):
    """Checker class for banned headers."""

    def __init__(self, banned_headers_list: list[str], strict_mode: bool = False):
        """Initialize with the list of banned headers to check for.

        Args:
            banned_headers_list: List of headers to ban
            strict_mode: If True, do not allow "// ok include" bypass (for fl/ directory)
        """
        self.banned_headers_list = banned_headers_list
        self.strict_mode = strict_mode
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for banned headers."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    @classmethod
    def _matches_pattern(cls, file_path: str, pattern: str) -> bool:
        """Check if file path matches a wildcard pattern.

        Supports:
        - Exact filename matches: "file.cpp"
        - Wildcard suffix: "*.cpp" (any .cpp file)
        - Directory wildcard: "platforms/*/*.cpp" (any .cpp in platforms subdirs)
        - Recursive wildcard: "platforms/**/*.cpp" (any .cpp in platforms tree)
        """
        normalized_path = file_path.replace("\\", "/")

        # Exact match
        if pattern == normalized_path or normalized_path.endswith("/" + pattern):
            return True

        # Recursive wildcard: "dir/**/*.ext" matches "dir/a/b/c/file.ext"
        if "**" in pattern:
            parts = pattern.split("/")
            pattern_prefix = "/".join(parts[: parts.index("**")])
            pattern_suffix = parts[-1] if parts[-1] != "**" else ""

            if pattern_prefix and pattern_prefix not in normalized_path:
                return False

            if pattern_suffix:
                if pattern_suffix.startswith("*"):
                    # Suffix match like "*.cpp"
                    return normalized_path.endswith(pattern_suffix[1:])
                else:
                    # Exact filename match
                    return normalized_path.endswith("/" + pattern_suffix)
            return True

        # Single-level wildcard: "platforms/*/file.cpp"
        if "*" in pattern:
            parts = pattern.split("/")
            path_parts = normalized_path.split("/")

            # Check if path has enough parts
            if len(path_parts) < len(parts):
                return False

            # Match from the end (most specific part)
            for i in range(1, len(parts) + 1):
                pattern_part = parts[-i]
                path_part = path_parts[-i]

                if pattern_part == "*":
                    continue  # Wildcard matches anything
                elif pattern_part.startswith("*"):
                    # Suffix match like "*.cpp"
                    if not path_part.endswith(pattern_part[1:]):
                        return False
                elif pattern_part != path_part:
                    return False

            return True

        # Substring match for simple patterns
        return pattern in normalized_path

    def _is_allowed_exception(self, file_path: str, header: str) -> bool:
        """Check if this is an allowed exception to the banned header rule.

        Optimized for O(1) average-case lookup instead of O(n) iteration.
        """
        # O(1) lookup: Check if header has any exceptions defined
        if header not in EXCEPTION_RULES:
            return False

        # Check each file pattern for this header
        for exception in EXCEPTION_RULES[header]:
            if self._matches_pattern(file_path, exception.file_pattern):
                return True

        return False

    def _get_recommendation(self, header: str) -> str:
        """Get recommendation for a banned header."""
        return HEADER_RECOMMENDATIONS.get(
            header, "Use fl/ alternatives instead of standard library headers"
        )

    def _should_allow_bypass(self, file_path: str) -> bool:
        """Check if file type allows 'ok include' bypass.

        - .h files: NEVER allow bypass (header purity is critical)
        - .cpp/.cc files: Allow bypass with '// ok include' comment
        - .hpp files: Treat like .h files (never allow bypass)
        """
        return file_path.endswith(".cpp") or file_path.endswith(".cc")

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned headers."""
        violations: list[tuple[int, str]] = []

        if len(self.banned_headers_list) == 0:
            return []

        # Determine file type and if bypass is allowed
        can_bypass = self._should_allow_bypass(file_content.path)
        file_ext = file_content.path.split(".")[-1]

        # Check each line for banned headers
        for line_number, line in enumerate(file_content.lines, 1):
            if line.strip().startswith("//"):
                continue

            for header in self.banned_headers_list:
                if f"#include <{header}>" in line or f'#include "{header}"' in line:
                    # Check if this is an allowed exception
                    if self._is_allowed_exception(file_content.path, header):
                        continue

                    # Check if bypass is allowed and present
                    has_bypass_comment = (
                        "// ok include" in line or "// OK include" in line
                    )

                    # Header files: NEVER allow bypass
                    if file_ext in ("h", "hpp"):
                        violations.append(
                            (
                                line_number,
                                f"Found banned header '{header}' - Use {self._get_recommendation(header)} instead (banned in header files). "
                                f"Try moving the #include to the corresponding .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace.",
                            )
                        )
                    # Strict mode: no bypass allowed (for fl/ directory)
                    elif self.strict_mode and not has_bypass_comment:
                        violations.append(
                            (
                                line_number,
                                f"Found banned header '{header}' - Use {self._get_recommendation(header)} instead (strict mode, no bypass allowed). "
                                f"Try moving the #include to a .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace.",
                            )
                        )
                    # Non-strict: allow bypass
                    elif not self.strict_mode and not has_bypass_comment:
                        violations.append(
                            (
                                line_number,
                                f"Found banned header '{header}' - Use {self._get_recommendation(header)} instead. "
                                f"Try finding the equivalent header in fl/stl in the fl:: namespace, or if you need this header in a .cpp file, add '// ok include' comment to suppress this warning.",
                            )
                        )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run banned headers checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = BannedHeadersChecker(
        banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True
    )
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found banned headers",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
