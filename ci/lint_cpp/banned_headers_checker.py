# pyright: reportUnknownMemberType=false
"""Checker for banned standard library headers.

FastLED uses custom fl/ alternatives instead of standard library headers
to ensure consistent behavior across all platforms and reduce code size.

This checker also detects private libc++ headers (pattern: #include "__*").
libc++ private headers (starting with double underscore like __algorithm/min.h)
are internal implementation details and should never be included directly.
They are unstable across libc++ versions and will break portability.
"""

import re
from dataclasses import dataclass

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


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
# PRIVATE LIBC++ HEADER MAPPINGS
# ============================================================================
# Mapping from private libc++ headers to FastLED equivalents
# These headers (starting with __) are internal implementation details of libc++
# and should never be included directly. They are unstable across libc++ versions.
#
# This checker is designed to catch IWYU (Include What You Use) mistakes that
# insert private headers and to prevent them from being committed to the codebase.

PRIVATE_LIBCPP_HEADER_MAPPINGS = {
    "__algorithm": '"fl/stl/algorithm.h"',
    "__atomic": '"fl/stl/atomic.h"',
    "__chrono": '"fl/stl/thread.h"',
    "__functional": '"fl/stl/functional.h"',
    "__fwd/string": '"fl/stl/string.h"',
    "__iterator": '"fl/stl/iterator.h"',
    "__numeric": '"fl/stl/algorithm.h"',
    "__ostream": '"fl/stl/ostream.h"',
    "__random": '"fl/stl/random.h"',
    "__thread": '"fl/stl/thread.h"',
    "__type_traits": '"fl/stl/type_traits.h"',
    "__utility": '"fl/stl/utility.h"',
    "__vector": '"fl/stl/vector.h"',
    "__hash_table": '"fl/stl/unordered_map.h"',
    "__tree": '"fl/stl/map.h"',
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
            "platforms/wasm/new.h", "Platform-specific placement new wrapper"
        ),
        HeaderException(
            "platforms/arm/new.h", "Platform-specific placement new wrapper"
        ),
        HeaderException(
            "platforms/esp/new.h", "Platform-specific placement new wrapper"
        ),
        HeaderException(
            "platforms/shared/new.h", "Platform-specific placement new wrapper"
        ),
    ],
    # Core system definitions and platform headers
    "Arduino.h": [
        HeaderException(
            "led_sysdefs.h", "Core system definitions need platform headers"
        ),
        HeaderException("fl/stl/time.cpp", "Platform-specific time implementation"),
        # platforms/ - .cpp files that need Arduino.h for platform functionality
        HeaderException("platforms/avr/io_avr.cpp", "AVR platform I/O implementation"),
        HeaderException("platforms/stub/Arduino.cpp", "Stub Arduino implementation"),
        HeaderException(
            "platforms/esp/32/drivers/cled.cpp", "ESP32 LED driver implementation"
        ),
        HeaderException(
            "platforms/arm/d21/spi_hw_2_samd21.cpp",
            "SAMD21 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/d51/spi_hw_4_samd51.cpp",
            "SAMD51 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/d51/spi_hw_2_samd51.cpp",
            "SAMD51 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/stm32/spi_hw_2_stm32.cpp",
            "STM32 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/stm32/spi_hw_8_stm32.cpp",
            "STM32 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/stm32/spi_hw_4_stm32.cpp",
            "STM32 SPI hardware implementation",
        ),
        HeaderException(
            "platforms/arm/rp/rp2040/delay.cpp", "RP2040 delay implementation"
        ),
        # platforms/ - Legacy headers that need Arduino.h (TODO: refactor these)
        HeaderException(
            "platforms/apollo3/fastpin_apollo3.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "platforms/apollo3/clockless_apollo3.h",
            "TODO: Refactor to remove dependency",
        ),
        HeaderException(
            "platforms/arm/sam/fastspi_arm_sam.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "platforms/arm/rp/rpcommon/led_sysdefs_rp_common.h",
            "TODO: Refactor to remove dependency",
        ),
        HeaderException(
            "platforms/io_teensy_lc.h", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "platforms/arm/teensy/common/io_teensy_lc.h",
            "TODO: Refactor to remove dependency",
        ),
        HeaderException(
            "platforms/arduino/audio_input.hpp", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "platforms/esp/io_esp_arduino.hpp", "TODO: Refactor to remove dependency"
        ),
        HeaderException(
            "platforms/wasm/led_sysdefs_wasm.h", "WASM wrapper for stub Arduino.h"
        ),
        HeaderException(
            "platforms/wasm/compiler/Arduino.h", "WASM stub Arduino.h implementation"
        ),
        # third_party/ - External libraries that need Arduino.h
        HeaderException(
            "third_party/ezws2812/ezWS2812.h", "Third-party SPI driver library"
        ),
        HeaderException("third_party/arduinojson/json.hpp", "Third-party JSON library"),
        HeaderException(
            "third_party/object_fled/src/ObjectFLEDDmaManager.h",
            "Third-party DMA manager library",
        ),
    ],
    # Threading and synchronization
    "pthread.h": [
        HeaderException("fl/thread_local.h", "Platform threading abstraction"),
    ],
    "mutex": [
        HeaderException("fl/stl/mutex.h", "Wrapper for platform mutex"),
        HeaderException(
            "platforms/stub/thread_stub_stl.h", "STL threading wrapper for tests"
        ),
        HeaderException(
            "platforms/stub/mutex_stub_stl.h",
            "STL mutex wrapper for multithreaded platforms",
        ),
        HeaderException(
            "platforms/esp/32/mutex_esp32.h",
            "ESP32 mutex implementation using std::unique_lock",
        ),
        HeaderException(
            "platforms/arm/stm32/mutex_stm32.h",
            "STM32 mutex implementation using std::unique_lock",
        ),
        HeaderException(
            "platforms/arm/d21/mutex_samd.h",
            "SAMD mutex implementation using std::unique_lock",
        ),
        HeaderException(
            "platforms/arm/rp/mutex_rp.h",
            "RP2040/RP2350 mutex implementation using std::unique_lock",
        ),
    ],
    "thread": [
        HeaderException("platforms/stub/isr_stub.hpp", "Test ISR timing simulation"),
        HeaderException("platforms/stub/time_stub.cpp", "Test time simulation"),
        HeaderException(
            "platforms/stub/thread_stub_stl.h", "STL threading wrapper for tests"
        ),
        HeaderException("platforms/wasm/timer.cpp", "WASM timer implementation"),
    ],
    # I/O and streams
    "iostream": [
        HeaderException("platforms/stub/isr_stub.hpp", "Test ISR timing simulation"),
    ],
    # Algorithms and utilities
    "algorithm": [
        HeaderException("fl/stl/mutex.h", "Dependency from <string_view> in <mutex>"),
        HeaderException("platforms/stub/fs_stub.hpp", "Test filesystem implementation"),
    ],
    # Integer types
    "stdint.h": [
        HeaderException("fl/stl/cstdint.h", "Limit macros: INT8_MAX, UINT64_MAX, etc."),
        HeaderException("fl/stl/time.cpp", "Platform-specific time implementation"),
    ],
    # Time and timing
    "chrono": [
        HeaderException("fl/stl/time.cpp", "Platform-specific time implementation"),
        HeaderException("platforms/stub/isr_stub.hpp", "Test ISR timing simulation"),
        HeaderException("platforms/stub/time_stub.cpp", "Test time simulation"),
        HeaderException(
            "platforms/esp/32/condition_variable_esp32.h",
            "ESP32 condition_variable needs std::chrono types for C++11 compatibility",
        ),
        HeaderException(
            "platforms/esp/32/semaphore_esp32.h",
            "ESP32 semaphore needs std::chrono types for C++20 compatibility",
        ),
    ],
    # String operations
    "string.h": [
        HeaderException(
            "fl/stl/str.cpp", "C string implementation (memcpy, strlen, etc.)"
        )
    ],
    "stdlib.h": [
        HeaderException(
            "fl/stl/str.cpp", "C string implementation (malloc, free, etc.)"
        )
    ],
    # Math operations
    "math.h": [
        HeaderException(
            "fl/stl/math.cpp", "Platform math functions (sin, cos, sqrt, etc.)"
        ),
        HeaderException("fl/audio_reactive.cpp", "Audio FFT and signal processing"),
        HeaderException(
            "fl/colorutils.cpp", "Color interpolation and gamma correction"
        ),
        HeaderException("fl/transform.cpp", "Matrix transformations"),
        HeaderException("fl/xypath.cpp", "Path geometry calculations"),
        HeaderException("fl/xypath_impls.cpp", "Path rendering algorithms"),
        HeaderException("fl/xypath_renderer.cpp", "Path rendering algorithms"),
        HeaderException("fx/video/frame_interpolator.cpp", "Video frame interpolation"),
    ],
    "cmath": [
        HeaderException("platforms/wasm/timer.cpp", "WASM timer implementation"),
    ],
    # File operations
    "fstream": [
        HeaderException("platforms/stub/fs_stub.hpp", "Test filesystem implementation"),
    ],
    "cstdio": [
        HeaderException("platforms/stub/fs_stub.hpp", "Test filesystem implementation"),
        HeaderException("platforms/wasm/io_wasm.h", "WASM platform I/O implementation"),
        HeaderException(
            "platforms/wasm/compiler/wasm_pch.h", "WASM precompiled header"
        ),
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

        # Check if file is in excluded list (handles both Windows and Unix paths)
        if is_excluded_file(file_path):
            return False

        return True

    @classmethod
    def _matches_pattern(cls, file_path: str, pattern: str) -> bool:
        r"""Check if file path matches the pattern.

        Since all exception patterns are now exact file paths (no wildcards),
        this performs a simple exact match after normalizing path separators.

        Args:
            file_path: The file path to check (may use \ or / separators)
            pattern: The pattern to match (uses / separators)

        Returns:
            True if the file path matches the pattern
        """
        normalized_path = file_path.replace("\\", "/")

        # Exact match: pattern equals the full path or just the suffix
        # Examples:
        #   pattern="fl/stl/mutex.h" matches "src/fl/stl/mutex.h"
        #   pattern="inplacenew.h" matches "src/inplacenew.h"
        return pattern == normalized_path or normalized_path.endswith("/" + pattern)

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

    @staticmethod
    def _get_private_libcpp_header_recommendation(private_header: str) -> str:
        """Map a private libc++ header to its FastLED equivalent.

        Args:
            private_header: The private header name (e.g., "__algorithm/min.h")

        Returns:
            The recommended FastLED header to use instead
        """
        # Check for exact matches first (e.g., "__hash_table", "__tree")
        if private_header in PRIVATE_LIBCPP_HEADER_MAPPINGS:
            return PRIVATE_LIBCPP_HEADER_MAPPINGS[private_header]

        # Check for prefix matches (e.g., "__algorithm/*" -> "__algorithm")
        for prefix, recommendation in PRIVATE_LIBCPP_HEADER_MAPPINGS.items():
            if private_header.startswith(prefix):
                return recommendation

        # Unknown private header - recommend checking IWYU mapping file
        return '"appropriate FastLED header" (check ci/iwyu/stdlib.imp)'

    def _should_allow_bypass(self, file_path: str) -> bool:
        """Check if file type allows 'ok include' bypass.

        - .h files: NEVER allow bypass (header purity is critical)
        - .cpp/.cc files: Allow bypass with '// ok include' comment
        - .hpp files: Treat like .h files (never allow bypass)
        """
        return file_path.endswith(".cpp") or file_path.endswith(".cc")

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned headers and private libc++ headers."""
        violations: list[tuple[int, str]] = []

        # Determine file type and if bypass is allowed
        self._should_allow_bypass(file_content.path)
        file_ext = file_content.path.split(".")[-1]

        # Pattern to detect private libc++ headers
        # Matches: #include "__anything" or #include <__anything>
        # But NOT triple underscores or more (e.g., ___pixeltypes.h)
        # Examples:
        #   #include "__algorithm/min.h"  ✓ Match
        #   #include "__atomic/atomic.h"  ✓ Match
        #   #include <__algorithm/min.h>  ✓ Match
        #   #include "___pixeltypes.h"    ✗ No match (triple underscore)
        private_header_pattern = re.compile(r'#include\s+["<](__[^_][^">]*)[">]')

        # Check each line for banned headers and private libc++ headers
        for line_number, line in enumerate(file_content.lines, 1):
            # Skip comment lines
            stripped = line.strip()
            if stripped.startswith("//"):
                continue

            # Check for private libc++ headers (these are always banned)
            match = private_header_pattern.search(line)
            if match:
                header_name = match.group(1)
                public_header = self._get_private_libcpp_header_recommendation(
                    header_name
                )
                violations.append(
                    (
                        line_number,
                        f'Found private libc++ header "{header_name}" - Use {public_header} instead. '
                        f"Private libc++ headers (starting with __) are internal implementation details and should never be included directly. "
                        f"They are unstable across libc++ versions and break portability.",
                    )
                )

            # Check for regular banned headers (only if list is not empty)
            if len(self.banned_headers_list) == 0:
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
