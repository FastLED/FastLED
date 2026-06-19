# pyright: reportUnknownMemberType=false
"""Banned-header data tables for the Rust C++ linter.

The Python `BannedHeadersChecker` class has been retired — the equivalent
checker now lives in `ci/lint_cpp_rs/` (see `RUST_SUPPORTED_CHECKERS` in
`ci/lint_cpp/rust_bridge.py`). This module is kept for the
`BANNED_HEADERS_*` / `HEADER_EXCEPTIONS` constants that other Python
tooling (notably `run_all_checkers.py`) still imports.
"""

from dataclasses import dataclass


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
    "unordered_map",
    "set",
    "unordered_set",
    "multimap",
    "multiset",
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
    "stdlib.h",  # Ban C stdlib.h - use fl/stl/cstdlib.h instead
    "malloc.h",  # Ban malloc.h - use fl/stl/cstdlib.h (fl::aligned_alloc) instead
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
    "Arduino.h": "fl/system/arduino.h (trampoline that includes Arduino.h + cleans up macros)",
    "pthread.h": "fl/stl/thread.h or fl/stl/mutex.h (depending on what you need)",
    "assert.h": "FL_CHECK or FL_ASSERT macros (check fl/stl/compiler_control.h)",
    "iostream": "fl/stl/iostream.h or fl/str.h",
    "stdio.h": "fl/stl/stdio.h (provides fl::printf, fl::snprintf, fl::sprintf)",
    "cstdio": "fl/stl/cstdio.h (provides fl::print, fl::println)",
    "cstdlib": "fl/stl/cstdlib.h or fl/string operations",
    "vector": "fl/stl/vector.h",
    "list": "fl/stl/list.h or fl/stl/vector.h",
    "map": "fl/stl/map.h (check fl/stl/hash_map.h for hash-based)",
    "unordered_map": "fl/stl/unordered_map.h",
    "set": "fl/stl/set.h",
    "unordered_set": "fl/stl/unordered_set.h",
    "multimap": "fl/stl/multi_map.h",
    "multiset": "fl/stl/multi_set.h",
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
    "cfloat": "fl/stl/limits.h or platform-specific headers",
    "cassert": "FL_CHECK macros from fl/stl/compiler_control.h",
    "cerrno": "Error handling through return codes",
    "cctype": "Character classification (implement if needed)",
    "cwctype": "Wide character classification (implement if needed)",
    "cstring": "fl/str.h or fl/stl/cstring.h",
    "cwchar": "Wide character support (implement if needed)",
    "cuchar": "Character support (implement if needed)",
    "cstdint": "fl/stl/stdint.h",
    "stdint.h": "fl/stl/stdint.h",
    "stddef.h": "fl/stl/stddef.h or fl/stl/cstddef.h",
    "cstddef": "fl/stl/cstddef.h",
    "stdlib.h": "fl/stl/cstdlib.h (provides fl::aligned_alloc, fl::strtol, fl::atoi, etc.)",
    "malloc.h": "fl/stl/cstdlib.h (provides fl::aligned_alloc / fl::aligned_free)",
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
    "__random": '"fl/math/random.h"',
    "__thread": '"fl/stl/thread.h"',
    "__type_traits": '"fl/stl/type_traits.h"',
    "__utility": '"fl/stl/utility.h"',
    "__vector": '"fl/stl/vector.h"',
    "__hash_table": '"fl/stl/unordered_map.h"',
    "__tree": '"fl/stl/map.h"',
    "__node_handle": '"fl/stl/unordered_map.h"',
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
    # Sole gateway to Arduino.h - includes Arduino.h + cleans up macros atomically
    "Arduino.h": [
        HeaderException("fl/system/arduino.h", "Sole gateway trampoline for Arduino.h"),
        # Third-party exemptions (cannot modify):
        HeaderException(
            "third_party/ezws2812/ezWS2812.h", "Third-party SPI driver library"
        ),
        HeaderException(
            "third_party/object_fled/src/ObjectFLEDDmaManager.h",
            "Third-party DMA manager library",
        ),
        # WASM stub is its own Arduino.h implementation:
        HeaderException(
            "platforms/wasm/compiler/Arduino.h", "WASM stub Arduino.h implementation"
        ),
        HeaderException(
            "platforms/wasm/led_sysdefs_wasm.h", "WASM wrapper for stub Arduino.h"
        ),
    ],
    # Threading and synchronization
    "pthread.h": [
        HeaderException("fl/stl/thread_local.h", "Platform threading abstraction"),
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
        HeaderException("fl/async.cpp.hpp", "Stub-specific thread yielding"),
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
        HeaderException("fl/stl/stdint.h", "Limit macros: INT8_MAX, UINT64_MAX, etc."),
        HeaderException("fl/stl/time.cpp", "Platform-specific time implementation"),
        HeaderException(
            "fl/stl/time.cpp.hpp", "Platform-specific time implementation (header-only)"
        ),
    ],
    # Time and timing
    "chrono": [
        HeaderException("fl/stl/time.cpp", "Platform-specific time implementation"),
        HeaderException(
            "fl/stl/time.cpp.hpp", "Platform-specific time implementation (header-only)"
        ),
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
        ),
        HeaderException(
            "fl/stl/cstring.cpp.hpp", "C string wrapper (header-only implementation)"
        ),
        HeaderException(
            "fl/stl/detail/string_holder.cpp.hpp",
            "String holder implementation requiring strlen",
        ),
    ],
    "stdlib.h": [
        HeaderException(
            "fl/stl/str.cpp", "C string implementation (malloc, free, etc.)"
        ),
        HeaderException(
            "fl/stl/cstdlib.cpp.hpp",
            "C stdlib wrapper (aligned_alloc/aligned_free implementation)",
        ),
        HeaderException(
            "fl/stl/cstring.cpp.hpp",
            "C string wrapper (memcpy, strlen, etc. implementation)",
        ),
        HeaderException(
            "fl/stl/detail/string_holder.cpp.hpp",
            "String holder implementation requiring malloc/free",
        ),
        HeaderException(
            "fl/stl/malloc.cpp.hpp",
            "fl::malloc/fl::free/fl::realloc implementation",
        ),
        HeaderException(
            "fl/stl/undef.h",
            "Macro-reset header must include stdlib.h to undefine abs/min/max macros",
        ),
        HeaderException(
            "platforms/arm/teensy/coroutine_teensy.impl.hpp",
            "Teensy coroutine needs malloc/free for stack allocation",
        ),
        HeaderException(
            "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.cpp.hpp",
            "SPI mock needs aligned_alloc for DMA buffer simulation",
        ),
        HeaderException(
            "platforms/shared/ui/json/json_console.cpp.hpp",
            "JSON console uses atoi/strtol from stdlib",
        ),
        HeaderException(
            "third_party/libhelix_mp3/real/buffers.hpp",
            "Third-party MP3 decoder library (cannot modify)",
        ),
        HeaderException(
            "third_party/stb/truetype/stb_truetype.cpp.hpp",
            "Third-party STB TrueType library (cannot modify)",
        ),
    ],
    "malloc.h": [
        HeaderException(
            "fl/stl/cstdlib.cpp.hpp",
            "Windows _aligned_malloc / _aligned_free implementation",
        ),
        HeaderException(
            "fl/stl/alloca.h",
            "alloca() fallback on compilers without VLA (e.g. Clang/MSVC)",
        ),
        HeaderException(
            "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.cpp.hpp",
            "SPI mock needs _aligned_malloc for DMA buffer simulation on Windows",
        ),
        HeaderException(
            "third_party/stb/stb_vorbis.cpp.hpp",
            "Third-party STB Vorbis library (false positive in changelog comment)",
        ),
    ],
    # Math operations
    "math.h": [
        HeaderException(
            "fl/math/math.cpp", "Platform math functions (sin, cos, sqrt, etc.)"
        ),
        HeaderException(
            "fl/math/math.cpp.hpp",
            "STL math wrapper (header-only unity build implementation)",
        ),
        HeaderException(
            "fl/math/math.cpp.hpp", "Platform math wrapper (header-only implementation)"
        ),
        HeaderException(
            "fl/audio/audio_reactive.cpp", "Audio FFT and signal processing"
        ),
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
    # Error handling
    "cerrno": [
        HeaderException(
            "fl/stl/cerrno.h", "fl::errno and error constants implementation"
        ),
        HeaderException("fl/stl/fstream.h", "FILE* error handling via errno"),
    ],
    # File operations
    "fstream": [],  # No exceptions - use fl::ifstream/ofstream from fl/stl/fstream.h instead
    "cstdio": [
        HeaderException(
            "fl/stl/detail/file_io.h", "Platform-agnostic FILE* abstraction (fl::FILE)"
        ),
        HeaderException("platforms/stub/fs_stub.hpp", "Test filesystem implementation"),
        HeaderException("platforms/wasm/io_wasm.h", "WASM platform I/O implementation"),
        HeaderException(
            "platforms/wasm/compiler/wasm_pch.h", "WASM precompiled header"
        ),
    ],
    "cstdlib": [
        HeaderException(
            "fl/stl/cstdlib.cpp.hpp",
            "C stdlib wrapper (template implementation requires standard header)",
        ),
    ],
}

