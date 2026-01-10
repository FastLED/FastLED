# pyright: reportUnknownMemberType=false
from pathlib import Path

from ..base_checker import BaseChecker
from ..result import CheckResult


class BannedHeadersChecker(BaseChecker):
    """Checks for banned header includes."""

    # Common banned headers - use FastLED alternatives instead
    BANNED_HEADERS_COMMON: set[str] = {
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
        "math.h",
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
        "cstddef",
        "string.h",  # Ban C string.h - use fl/str.h instead
        "type_traits",
        "new",  # Ban <new> except for placement new in inplacenew.h
    }

    # Recommendations: map banned headers to their fl:: alternatives
    HEADER_RECOMMENDATIONS: dict[str, str] = {
        "pthread.h": "fl/thread.h or fl/mutex.h (depending on what you need)",
        "assert.h": "FL_CHECK or FL_ASSERT macros (check fl/compiler_control.h)",
        "iostream": "fl/str.h or fl/strstream.h",
        "stdio.h": "fl/str.h for string operations",
        "cstdio": "fl/str.h for string operations",
        "cstdlib": "fl/string operations or fl/vector.h",
        "vector": "fl/vector.h",
        "list": "fl/vector.h (or implement with custom data structure)",
        "map": "fl/map.h (check fl/unordered_map.h for hash-based)",
        "set": "fl/set.h",
        "queue": "fl/vector.h with manual queue semantics",
        "deque": "fl/vector.h",
        "algorithm": "fl/algorithm.h or fl/sort.h",
        "memory": "fl/shared_ptr.h or fl/unique_ptr.h",
        "thread": "fl/thread.h",
        "mutex": "fl/mutex.h",
        "chrono": "fl/time.h",
        "fstream": "fl/file.h or platform file operations",
        "sstream": "fl/strstream.h",
        "iomanip": "fl/str.h stream manipulators",
        "exception": "Use error codes or fl/exceptions.h if available",
        "stdexcept": "Use error codes instead",
        "typeinfo": "Use fl/type_traits.h or RTTI if unavoidable",
        "ctime": "fl/time.h",
        "cmath": "fl/math.h",
        "math.h": "fl/math.h",
        "complex": "Custom complex number class or fl/geometry.h",
        "valarray": "fl/vector.h",
        "cfloat": "fl/numeric_limits.h or platform-specific headers",
        "cassert": "FL_CHECK macros from fl/compiler_control.h",
        "cerrno": "Error handling through return codes",
        "cctype": "Character classification (implement if needed)",
        "cwctype": "Wide character classification (implement if needed)",
        "cstring": "fl/str.h",
        "cwchar": "Wide character support (implement if needed)",
        "cuchar": "Character support (implement if needed)",
        "cstdint": "fl/stdint.h",
        "stdint.h": "fl/stdint.h",
        "stddef.h": "fl/stddef.h",
        "cstddef": "fl/stddef.h",
        "string.h": "fl/str.h (or use extern declarations for memset/memcpy if only C functions needed)",
        "type_traits": "fl/type_traits.h",
        "new": "Use stack allocation or custom allocators (placement new allowed in inplacenew.h)",
    }

    # ESP-specific banned headers (only if paranoid mode enabled)
    BANNED_HEADERS_ESP: set[str] = (
        set()
    )  # Empty for now, could add "esp32-hal.h" if needed

    # Platform-specific: Arduino.h should only be in platforms/, not core
    BANNED_HEADERS_CORE: set[str] = BANNED_HEADERS_COMMON | {"Arduino.h"}

    # For platforms/ directory: ban all stdlib headers (same as CORE)
    # Platform headers must use fl/ alternatives like core FastLED code
    BANNED_HEADERS_PLATFORMS: set[str] = BANNED_HEADERS_COMMON | {"Arduino.h"}

    def name(self) -> str:
        return "banned-headers"

    def should_check(self, file_path: Path) -> bool:
        if file_path.suffix not in {".cpp", ".h", ".hpp", ".cc", ".ino"}:
            return False

        # Check all files including third_party (they should also use fl/ alternatives)
        return True

    def _get_banned_headers_for_path(self, file_path: Path) -> set[str]:
        """Return the appropriate banned headers list based on file path."""
        path_str = str(file_path).replace("\\", "/")
        path_parts = path_str.split("/")

        # Check if in tests/ directory
        if "tests" in path_parts:
            # Tests can use standard library headers for testing infrastructure
            return set()  # Don't ban anything in tests

        # Check if in examples/ directory
        if "examples" in path_parts:
            # Examples can use standard headers for demonstration purposes
            return set()  # Don't ban anything in examples

        # Check if in platforms/ directory
        # Platform headers must use fl/ alternatives just like core code
        if "platforms" in path_parts:
            return self.BANNED_HEADERS_PLATFORMS

        # lib8tion directories must also follow strict banned headers rule
        # (they should use fl/ alternatives instead of stdlib headers)
        if "lib8tion" in path_parts:
            return self.BANNED_HEADERS_CORE

        # third_party directories must also follow the banned headers rule
        # (they should use fl/ alternatives instead of stdlib headers)
        # Default: core source (includes Arduino.h ban)
        return self.BANNED_HEADERS_CORE

    def _is_in_fl_directory(self, file_path: Path) -> bool:
        """Check if file is in the fl/ directory."""
        path_str = str(file_path).replace("\\", "/")
        return "/fl/" in path_str or path_str.endswith("/fl") or "/src/fl/" in path_str

    def _is_allowed_exception(
        self, file_path: Path, banned_header: str, line: str
    ) -> bool:
        """Check if this is an allowed exception to the banned header rule."""
        file_path_str = str(file_path)
        file_path_normalized = file_path_str.replace("\\", "/")

        # Allow <new> in inplacenew.h for placement new operator
        if banned_header == "new" and "inplacenew.h" in file_path_str:
            return True
        # Allow <new> in platforms/*/new.h for platform-specific placement new handling
        if (
            banned_header == "new"
            and "/platforms/" in file_path_normalized
            and file_path_str.endswith("new.h")
        ):
            return True

        # Allow Arduino.h in led_sysdefs.h - core system definitions need platform headers
        if banned_header == "Arduino.h" and "led_sysdefs.h" in file_path_str:
            return True

        # For fl/ directory, allow specific platform headers that have no alternatives
        # These are genuinely needed for platform-specific implementations
        if "/fl/" in file_path_normalized or "/src/fl/" in file_path_normalized:
            # Allow iostream in stub_main.cpp for testing entry point
            if banned_header == "iostream" and "stub_main.cpp" in file_path_str:
                return True

            # Allow pthread.h in thread_local.h - needed for platform threading abstraction
            if banned_header == "pthread.h" and "thread_local.h" in file_path_str:
                return True

            # Allow mutex in mutex.h - this is the wrapper for platform mutex
            if banned_header == "mutex" and "mutex.h" in file_path_str:
                return True

            # Allow math.h in math implementation files (cpp files)
            if banned_header == "math.h" and file_path_str.endswith(".cpp"):
                if any(
                    x in file_path_str
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
            if "time.cpp" in file_path_str:
                if banned_header in {"stdint.h", "Arduino.h", "chrono"}:
                    return True

            # Allow Arduino.h in delay.cpp for platform-specific delay implementations
            if "delay.cpp" in file_path_str:
                if banned_header == "Arduino.h":
                    return True

            # Allow C library headers in str.cpp for string implementation
            if "str.cpp" in file_path_str:
                if banned_header in {"string.h", "stdlib.h"}:
                    return True

            # Allow C library headers in cstring.cpp for C string wrapping
            if "cstring.cpp" in file_path_str:
                if banned_header in {"string.h", "stdlib.h"}:
                    return True

        # Platform-specific headers need Arduino.h
        if "/platforms/" in file_path_normalized:
            if banned_header == "Arduino.h":
                return True

            # Stub platform implementations need stdlib headers for testing
            if "/stub/" in file_path_normalized:
                # fs_stub.hpp needs algorithm, fstream, cstdio for test filesystem
                if "fs_stub" in file_path_str and banned_header in {
                    "algorithm",
                    "fstream",
                    "cstdio",
                }:
                    return True
                # isr_stub.cpp and time_stub.cpp need chrono, thread, iostream for testing
                if (
                    "isr_stub" in file_path_str or "time_stub" in file_path_str
                ) and banned_header in {
                    "chrono",
                    "thread",
                    "iostream",
                }:
                    return True

            # WASM platform implementations need stdlib headers for I/O and threading
            if "/wasm/" in file_path_normalized:
                # WASM platform headers need vector, cstdio, stdio.h for platform-specific implementation
                if file_path_str.endswith(".h") and banned_header in {
                    "vector",
                    "cstdio",
                    "stdio.h",
                }:
                    return True
                # WASM timer.cpp needs thread and cmath for timer implementation
                if "timer.cpp" in file_path_str and banned_header in {
                    "thread",
                    "cmath",
                }:
                    return True

        # Third-party libraries may need Arduino.h in some cases
        if "/third_party/" in file_path_normalized:
            # ArduinoJSON and other third-party libraries may reference Arduino.h
            if banned_header == "Arduino.h":
                return True

            # Espressif LED strip library code needs C library headers
            if "/led_strip/" in file_path_normalized:
                if banned_header in {"string.h", "stdlib.h"}:
                    return True

        return False

    def _get_recommendation(self, banned_header: str) -> str:
        """Get recommendation for a banned header."""
        return self.HEADER_RECOMMENDATIONS.get(
            banned_header, "Use fl/ alternatives instead of standard library headers"
        )

    def _is_strict_mode_directory(self, file_path: Path) -> bool:
        """Check if file is in a strict mode directory (no bypass allowed).

        Strict mode directories include:
        - fl/ (FastLED library core)
        - lib8tion/ (FastLED 8-bit math library)
        - third_party/ (third-party libraries should follow FastLED conventions)
        """
        path_str = str(file_path).replace("\\", "/")
        return any(
            part in path_str
            for part in [
                "/fl/",
                "/lib8tion/",
                "/third_party/",
                "/src/fl/",
                "/src/lib8tion/",
                "/src/third_party/",
            ]
        )

    def _should_allow_bypass(self, file_path: Path) -> bool:
        """Check if file type allows 'ok include' bypass.

        - .h files: NEVER allow bypass (header purity is critical)
        - .cpp/.cc files: Allow bypass with '// ok include' comment (unless in strict mode)
        - .hpp files: Treat like .h files (never allow bypass)
        - Strict mode directories: NEVER allow bypass
        """
        file_path_str = str(file_path)
        if self._is_strict_mode_directory(file_path):
            return False  # Strict mode directories never allow bypass
        return file_path_str.endswith(".cpp") or file_path_str.endswith(".cc")

    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        results: list[CheckResult] = []
        lines = content.split("\n")

        # Get appropriate banned list for this file's location
        banned_headers = self._get_banned_headers_for_path(file_path)

        # Determine if this file can use "// ok include" bypass
        # Only .cpp/.cc files can bypass, never .h/.hpp files (and never in strict mode)
        can_bypass = self._should_allow_bypass(file_path)
        is_strict = self._is_strict_mode_directory(file_path)
        file_extension = file_path.suffix

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Check for #include
            if not stripped.startswith("#include"):
                continue

            # Check for banned headers
            for banned in banned_headers:
                # Check both <header> and "header" formats
                if (
                    f"<{banned}>" in stripped
                    or f'"{banned}"' in stripped
                    or f"<{banned}" in stripped  # Handles cases like <iostream
                ):
                    # Check if this is an allowed exception (e.g., <new> in inplacenew.h)
                    if self._is_allowed_exception(file_path, banned, line):
                        continue

                    # Check if bypass is allowed and present
                    has_bypass_comment = (
                        "// ok include" in line or "// OK include" in line
                    )
                    if can_bypass and has_bypass_comment:
                        continue

                    # Generate recommendation
                    recommendation = self._get_recommendation(banned)

                    # Build suggestion based on file type and bypass capability
                    if file_extension in {".h", ".hpp"}:
                        # Header files: NEVER allow bypass
                        suggestion = (
                            f"Use {recommendation} instead (banned in header files)"
                        )
                    elif is_strict:
                        # Strict mode directories: no bypass allowed
                        suggestion = f"Use {recommendation} instead (strict mode, no bypass allowed)"
                    elif can_bypass:
                        # .cpp/.cc files: can bypass but should try to fix
                        suggestion = f"Use {recommendation} instead (or add '// ok include' comment if necessary)"
                    else:
                        suggestion = f"Use {recommendation} instead"

                    results.append(
                        CheckResult(
                            checker_name=self.name(),
                            file_path=str(file_path),
                            line_number=line_num,
                            severity="ERROR",
                            message=f"Banned header '{banned}' in {file_extension} file",
                            suggestion=suggestion,
                        )
                    )

        return results
