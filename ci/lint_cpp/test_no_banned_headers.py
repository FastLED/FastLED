# pyright: reportUnknownMemberType=false
import os
import unittest
from typing import Callable, List

from ci.util.check_files import (
    EXCLUDED_FILES,
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

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


class BannedHeadersChecker(FileContentChecker):
    """Checker class for banned headers."""

    # Header recommendations
    HEADER_RECOMMENDATIONS = {
        "pthread.h": "fl/thread.h or fl/mutex.h (depending on what you need)",
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
        "thread": "fl/thread.h",
        "mutex": "fl/mutex.h",
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

    def __init__(self, banned_headers_list: list[str], strict_mode: bool = False):
        """Initialize with the list of banned headers to check for.

        Args:
            banned_headers_list: List of headers to ban
            strict_mode: If True, do not allow "// ok include" bypass (for fl/ directory)
        """
        self.banned_headers_list = banned_headers_list
        self.strict_mode = strict_mode

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for banned headers."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def _is_allowed_exception(self, file_path: str, header: str) -> bool:
        """Check if this is an allowed exception to the banned header rule."""
        # Allow <new> in inplacenew.h for placement new operator
        if header == "new" and "inplacenew.h" in file_path:
            return True

        # Allow <new> in platform new.h files - they are wrappers for placement new
        if header == "new" and file_path.endswith("new.h"):
            if "/platforms/" in file_path.replace("\\", "/"):
                return True

        # Allow Arduino.h in led_sysdefs.h - core system definitions need platform headers
        if header == "Arduino.h" and "led_sysdefs.h" in file_path:
            return True

        # For fl/ directory, allow specific platform headers that have no alternatives
        # These are genuinely needed for platform-specific implementations
        if "/fl/" in file_path.replace("\\", "/"):
            # Allow iostream in stub_main.cpp for testing entry point
            if header == "iostream" and "stub_main.cpp" in file_path:
                return True

            # Allow pthread.h in thread_local.h - needed for platform threading abstraction
            if header == "pthread.h" and "thread_local.h" in file_path:
                return True

            # Allow mutex in mutex.h - this is the wrapper for platform mutex
            if header == "mutex" and "mutex.h" in file_path:
                return True

            # Allow algorithm in mutex.h - needed for <string_view> dependency in <mutex>
            if header == "algorithm" and "mutex.h" in file_path:
                return True

            # Allow stdint.h in cstdint.h - needed for limit macros (INT8_MAX, UINT64_MAX, etc.)
            if header == "stdint.h" and "cstdint.h" in file_path:
                return True

            # Allow math.h in math implementation files (cpp files)
            if header == "math.h" and file_path.endswith(".cpp"):
                # math.h is only used in implementation files for platform math functions
                if any(
                    x in file_path
                    for x in [
                        "math.cpp",
                        "audio_reactive.cpp",
                        "colorutils.cpp",
                        "transform.cpp",
                        "xypath.cpp",
                        "xypath_impls.cpp",
                        "xypath_renderer.cpp",
                    ]
                ):
                    return True

            # Allow platform-specific time headers in time.cpp
            if "time.cpp" in file_path:
                if header in {"stdint.h", "Arduino.h", "chrono"}:
                    return True

            # Allow C library headers in str.cpp for string implementation
            if "str.cpp" in file_path:
                if header in {"string.h", "stdlib.h"}:
                    return True

        # Platform-specific headers need Arduino.h
        if "/platforms/" in file_path.replace("\\", "/"):
            if header == "Arduino.h":
                return True

        # Espressif LED strip library code needs C library headers
        if "/led_strip/" in file_path.replace("\\", "/"):
            if header in {"string.h", "stdlib.h", "sys/cdefs.h"}:
                return True

        # Testing stub implementations need stdlib headers for file/thread operations
        if "/stub/" in file_path.replace("\\", "/") or "/wasm/" in file_path.replace(
            "\\", "/"
        ):
            # fs_stub.hpp needs algorithm, fstream, cstdio for test filesystem
            if "fs_stub" in file_path and header in {"algorithm", "fstream", "cstdio"}:
                return True
            # isr_stub.cpp and time_stub.cpp need chrono, thread, iostream for testing
            if ("isr_stub" in file_path or "time_stub" in file_path) and header in {
                "chrono",
                "thread",
                "iostream",
            }:
                return True
            # WASM platform implementations need stdlib headers for I/O and threading
            if "/wasm/" in file_path.replace("\\", "/"):
                # WASM platform headers need vector, cstdio, stdio.h for platform-specific implementation
                if file_path.endswith(".h"):
                    if header in {"vector", "cstdio", "stdio.h"}:
                        return True
                # WASM timer.cpp needs thread and cmath for timer implementation
                elif "timer.cpp" in file_path and header in {"thread", "cmath"}:
                    return True

        # FX audio/video files need math.h for specialized calculations
        if "/fx/" in file_path.replace("\\", "/"):
            if header == "math.h" and file_path.endswith(".cpp"):
                if any(
                    x in file_path
                    for x in ["beat_detector.cpp", "frame_interpolator.cpp"]
                ):
                    return True

        return False

    def _get_recommendation(self, header: str) -> str:
        """Get recommendation for a banned header."""
        return self.HEADER_RECOMMENDATIONS.get(
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
        failings: list[str] = []

        if len(self.banned_headers_list) == 0:
            return failings

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
                        failings.append(
                            f"Found banned header '{header}' in {file_content.path}:{line_number} - "
                            f"Use {self._get_recommendation(header)} instead (banned in header files)"
                        )
                    # Strict mode: no bypass allowed (for fl/ directory)
                    elif self.strict_mode and not has_bypass_comment:
                        failings.append(
                            f"Found banned header '{header}' in {file_content.path}:{line_number} - "
                            f"Use {self._get_recommendation(header)} instead (strict mode, no bypass allowed)"
                        )
                    # Non-strict: allow bypass
                    elif not self.strict_mode and not has_bypass_comment:
                        failings.append(
                            f"Found banned header '{header}' in {file_content.path}:{line_number} - "
                            f"Use {self._get_recommendation(header)} instead"
                        )

        return failings


def _test_no_banned_headers(
    test_directories: list[str],
    banned_headers_list: list[str],
    on_fail: Callable[[str], None],
    strict_mode: bool = False,
) -> None:
    """Searches through the program files to check for banned headers.

    Args:
        test_directories: Directories to check
        banned_headers_list: List of banned headers
        on_fail: Callback to call on failure
        strict_mode: If True, do not allow "// ok include" bypass (for fl/ directory)
    """
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = BannedHeadersChecker(banned_headers_list, strict_mode=strict_mode)

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results for banned headers checker
    all_failings = results.get("BannedHeadersChecker", []) or []

    if all_failings:
        msg = f"Found {len(all_failings)} banned header(s): \n" + "\n".join(
            all_failings
        )
        for failing in all_failings:
            print(failing)

        on_fail(msg)
    else:
        print("No banned headers found.")


class TestNoBannedHeaders(unittest.TestCase):
    def test_no_banned_headers_fl(self) -> None:
        """Searches through fl/ directory with STRICT enforcement - no bypass allowed."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n\n"
                "STRICT MODE: fl/ directory headers must not include stdlib headers.\n"
                "- .h/.hpp files: NEVER allow stdlib headers (header purity is critical)\n"
                "- .cpp files: STRICT mode - no '// ok include' bypass allowed\n"
                "Fix by removing the banned header or use fl/ alternatives.\n"
                "See HEADER_RECOMMENDATIONS for suggested replacements."
            )

        # Test fl/ directory with strict mode
        test_directories = [
            os.path.join(SRC_ROOT, "fl"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
            strict_mode=True,
        )

    def test_no_banned_headers_lib8tion(self) -> None:
        """Searches through lib8tion/ directory with STRICT enforcement - no bypass allowed."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n\n"
                "STRICT MODE: lib8tion/ directory headers must not include stdlib headers.\n"
                "- .h/.hpp files: NEVER allow stdlib headers (header purity is critical)\n"
                "- .cpp files: STRICT mode - no '// ok include' bypass allowed\n"
                "Fix by removing the banned header or use fl/ alternatives.\n"
                "See HEADER_RECOMMENDATIONS for suggested replacements."
            )

        # Test lib8tion/ directory with strict mode
        test_directories = [
            os.path.join(SRC_ROOT, "lib8tion"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
            strict_mode=True,
        )

    def test_no_banned_headers_src(self) -> None:
        """Searches through fx/, sensors/, and platforms/shared/ directories for banned headers."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n\n"
                "Policy for fx/, sensors/, platforms/shared/ directories:\n"
                "- .h/.hpp files: NEVER allow stdlib headers (header purity is critical)\n"
                "- .cpp files: Use '// ok include' comment at the end of line to bypass\n"
                "Recommended: Use fl/ alternatives instead of stdlib headers.\n"
                "See HEADER_RECOMMENDATIONS for suggested replacements."
            )

        # Test other source directories (not fl/)
        test_directories = [
            os.path.join(SRC_ROOT, "fx"),
            os.path.join(SRC_ROOT, "sensors"),
            os.path.join(SRC_ROOT, "platforms", "shared"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
            strict_mode=False,
        )

    def test_no_banned_headers_examples(self) -> None:
        """Searches through the program files to check for banned headers."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )

        test_directories = ["examples"]

        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_COMMON,
            on_fail=on_fail,
        )

    def test_no_banned_headers_platforms(self) -> None:
        """Searches through the platforms directory to enforce no stdlib headers in .h files.

        Platform headers must use fl/ alternatives, just like core FastLED code.
        This ensures platform implementations are consistent with library standards.
        """

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n\n"
                "Policy for src/platforms/**/*.h files:\n"
                "- .h/.hpp files: NEVER allow stdlib headers (must use fl/ alternatives)\n"
                "- .cpp files: Use '// ok include' comment at the end of line to bypass\n"
                "Platform implementations must follow the same standards as core FastLED code.\n"
                "See HEADER_RECOMMENDATIONS for suggested replacements."
            )

        # Test the platforms directory with strict rules for .h files
        test_directories = [
            os.path.join(SRC_ROOT, "platforms"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_PLATFORMS,
            on_fail=on_fail,
            strict_mode=False,  # Allow bypass in .cpp files, but never in .h files
        )

    def test_no_banned_headers_third_party(self) -> None:
        """Searches through third_party directory to check for stdlib headers.

        Third-party libraries should use fl/ alternatives instead of standard library headers.
        STRICT MODE: No banned headers allowed in third_party/**/*.h files.
        """

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n\n"
                "STRICT MODE: Third-party libraries must use fl/ equivalents instead of stdlib headers.\n"
                "Policy for third_party/ directory:\n"
                "- .h/.hpp files: NEVER allow stdlib headers (header purity is critical)\n"
                "- .cpp files: STRICT mode - no '// ok include' bypass allowed\n"
                "Third-party code must follow FastLED conventions and use fl/ alternatives.\n"
                "See HEADER_RECOMMENDATIONS for suggested replacements."
            )

        # Test the third_party directory
        test_directories = [
            os.path.join(SRC_ROOT, "third_party"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_COMMON,
            on_fail=on_fail,
            strict_mode=True,  # STRICT mode - no "// ok include" bypass for third_party
        )


if __name__ == "__main__":
    unittest.main()
