use glob::glob;
use rayon::prelude::*;
use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use walkdir::WalkDir;

type DynError = Box<dyn Error + Send + Sync>;

const CPP_EXTENSIONS: &[&str] = &["cpp", "h", "hpp", "ino", "cpp.hpp"];

const EXCLUDED_FILES: &[&str] = &[
    "stub_main.cpp",
    "doctest_main.cpp",
    "fltest.h",
    "doctest.h",
    "crash_handler_execinfo.h",
    "crash_handler_libunwind.h",
    "crash_handler_noop.h",
    "crash_handler_win.h",
    "run_example.h",
    "run_unit_test.h",
    "platforms/apple/run_example.hpp",
    "platforms/apple/run_unit_test.hpp",
    "platforms/posix/run_example.hpp",
    "platforms/posix/run_unit_test.hpp",
    "platforms/win/run_example.hpp",
    "platforms/win/run_unit_test.hpp",
];

const BARE_ALLOCATION_WHITELISTED_SUFFIXES: &[&str] = &[
    "fl/stl/allocator.h",
    "fl/stl/allocator.cpp.hpp",
    "fl/stl/malloc.h",
    "fl/stl/malloc.cpp.hpp",
    "fl/stl/new.h",
    "fl/stl/unique_ptr.h",
    "fl/stl/shared_ptr.h",
    "fl/stl/shared_ptr.cpp.hpp",
    "fl/stl/scoped_ptr.h",
    "fl/stl/weak_ptr.h",
    "fl/stl/detail/string_holder.cpp.hpp",
    "fl/system/heap.h",
    "fl/system/heap.cpp.hpp",
];

const SLEEP_FOR_WHITELISTED_SUFFIXES: &[&str] = &[
    "fl/stl/thread.h",
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    "platforms/stub/platform_time.cpp.hpp",
    "fl/async.cpp.hpp",
    "platforms/shared/spi_bitbang/host_timer.cpp.hpp",
    "platforms/stub/coroutine_stub.impl.hpp",
];

const VALID_INCLUDE_PREFIXES: &[&str] = &["fl/", "platforms/", "fx/", "sensors/", "third_party/"];

const EXTERNAL_SDK_PREFIXES: &[&str] = &[
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
    "hardware/",
    "pico/",
    "boards/",
    "cmsis/",
    "class/",
    "tusb_",
    "Arduino.h",
    "Wire.h",
    "SPI.h",
    "pins_arduino.h",
    "util/",
    "core_",
    "cmsis_",
    "nrf_",
    "nrf52",
    "nrfx",
    "stm32",
    "core_pins.h",
    "usb_",
    "kinetis.h",
    "imxrt.h",
    "RP2040.h",
    "RP2350.h",
    "bsp_",
    "r_",
    "fsp/",
];

const AMBIGUOUS_INCLUDE_PREFIXES: &[&str] = &["avr/", "arm/"];

const FASTLED_PLATFORM_SUBDIRS: &[&str] = &[
    "adafruit/",
    "apollo3/",
    "arm/",
    "avr/",
    "esp/",
    "shared/",
    "stub/",
    "wasm/",
    "posix/",
];

const BANNED_INTERNAL_SUBPATHS: &[(&str, &str)] = &[("fl/math/lib8tion/", "fl/math/")];

const TYPO_INCLUDE_PREFIXES: &[(&str, &str)] = &[
    ("fL/", "fl/"),
    ("FL/", "fl/"),
    ("Fl/", "fl/"),
    ("platform/", "platforms/"),
    ("Platform/", "platforms/"),
    ("PLATFORMS/", "platforms/"),
];

const ARDUINO_BANNED_MACROS: &[&str] = &["INPUT", "OUTPUT", "DEFAULT"];

const ATTRIBUTE_MAPPINGS: &[(&str, &str)] = &[
    ("maybe_unused", "FL_MAYBE_UNUSED"),
    ("nodiscard", "FL_NODISCARD"),
    ("fallthrough", "FL_FALLTHROUGH"),
    ("deprecated", "FL_DEPRECATED"),
    ("noreturn", "FL_NORETURN"),
    ("likely", "FL_LIKELY"),
    ("unlikely", "FL_UNLIKELY"),
    ("no_unique_address", "FL_NO_UNIQUE_ADDRESS"),
];

const NUMERIC_LIMIT_MACROS: &[&str] = &[
    "UINT8_MAX",
    "UINT16_MAX",
    "UINT32_MAX",
    "UINT64_MAX",
    "UINTMAX_MAX",
    "UINTPTR_MAX",
    "SIZE_MAX",
    "INT8_MAX",
    "INT16_MAX",
    "INT32_MAX",
    "INT64_MAX",
    "INTMAX_MAX",
    "INTPTR_MAX",
    "INT8_MIN",
    "INT16_MIN",
    "INT32_MIN",
    "INT64_MIN",
    "INTMAX_MIN",
    "INTPTR_MIN",
    "UCHAR_MAX",
    "USHRT_MAX",
    "UINT_MAX",
    "ULONG_MAX",
    "ULLONG_MAX",
    "CHAR_MAX",
    "SHRT_MAX",
    "INT_MAX",
    "LONG_MAX",
    "LLONG_MAX",
    "CHAR_MIN",
    "SHRT_MIN",
    "INT_MIN",
    "LONG_MIN",
    "LLONG_MIN",
];

const NUMERIC_LIMIT_EXCLUDED_SUFFIXES: &[&str] =
    &["tests/fl/stl/cstdint.cpp", "tests/fl/stl/stdint.cpp"];

const STD_BRIDGE_FILE_WHITELIST: &[&str] = &[
    "platforms/stub/mutex_stub_stl.h",
    "platforms/stub/mutex_stub_noop.h",
    "platforms/esp/32/mutex_esp32.h",
    "platforms/arm/rp/mutex_rp.h",
    "platforms/arm/stm32/mutex_stm32.h",
    "platforms/arm/stm32/mutex_stm32_rtos.h",
    "platforms/arm/d21/mutex_samd.h",
    "platforms/arm/nrf52/mutex_nrf52.h",
    "platforms/stub/condition_variable_stub.h",
    "platforms/esp/32/condition_variable_esp32.h",
    "platforms/esp/32/condition_variable_esp32.cpp.hpp",
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    "platforms/stub/semaphore_stub_stl.h",
    "platforms/stub/semaphore_stub_noop.h",
    "platforms/esp/32/semaphore_esp32.h",
    "platforms/arm/d21/semaphore_samd.h",
    "platforms/arm/d21/semaphore_samd.cpp.hpp",
    "platforms/arm/rp/semaphore_rp.h",
    "platforms/arm/rp/semaphore_rp.cpp.hpp",
    "platforms/arm/stm32/semaphore_stm32.h",
    "platforms/arm/stm32/semaphore_stm32.cpp.hpp",
    "platforms/stub/platform_time.cpp.hpp",
    "platforms/apple/run_example.hpp",
    "platforms/apple/run_unit_test.hpp",
    "platforms/posix/run_example.hpp",
    "platforms/posix/run_unit_test.hpp",
    "platforms/win/run_example.hpp",
    "platforms/win/run_unit_test.hpp",
];

const ALLOWED_STD_SYMBOLS: &[&str] = &[
    "std::atomic_thread_fence",
    "std::memory_order_acquire",
    "std::memory_order_release",
    "std::memory_order_seq_cst",
    "std::memory_order_relaxed",
    "std::memory_order_acq_rel",
    "std::memory_order_consume",
];

const FORBIDDEN_SERIAL_METHODS: &[&str] = &[
    "print",
    "println",
    "printf",
    "write",
    "read",
    "available",
    "peek",
    "readStringUntil",
    "flush",
    "begin",
];

const ALLOWED_SERIAL_METHODS: &[&str] = &["setTxBufferSize", "setTxTimeoutMs"];

const INCLUDE_AFTER_NAMESPACE_SKIP_PATTERNS: &[&str] = &[
    ".venv",
    "node_modules",
    "build",
    ".build",
    "third_party",
    "ziglang",
    "greenlet",
    ".git",
];

const PLATFORM_INCLUDE_LOCATIONS: &[&str] = &[
    "/src/fl/",
    "/src/fx/",
    "/src/lib8tion/",
    "/src/sensors/",
    "/tests/fl/",
    "/tests/fx/",
    "/examples/",
];

const DEEP_PLATFORM_REPLACEMENTS: &[(&str, &str)] = &[
    ("platforms/shared/int_windows.h", "platforms/int.h"),
    ("platforms/shared/int.h", "platforms/int.h"),
    ("platforms/esp/", "platforms/init.h"),
    ("platforms/arm/", "platforms/init.h"),
    ("platforms/stub/", "platforms/init.h"),
    ("platforms/avr/", "platforms/init.h"),
    ("platforms/wasm/", "platforms/init.h"),
];

const SIMD_PATTERNS: &[(&str, &str)] = &[
    (
        "__m128i",
        "x86 SSE type — use fl::simd::simd_u8x16 / simd_u16x8 / simd_u32x4",
    ),
    ("__m128", "x86 SSE type — use fl::simd types"),
    ("__m256i", "x86 AVX type — use fl::simd types"),
    ("__m256", "x86 AVX type — use fl::simd types"),
    ("_mm_", "x86 SSE intrinsic — use fl::simd operations"),
    ("_mm256_", "x86 AVX2 intrinsic — use fl::simd operations"),
    ("_mm512_", "x86 AVX-512 intrinsic — use fl::simd operations"),
    ("_MM_HINT", "x86 prefetch hint — use __builtin_prefetch"),
    ("<emmintrin.h>", "SSE2 header — use fl/math/simd.h"),
    ("<immintrin.h>", "AVX header — use fl/math/simd.h"),
    ("<smmintrin.h>", "SSE4.1 header — use fl/math/simd.h"),
    ("<xmmintrin.h>", "SSE header — use fl/math/simd.h"),
    ("<tmmintrin.h>", "SSSE3 header — use fl/math/simd.h"),
    ("<arm_neon.h>", "ARM NEON header — use fl/math/simd.h"),
    ("vld1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vst1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vaddq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmulq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmovl_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vqmovn_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vcombine_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vdupq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("ee.vadds", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vld", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vst", "Xtensa PIE intrinsic — use fl::simd operations"),
];

const CTYPE_FUNCTIONS: &[&str] = &[
    "isspace", "isdigit", "isalpha", "isalnum", "isupper", "islower", "tolower", "toupper",
];

const CSTRING_FUNCTIONS: &[&str] = &[
    "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strcat", "strncat", "strstr", "strchr",
    "strrchr", "strspn", "strcspn", "strpbrk", "strtok", "strerror", "memcpy", "memcmp", "memmove",
    "memset", "memchr",
];

const CTYPE_WHITELISTED_SUFFIXES: &[&str] = &[
    "fl/stl/cctype.h",
    "fl/stl/cstring.h",
    "fl/stl/cstring.cpp.hpp",
];

const STDINT_TYPE_MAPPINGS: &[(&str, &str)] = &[
    ("uint8_t", "u8"),
    ("uint16_t", "u16"),
    ("uint32_t", "u32"),
    ("uint64_t", "u64"),
    ("int8_t", "i8"),
    ("int16_t", "i16"),
    ("int32_t", "i32"),
    ("int64_t", "i64"),
    ("unsigned int", "u32"),
    ("signed int", "i32"),
];

const STDINT_EXCLUDED_FILENAMES: &[&str] = &[
    "stdint.h",
    "int.h",
    "run_unit_test.hpp",
    "dual_isr_context.h",
    "mcpwm_timer.h",
];

const UNIT_TEST_EXEMPT_FILES: &[&str] = &["test.h", "doctest.h", "doctest_main.cpp"];

const UNIT_TEST_BANNED_MACROS: &[(&str, &str)] = &[
    ("TEST_CASE_TEMPLATE", "FL_TEST_CASE_TEMPLATE"),
    ("TEST_CASE_FIXTURE", "FL_TEST_CASE_FIXTURE"),
    (
        "REQUIRE_THROWS_WITH_MESSAGE",
        "FL_REQUIRE_THROWS_WITH_MESSAGE",
    ),
    ("CHECK_FALSE_MESSAGE", "FL_CHECK_FALSE_MESSAGE"),
    ("REQUIRE_FALSE_MESSAGE", "FL_REQUIRE_FALSE_MESSAGE"),
    ("WARN_FALSE_MESSAGE", "FL_WARN_FALSE_MESSAGE"),
    ("REQUIRE_THROWS_WITH", "FL_REQUIRE_THROWS_WITH"),
    ("CHECK_FALSE", "FL_CHECK_FALSE"),
    ("CHECK_MESSAGE", "FL_CHECK_MESSAGE"),
    ("CHECK_THROWS_AS", "FL_CHECK_THROWS_AS"),
    ("CHECK_THROWS_WITH", "FL_CHECK_THROWS_WITH"),
    ("REQUIRE_FALSE", "FL_REQUIRE_FALSE"),
    ("REQUIRE_MESSAGE", "FL_REQUIRE_MESSAGE"),
    ("REQUIRE_THROWS_AS", "FL_REQUIRE_THROWS_AS"),
    ("WARN_FALSE", "FL_DWARN_FALSE"),
    ("WARN_MESSAGE", "FL_WARN_MESSAGE"),
    ("WARN_THROWS_AS", "FL_WARN_THROWS_AS"),
    ("WARN_THROWS_WITH", "FL_WARN_THROWS_WITH"),
    ("TYPE_TO_STRING", "FL_TYPE_TO_STRING"),
    ("CHECK_UNARY_FALSE", "FL_CHECK_UNARY_FALSE"),
    ("REQUIRE_UNARY_FALSE", "FL_REQUIRE_UNARY_FALSE"),
    ("WARN_UNARY_FALSE", "FL_WARN_UNARY_FALSE"),
    ("TEST_CASE", "FL_TEST_CASE"),
    ("TEST_SUITE", "FL_TEST_SUITE"),
    ("CHECK_CLOSE", "FL_CHECK_CLOSE"),
    ("CHECK_DOUBLE_EQ", "FL_CHECK_DOUBLE_EQ"),
    ("CHECK_APPROX", "FL_CHECK_APPROX"),
    ("CHECK_STREQ", "FL_CHECK_STREQ"),
    ("CHECK_THROWS", "FL_CHECK_THROWS"),
    ("CHECK_NOTHROW", "FL_CHECK_NOTHROW"),
    ("CHECK_TRAIT", "FL_CHECK_TRAIT"),
    ("REQUIRE_CLOSE", "FL_REQUIRE_CLOSE"),
    ("REQUIRE_APPROX", "FL_REQUIRE_APPROX"),
    ("REQUIRE_THROWS", "FL_REQUIRE_THROWS"),
    ("REQUIRE_NOTHROW", "FL_REQUIRE_NOTHROW"),
    ("WARN_UNARY", "FL_WARN_UNARY"),
    ("WARN_THROWS", "FL_WARN_THROWS"),
    ("WARN_NOTHROW", "FL_WARN_NOTHROW"),
    ("AND_WHEN", "FL_AND_WHEN"),
    ("AND_THEN", "FL_AND_THEN"),
    ("SUBCASE", "FL_SUBCASE"),
    ("MESSAGE", "FL_MESSAGE"),
    ("CAPTURE", "FL_CAPTURE"),
    ("FAIL_CHECK", "FL_FAIL_CHECK"),
    ("CHECK_UNARY", "FL_CHECK_UNARY"),
    ("CHECK_EQ", "FL_CHECK_EQ"),
    ("CHECK_NE", "FL_CHECK_NE"),
    ("CHECK_GT", "FL_CHECK_GT"),
    ("CHECK_GE", "FL_CHECK_GE"),
    ("CHECK_LT", "FL_CHECK_LT"),
    ("CHECK_LE", "FL_CHECK_LE"),
    ("REQUIRE_UNARY", "FL_REQUIRE_UNARY"),
    ("REQUIRE_EQ", "FL_REQUIRE_EQ"),
    ("REQUIRE_NE", "FL_REQUIRE_NE"),
    ("REQUIRE_GT", "FL_REQUIRE_GT"),
    ("REQUIRE_GE", "FL_REQUIRE_GE"),
    ("REQUIRE_LT", "FL_REQUIRE_LT"),
    ("REQUIRE_LE", "FL_REQUIRE_LE"),
    ("WARN_FALSE", "FL_DWARN_FALSE"),
    ("WARN_EQ", "FL_WARN_EQ"),
    ("WARN_NE", "FL_WARN_NE"),
    ("WARN_GT", "FL_WARN_GT"),
    ("WARN_GE", "FL_WARN_GE"),
    ("WARN_LT", "FL_WARN_LT"),
    ("WARN_LE", "FL_WARN_LE"),
    ("SCENARIO", "FL_SCENARIO"),
    ("REQUIRE", "FL_REQUIRE"),
    ("CHECK", "FL_CHECK"),
    ("GIVEN", "FL_GIVEN"),
    ("WHEN", "FL_WHEN"),
    ("THEN", "FL_THEN"),
    ("INFO", "FL_DINFO"),
    ("FAIL", "FL_FAIL"),
    ("WARN", "FL_DWARN"),
];

const UNIT_TEST_FAST_PREFIXES: &[&str] = &[
    "CHECK",
    "REQUIRE",
    "WARN",
    "TEST_",
    "SUBCASE",
    "FAIL",
    "SCENARIO",
    "GIVEN",
    "WHEN",
    "THEN",
    "AND_",
    "MESSAGE",
    "INFO",
    "CAPTURE",
    "TYPE_TO_STRING",
];

const FL_IS_PREFIX_TO_HEADER: &[(&str, &str)] = &[
    ("FL_IS_RENESAS", "is_renesas.h"),
    ("FL_IS_APOLLO3", "is_apollo3.h"),
    ("FL_IS_STM32", "is_stm32.h"),
    ("FL_IS_TEENSY", "is_teensy.h"),
    ("FL_IS_NRF52", "is_nrf52.h"),
    ("FL_IS_SILABS", "is_silabs.h"),
    ("FL_IS_POSIX", "is_posix.h"),
    ("FL_IS_WASM", "is_wasm.h"),
    ("FL_IS_STUB", "is_stub.h"),
    ("FL_IS_AVR", "is_avr.h"),
    ("FL_IS_ESP", "is_esp.h"),
    ("FL_IS_ARM", "is_arm.h"),
    ("FL_IS_SAMD", "is_samd.h"),
    ("FL_IS_SAM", "is_sam.h"),
    ("FL_IS_WIN", "is_win.h"),
    ("FL_IS_RP", "is_rp.h"),
];

const IWYU_INTERNAL_HEADER_PREFIXES: &[&str] = &["fl/", "fx/", "sensors/", "lib8tion/"];

const TEST_PATH_EXCLUDED_FILES: &[&str] = &[
    "doctest_main.cpp",
    "audio.cpp",
    "codec.cpp",
    "log.cpp",
    "detectors.cpp",
    "encoders.cpp",
    "2d.cpp",
    "validation.cpp",
    "draw_ring.hpp",
    "draw_thick_line.hpp",
    "draw_line.hpp",
    "draw_disc.hpp",
    "draw_disc_16.hpp",
    "perf_primitives.hpp",
    "gain.hpp",
    "test_helpers.hpp",
    "vocal_real_audio.hpp",
    "map_range.hpp",
    "assume_aligned.hpp",
    "insert_result.hpp",
    "active_strip_data_json.cpp",
    "audio_url.cpp",
    "bytestream.cpp",
    "clamp.cpp",
    "force_inline.cpp",
    "hsv2rgb_accuracy.cpp",
    "noise_range.cpp",
    "noise_ring.cpp",
    "power_estimation.cpp",
    "slice.cpp",
    "unused.cpp",
    "channel_manager.cpp",
    "spi_channel.cpp",
    "wave8_spi.cpp",
    "loopback.cpp",
    "rpc.cpp",
    "rpc_http_stream.cpp",
    "adversarial.cpp",
    "deficiencies.cpp",
    "sound_level_meter.cpp",
    "allocator_move.cpp",
    "cstdint.cpp",
    "function_list.cpp",
    "strstream_integers.cpp",
    "test_tcp_socket.cpp",
    "test_tcp_acceptor.cpp",
    "http_promise.cpp",
    "http_transport.cpp",
    "server_loopback.cpp",
];

const HEADERS_EXIST_EXCLUDED_FILES: &[&str] = &[
    "doctest_main.cpp",
    "sketch_runner.cpp",
    "spi_batching_logic.cpp",
    "serial_printf.cpp",
    "test_runner.cpp",
    "runner.cpp",
    "crash_handler_main.cpp",
    "example_runner.cpp",
    "fltest_self_test.cpp",
    "asan_leak.cpp",
    "test_helpers.hpp",
    "codec.cpp",
    "log.cpp",
    "detectors.cpp",
    "encoders.cpp",
    "2d.cpp",
    "validation.cpp",
    "rpc.cpp",
];

const TEST_CONFIG_EXCLUDED_DIRS: &[&str] = &[
    "tests/shared",
    "tests/testing",
    "tests/data",
    "tests/manual",
    "tests/profile",
    "tests/build",
    "tests/builddir",
    "tests/x64",
    "tests/bin",
    "tests/example_compile_direct",
    "tests/fastled_js",
    "tests/fl/chipsets/encoders",
    "tests/fl/log",
    "tests/fl/channels/detail/validation",
    "tests/fl/remote/rpc",
    "tests/fl/codec",
    "tests/fl/fx/2d",
    "tests/fbuild_qemu_smoke",
];

const TEST_AGGREGATED_DIRS: &[&str] = &[
    "tests/fl/chipsets/encoders",
    "tests/fl/log",
    "tests/fl/channels/detail/validation",
    "tests/fl/remote/rpc",
    "tests/fl/codec",
    "tests/fl/fx/2d",
];

const TEST_INCLUDE_VALID_PREFIXES: &[&str] = &[
    "fl/",
    "platforms/",
    "fx/",
    "sensors/",
    "third_party/",
    "tests/",
];

const NOEXCEPT_CTOR_QUALS: &[&str] = &[
    "",
    "explicit",
    "virtual",
    "inline",
    "constexpr",
    "explicit constexpr",
    "constexpr explicit",
    "inline explicit",
    "explicit inline",
    "inline constexpr",
    "constexpr inline",
    "virtual explicit",
    "explicit virtual",
    "inline virtual",
    "virtual inline",
];

const NATIVE_TO_MODERN_DEFINES: &[(&str, &str)] = &[
    ("__AVR__", "FL_IS_AVR"),
    ("__AVR_ATmega328P__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega328PB__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega328__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega168P__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega168__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega8__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega8A__", "FL_IS_AVR_ATMEGA_328P"),
    ("__AVR_ATmega2560__", "FL_IS_AVR_ATMEGA_2560"),
    ("__AVR_ATmega1280__", "FL_IS_AVR_ATMEGA_2560"),
    ("__AVR_ATmega32U4__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega1284__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega1284P__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega644P__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega644__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega32__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega16__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega128__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega64__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega32U2__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega16U2__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega8U2__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_AT90USB1286__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_AT90USB646__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_AT90USB162__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_AT90USB82__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega128RFA1__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega256RFR2__", "FL_IS_AVR_ATMEGA"),
    ("__AVR_ATmega4809__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega4808__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega3209__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega3208__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega1609__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega1608__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega809__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATmega808__", "FL_IS_AVR_MEGAAVR"),
    ("__AVR_ATtiny13__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny13A__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny24__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny44__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny84__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny25__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny45__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny85__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny48__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny87__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny88__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny167__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny261__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny441__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny841__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny861__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny2313__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny2313A__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny4313__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtinyX41__", "FL_IS_AVR_ATTINY"),
    ("__AVR_ATtiny202__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny204__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny212__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny214__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny402__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny404__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny406__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny407__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny412__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny414__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny416__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny417__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny1604__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny1616__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny3216__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtiny3217__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtinyxy4__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtinyxy6__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtinyxy7__", "FL_IS_AVR_ATTINY_MODERN"),
    ("__AVR_ATtinyxy2__", "FL_IS_AVR_ATTINY_MODERN"),
    ("ARDUINO_AVR_DIGISPARK", "FL_IS_AVR_ATTINY"),
    ("ARDUINO_AVR_DIGISPARKPRO", "FL_IS_AVR_ATTINY"),
    ("IS_BEAN", "FL_IS_AVR_ATTINY"),
    ("ESP32", "FL_IS_ESP32"),
    ("ESP8266", "FL_IS_ESP8266"),
    ("ESP_PLATFORM", "FL_IS_ESP32"),
    ("ARDUINO_ARCH_ESP32", "FL_IS_ESP32"),
    ("ARDUINO_ARCH_ESP8266", "FL_IS_ESP8266"),
    ("CONFIG_IDF_TARGET_ESP32", "FL_IS_ESP_32DEV"),
    ("CONFIG_IDF_TARGET_ESP32S2", "FL_IS_ESP_32S2"),
    ("CONFIG_IDF_TARGET_ESP32S3", "FL_IS_ESP_32S3"),
    ("CONFIG_IDF_TARGET_ESP32C2", "FL_IS_ESP_32C2"),
    ("CONFIG_IDF_TARGET_ESP32C3", "FL_IS_ESP_32C3"),
    ("CONFIG_IDF_TARGET_ESP32C5", "FL_IS_ESP_32C5"),
    ("CONFIG_IDF_TARGET_ESP32C6", "FL_IS_ESP_32C6"),
    ("CONFIG_IDF_TARGET_ESP32H2", "FL_IS_ESP_32H2"),
    ("CONFIG_IDF_TARGET_ESP32P4", "FL_IS_ESP_32P4"),
    ("CONFIG_IDF_TARGET_ESP8266", "FL_IS_ESP8266"),
    ("CONFIG_IDF_TARGET_ARCH_XTENSA", "FL_IS_ESP32_XTENSA"),
    ("CONFIG_IDF_TARGET_ARCH_RISCV", "FL_IS_ESP32_RISCV"),
    ("ESP32S3", "FL_IS_ESP_32S3"),
    ("ESP32C3", "FL_IS_ESP_32C3"),
    ("ESP32C6", "FL_IS_ESP_32C6"),
    ("__MK20DX128__", "FL_IS_TEENSY_30"),
    ("__MK20DX256__", "FL_IS_TEENSY_3X"),
    ("__MKL26Z64__", "FL_IS_TEENSY_LC"),
    ("__MK64FX512__", "FL_IS_TEENSY_35"),
    ("__MK66FX1M0__", "FL_IS_TEENSY_36"),
    ("__IMXRT1062__", "FL_IS_TEENSY_4X"),
    ("__IMXRT1052__", "FL_IS_TEENSY_4X"),
    ("__arm__", "FL_IS_ARM"),
    ("__SAM3X8E__", "FL_IS_SAM"),
    ("STM32F10X_MD", "FL_IS_STM32_F1"),
    ("__STM32F1__", "FL_IS_STM32_F1"),
    ("STM32F1", "FL_IS_STM32_F1"),
    ("STM32F1xx", "FL_IS_STM32_F1"),
    ("STM32F2XX", "FL_IS_STM32_F2"),
    ("STM32F2xx", "FL_IS_STM32_F2"),
    ("STM32F4", "FL_IS_STM32_F4"),
    ("STM32F4xx", "FL_IS_STM32_F4"),
    ("STM32F7", "FL_IS_STM32_F7"),
    ("STM32F7xx", "FL_IS_STM32_F7"),
    ("STM32L4", "FL_IS_STM32_L4"),
    ("STM32L4xx", "FL_IS_STM32_L4"),
    ("STM32H7", "FL_IS_STM32_H7"),
    ("STM32H7xx", "FL_IS_STM32_H7"),
    ("STM32G4", "FL_IS_STM32_G4"),
    ("STM32G4xx", "FL_IS_STM32_G4"),
    ("STM32U5", "FL_IS_STM32_U5"),
    ("STM32U5xx", "FL_IS_STM32_U5"),
    ("STM32U585xx", "FL_IS_STM32_U5"),
    ("STM32U585XX", "FL_IS_STM32_U5"),
    ("ARDUINO_UNO_Q", "FL_IS_STM32_U5"),
    ("CONFIG_BOARD_ARDUINO_UNO_Q", "FL_IS_STM32_U5"),
    ("CONFIG_SOC_STM32U585XX", "FL_IS_STM32_U5"),
    ("NRF51", "FL_IS_NRF52"),
    ("NRF52_SERIES", "FL_IS_NRF52"),
    ("NRF52840_XXAA", "FL_IS_NRF52840"),
    ("NRF52833_XXAA", "FL_IS_NRF52833"),
    ("NRF52832_XXAA", "FL_IS_NRF52832"),
    ("NRF52", "FL_IS_NRF52"),
    ("NRF52840", "FL_IS_NRF52840"),
    ("NRF52833", "FL_IS_NRF52833"),
    ("NRF52832", "FL_IS_NRF52832"),
    ("__SAMD21G18A__", "FL_IS_SAMD21"),
    ("__SAMD21J18A__", "FL_IS_SAMD21"),
    ("__SAMD21E17A__", "FL_IS_SAMD21"),
    ("__SAMD21E18A__", "FL_IS_SAMD21"),
    ("__SAMD21__", "FL_IS_SAMD21"),
    ("__SAMD51G19A__", "FL_IS_SAMD51"),
    ("__SAMD51J19A__", "FL_IS_SAMD51"),
    ("__SAME51J19A__", "FL_IS_SAMD51"),
    ("__SAMD51P19A__", "FL_IS_SAMD51"),
    ("__SAMD51P20A__", "FL_IS_SAMD51"),
    ("__SAMD51J20A__", "FL_IS_SAMD51"),
    ("__SAMD51__", "FL_IS_SAMD51"),
    ("TARGET_RP2040", "FL_IS_RP2040"),
    ("ARDUINO_ARCH_RP2040", "FL_IS_RP2040"),
    ("PICO_RP2040", "FL_IS_RP2040"),
    ("ARDUINO_ARCH_RP2350", "FL_IS_RP2350"),
    ("PICO_RP2350", "FL_IS_RP2350"),
    ("ARDUINO_ARCH_SAMD", "FL_IS_SAMD"),
    ("ARDUINO_ARCH_STM32", "FL_IS_STM32"),
    ("ARDUINO_ARCH_NRF52", "FL_IS_NRF52"),
    ("ARDUINO_ARCH_APOLLO3", "FL_IS_APOLLO3"),
    ("ARDUINO_ARCH_RENESAS", "FL_IS_RENESAS"),
    ("ARDUINO_ARCH_RENESAS_UNO", "FL_IS_RENESAS"),
    ("ARDUINO_ARCH_RENESAS_PORTENTA", "FL_IS_RENESAS"),
    ("ARDUINO_ARCH_SILABS", "FL_IS_SILABS"),
    ("ARDUINO_ARCH_AVR", "FL_IS_AVR"),
    ("ARDUINO_ARCH_MBED", "FL_IS_STM32_MBED"),
    ("ARDUINO_SAM_DUE", "FL_IS_SAM"),
    ("ARDUINO_GIGA", "FL_IS_STM32_H7"),
    ("ARDUINO_GIGA_M7", "FL_IS_STM32_H7"),
    ("ARDUINO_NRF52_ADAFRUIT", "FL_IS_NRF52"),
    ("__EMSCRIPTEN__", "FL_IS_WASM"),
    ("SPARK", "FL_IS_STM32_F2"),
    ("APOLLO3", "FL_IS_APOLLO3"),
    ("TEENSYDUINO", "FL_IS_TEENSY"),
    ("CORE_TEENSY", "FL_IS_TEENSY"),
    ("FASTLED_TEENSY3", "FL_IS_TEENSY_3X"),
    ("FASTLED_TEENSY4", "FL_IS_TEENSY_4X"),
    ("FASTLED_TEENSYLC", "FL_IS_TEENSY_LC"),
    ("__RFduino__", "FL_IS_NRF52"),
    ("__Simblee__", "FL_IS_NRF52"),
    ("__APPLE__", "FL_IS_APPLE"),
    ("__linux__", "FL_IS_LINUX"),
    ("__unix__", "FL_IS_POSIX"),
    ("_WIN32", "FL_IS_WIN"),
    ("_WIN64", "FL_IS_WIN"),
    ("__CYGWIN__", "FL_IS_WIN"),
    ("__MINGW32__", "FL_IS_WIN_MINGW"),
    ("__MINGW64__", "FL_IS_WIN_MINGW"),
    ("_MSC_VER", "FL_IS_WIN_MSVC"),
    ("__FreeBSD__", "FL_IS_BSD"),
    ("__OpenBSD__", "FL_IS_BSD"),
    ("__NetBSD__", "FL_IS_BSD"),
    ("__DragonFly__", "FL_IS_BSD"),
    ("__wasm__", "FL_IS_WASM"),
    ("EMSCRIPTEN", "FL_IS_WASM"),
    ("__clang__", "FL_IS_CLANG"),
    ("__GNUC__", "FL_IS_GCC"),
    ("__GNUG__", "FL_IS_GCC"),
];

const NATIVE_ROOT_DISPATCH_HEADERS: &[&str] = &[
    "platforms.h",
    "led_sysdefs.h",
    "fastspi.h",
    "fastled_progmem.h",
];

const NATIVE_DISPATCH_CONFIG_FILES: &[&str] = &["config.h"];

const NATIVE_COMPILER_ABSTRACTION_FILES: &[&str] = &[
    "deprecated.h",
    "align.h",
    "export.h",
    "fltest.h",
    "type_traits.h",
    "m0clockless_c.h",
];

const BANNED_HEADERS_COMMON_RS: &[&str] = &[
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
    "stdlib.h",
    "malloc.h",
    "string.h",
    "type_traits",
    "new",
];

const BANNED_HEADERS_CORE_RS: &[&str] = &[
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
    "stdlib.h",
    "malloc.h",
    "string.h",
    "type_traits",
    "new",
    "Arduino.h",
];

const BANNED_HEADER_RECOMMENDATIONS: &[(&str, &str)] = &[
    (
        "Arduino.h",
        "fl/system/arduino.h (trampoline that includes Arduino.h + cleans up macros)",
    ),
    (
        "pthread.h",
        "fl/stl/thread.h or fl/stl/mutex.h (depending on what you need)",
    ),
    (
        "assert.h",
        "FL_CHECK or FL_ASSERT macros (check fl/stl/compiler_control.h)",
    ),
    ("iostream", "fl/stl/iostream.h or fl/str.h"),
    (
        "stdio.h",
        "fl/stl/stdio.h (provides fl::printf, fl::snprintf, fl::sprintf)",
    ),
    (
        "cstdio",
        "fl/stl/cstdio.h (provides fl::print, fl::println)",
    ),
    ("cstdlib", "fl/stl/cstdlib.h or fl/string operations"),
    ("vector", "fl/stl/vector.h"),
    ("list", "fl/stl/list.h or fl/stl/vector.h"),
    (
        "map",
        "fl/stl/map.h (check fl/stl/hash_map.h for hash-based)",
    ),
    ("unordered_map", "fl/stl/unordered_map.h"),
    ("set", "fl/stl/set.h"),
    ("unordered_set", "fl/stl/unordered_set.h"),
    ("multimap", "fl/stl/multi_map.h"),
    ("multiset", "fl/stl/multi_set.h"),
    (
        "queue",
        "fl/stl/queue.h or fl/stl/vector.h with manual queue semantics",
    ),
    ("deque", "fl/stl/deque.h"),
    ("algorithm", "fl/stl/algorithm.h"),
    ("memory", "fl/stl/shared_ptr.h or fl/stl/unique_ptr.h"),
    ("thread", "fl/stl/thread.h"),
    ("mutex", "fl/stl/mutex.h"),
    ("chrono", "fl/time.h"),
    ("fstream", "fl/file.h or platform file operations"),
    ("sstream", "fl/stl/sstream.h"),
    ("iomanip", "fl/stl/iostream.h stream manipulators"),
    (
        "exception",
        "Use error codes or fl/stl/exception.h if available",
    ),
    ("stdexcept", "Use error codes instead"),
    (
        "typeinfo",
        "Use fl/stl/type_traits.h or RTTI if unavoidable",
    ),
    ("ctime", "fl/time.h"),
    ("cmath", "fl/math.h"),
    ("math.h", "fl/math.h"),
    ("complex", "Custom complex number class or fl/geometry.h"),
    ("valarray", "fl/stl/vector.h"),
    ("cfloat", "fl/stl/limits.h or platform-specific headers"),
    ("cassert", "FL_CHECK macros from fl/stl/compiler_control.h"),
    ("cerrno", "Error handling through return codes"),
    ("cctype", "Character classification (implement if needed)"),
    (
        "cwctype",
        "Wide character classification (implement if needed)",
    ),
    ("cstring", "fl/str.h or fl/stl/cstring.h"),
    ("cwchar", "Wide character support (implement if needed)"),
    ("cuchar", "Character support (implement if needed)"),
    ("cstdint", "fl/stl/stdint.h"),
    ("stdint.h", "fl/stl/stdint.h"),
    ("stddef.h", "fl/stl/stddef.h or fl/stl/cstddef.h"),
    ("cstddef", "fl/stl/cstddef.h"),
    (
        "stdlib.h",
        "fl/stl/cstdlib.h (provides fl::aligned_alloc, fl::strtol, fl::atoi, etc.)",
    ),
    (
        "malloc.h",
        "fl/stl/cstdlib.h (provides fl::aligned_alloc / fl::aligned_free)",
    ),
    (
        "string.h",
        "fl/str.h (or use extern declarations for memset/memcpy if only C functions needed)",
    ),
    ("type_traits", "fl/stl/type_traits.h"),
    (
        "new",
        "Use stack allocation or custom allocators (placement new allowed in inplacenew.h)",
    ),
];

const PRIVATE_LIBCPP_HEADER_MAPPINGS_RS: &[(&str, &str)] = &[
    ("__algorithm", "\"fl/stl/algorithm.h\""),
    ("__atomic", "\"fl/stl/atomic.h\""),
    ("__chrono", "\"fl/stl/thread.h\""),
    ("__functional", "\"fl/stl/functional.h\""),
    ("__fwd/string", "\"fl/stl/string.h\""),
    ("__iterator", "\"fl/stl/iterator.h\""),
    ("__numeric", "\"fl/stl/algorithm.h\""),
    ("__ostream", "\"fl/stl/ostream.h\""),
    ("__random", "\"fl/math/random.h\""),
    ("__thread", "\"fl/stl/thread.h\""),
    ("__type_traits", "\"fl/stl/type_traits.h\""),
    ("__utility", "\"fl/stl/utility.h\""),
    ("__vector", "\"fl/stl/vector.h\""),
    ("__hash_table", "\"fl/stl/unordered_map.h\""),
    ("__tree", "\"fl/stl/map.h\""),
    ("__node_handle", "\"fl/stl/unordered_map.h\""),
];

const BANNED_HEADER_EXCEPTIONS: &[(&str, &str)] = &[
    ("new", "inplacenew.h"),
    ("new", "platforms/wasm/new.h"),
    ("new", "platforms/arm/new.h"),
    ("new", "platforms/esp/new.h"),
    ("new", "platforms/shared/new.h"),
    ("Arduino.h", "fl/system/arduino.h"),
    ("Arduino.h", "third_party/ezws2812/ezWS2812.h"),
    (
        "Arduino.h",
        "third_party/object_fled/src/ObjectFLEDDmaManager.h",
    ),
    ("Arduino.h", "platforms/wasm/compiler/Arduino.h"),
    ("Arduino.h", "platforms/wasm/led_sysdefs_wasm.h"),
    ("pthread.h", "fl/stl/thread_local.h"),
    ("mutex", "fl/stl/mutex.h"),
    ("mutex", "platforms/stub/thread_stub_stl.h"),
    ("mutex", "platforms/stub/mutex_stub_stl.h"),
    ("mutex", "platforms/esp/32/mutex_esp32.h"),
    ("mutex", "platforms/arm/stm32/mutex_stm32.h"),
    ("mutex", "platforms/arm/d21/mutex_samd.h"),
    ("mutex", "platforms/arm/rp/mutex_rp.h"),
    ("thread", "fl/async.cpp.hpp"),
    ("thread", "platforms/stub/isr_stub.hpp"),
    ("thread", "platforms/stub/time_stub.cpp"),
    ("thread", "platforms/stub/thread_stub_stl.h"),
    ("thread", "platforms/wasm/timer.cpp"),
    ("iostream", "platforms/stub/isr_stub.hpp"),
    ("algorithm", "fl/stl/mutex.h"),
    ("algorithm", "platforms/stub/fs_stub.hpp"),
    ("stdint.h", "fl/stl/stdint.h"),
    ("stdint.h", "fl/stl/time.cpp"),
    ("stdint.h", "fl/stl/time.cpp.hpp"),
    ("chrono", "fl/stl/time.cpp"),
    ("chrono", "fl/stl/time.cpp.hpp"),
    ("chrono", "platforms/stub/isr_stub.hpp"),
    ("chrono", "platforms/stub/time_stub.cpp"),
    ("chrono", "platforms/esp/32/condition_variable_esp32.h"),
    ("chrono", "platforms/esp/32/semaphore_esp32.h"),
    ("string.h", "fl/stl/str.cpp"),
    ("string.h", "fl/stl/cstring.cpp.hpp"),
    ("string.h", "fl/stl/detail/string_holder.cpp.hpp"),
    ("stdlib.h", "fl/stl/str.cpp"),
    ("stdlib.h", "fl/stl/cstdlib.cpp.hpp"),
    ("stdlib.h", "fl/stl/cstring.cpp.hpp"),
    ("stdlib.h", "fl/stl/detail/string_holder.cpp.hpp"),
    ("stdlib.h", "fl/stl/malloc.cpp.hpp"),
    ("stdlib.h", "fl/stl/undef.h"),
    ("stdlib.h", "platforms/arm/teensy/coroutine_teensy.impl.hpp"),
    (
        "stdlib.h",
        "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.cpp.hpp",
    ),
    ("stdlib.h", "platforms/shared/ui/json/json_console.cpp.hpp"),
    ("stdlib.h", "third_party/libhelix_mp3/real/buffers.hpp"),
    ("stdlib.h", "third_party/stb/truetype/stb_truetype.cpp.hpp"),
    ("malloc.h", "fl/stl/cstdlib.cpp.hpp"),
    ("malloc.h", "fl/stl/alloca.h"),
    (
        "malloc.h",
        "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.cpp.hpp",
    ),
    ("malloc.h", "third_party/stb/stb_vorbis.cpp.hpp"),
    ("math.h", "fl/math/math.cpp"),
    ("math.h", "fl/math/math.cpp.hpp"),
    ("math.h", "fl/audio/audio_reactive.cpp"),
    ("math.h", "fl/colorutils.cpp"),
    ("math.h", "fl/transform.cpp"),
    ("math.h", "fl/xypath.cpp"),
    ("math.h", "fl/xypath_impls.cpp"),
    ("math.h", "fl/xypath_renderer.cpp"),
    ("math.h", "fx/video/frame_interpolator.cpp"),
    ("cmath", "platforms/wasm/timer.cpp"),
    ("cerrno", "fl/stl/cerrno.h"),
    ("cerrno", "fl/stl/fstream.h"),
    ("cstdio", "fl/stl/detail/file_io.h"),
    ("cstdio", "platforms/stub/fs_stub.hpp"),
    ("cstdio", "platforms/wasm/io_wasm.h"),
    ("cstdio", "platforms/wasm/compiler/wasm_pch.h"),
    ("cstdlib", "fl/stl/cstdlib.cpp.hpp"),
];

#[derive(Debug, Clone)]
pub struct FileContent {
    pub path: String,
    pub content: String,
    pub lines: Vec<String>,
}

impl FileContent {
    fn read(path: &Path) -> Result<Self, DynError> {
        let content = fs::read_to_string(path)?;
        let lines = content.lines().map(str::to_string).collect();
        Ok(Self {
            path: normalize_path(&path_to_string(path)),
            content,
            lines,
        })
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Serialize, Deserialize)]
pub struct LintViolation {
    pub checker: String,
    pub path: String,
    pub line: usize,
    pub message: String,
}

pub trait FileContentChecker: Sync {
    fn name(&self) -> &'static str;
    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool;
    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)>;
}

pub struct MultiCheckerFileProcessor {
    file_cache: HashMap<PathBuf, FileContent>,
}

impl MultiCheckerFileProcessor {
    pub fn new() -> Self {
        Self {
            file_cache: HashMap::new(),
        }
    }

    pub fn process_files_with_checkers(
        &mut self,
        file_paths: &[PathBuf],
        checkers: &[Box<dyn FileContentChecker>],
        project_root: &Path,
    ) -> Result<Vec<LintViolation>, DynError> {
        let mut unique_paths = BTreeSet::new();
        for path in file_paths {
            unique_paths.insert(path.clone());
        }

        let pending_paths: Vec<PathBuf> = unique_paths
            .iter()
            .filter(|path| !self.file_cache.contains_key(*path))
            .cloned()
            .collect();

        let loaded_files: Vec<(PathBuf, Result<FileContent, DynError>)> = pending_paths
            .par_iter()
            .map(|path| (path.clone(), FileContent::read(path)))
            .collect();

        for (path, loaded) in loaded_files {
            match loaded {
                Ok(content) => {
                    self.file_cache.insert(path, content);
                }
                Err(error) => {
                    eprintln!(
                        "warning: skipped unreadable file {}: {error}",
                        path.display()
                    );
                }
            }
        }

        let files: Vec<FileContent> = unique_paths
            .iter()
            .filter_map(|path| self.file_cache.get(path).cloned())
            .collect();

        let mut violations: Vec<LintViolation> = files
            .par_iter()
            .flat_map_iter(|file_content| {
                let mut file_violations = Vec::new();
                for checker in checkers {
                    if !checker.should_process_file(&file_content.path, project_root) {
                        continue;
                    }
                    for (line, message) in checker.check_file_content(file_content) {
                        file_violations.push(LintViolation {
                            checker: checker.name().to_string(),
                            path: file_content.path.clone(),
                            line,
                            message,
                        });
                    }
                }
                file_violations
            })
            .collect();

        violations.sort();
        Ok(violations)
    }
}

impl Default for MultiCheckerFileProcessor {
    fn default() -> Self {
        Self::new()
    }
}

pub fn supported_checker_names() -> &'static [&'static str] {
    &[
        "arduino_macro_usage",
        "asm_js_location",
        "attribute",
        "bare_allocation",
        "bare_using",
        "banned_headers",
        "banned_define",
        "banned_macros",
        "banned_namespace",
        "builtin_memcpy",
        "cpp_hpp_includes",
        "cpp_hpp_header_pair",
        "cpp_include",
        "ctype_global",
        "enum_class",
        "esp_rom_printf",
        "fastled_header_usage",
        "fl_is_defined",
        "headers_exist",
        "include_after_namespace",
        "include_paths",
        "impl_hpp_includes",
        "logging_in_iram",
        "is_header_include",
        "iwyu_pragma_block",
        "member_style",
        "namespace_fl_declaration",
        "namespace_includes",
        "namespace_platforms",
        "native_platform_defines",
        "noexcept_special_members",
        "numeric_limit_macros",
        "platform_includes",
        "platforms_fl_namespace",
        "pragma_once",
        "platform_pragma",
        "platform_trampoline",
        "raw_noexcept",
        "raw_pragma",
        "reinterpret_cast",
        "relative_include",
        "serial_printf",
        "simd_intrinsics",
        "sleep_for",
        "span_from_pointer",
        "static_in_headers",
        "std_namespace",
        "singleton_in_headers",
        "stdint_type",
        "subdir_namespace",
        "test_aggregation",
        "test_include_paths",
        "test_path_structure",
        "thread_local_keyword",
        "example_serial",
        "unit_test",
        "using_namespace",
        "using_namespace_fl",
        "using_namespace_fl_in_examples",
        "weak_attribute",
    ]
}

pub fn supported_python_checker_names() -> &'static [&'static str] {
    &[
        "ArduinoMacroUsageChecker",
        "AsmJsLocationChecker",
        "AttributeChecker",
        "BareAllocationChecker",
        "BareUsingChecker",
        "BannedDefineChecker",
        "BannedHeadersChecker",
        "BannedMacrosChecker",
        "BannedNamespaceChecker",
        "BuiltinMemcpyChecker",
        "CppHppIncludesChecker",
        "CppHppHeaderPairChecker",
        "CppIncludeChecker",
        "CtypeGlobalChecker",
        "EnumClassChecker",
        "EspRomPrintfChecker",
        "ExampleSerialChecker",
        "FastLEDHeaderUsageChecker",
        "FlIsDefinedChecker",
        "HeadersExistChecker",
        "IncludeAfterNamespaceChecker",
        "IncludePathsChecker",
        "ImplHppIncludesChecker",
        "IsHeaderIncludeChecker",
        "IwyuPragmaBlockChecker",
        "LoggingInIramChecker",
        "MemberStyleChecker",
        "NamespaceFlDeclarationChecker",
        "NamespaceIncludesChecker",
        "NamespacePlatformsChecker",
        "NativePlatformDefinesChecker",
        "NoexceptSpecialMembersChecker",
        "NumericLimitMacroChecker",
        "PlatformIncludesChecker",
        "PlatformsFlNamespaceChecker",
        "PragmaOnceChecker",
        "PlatformPragmaChecker",
        "PlatformTrampolineChecker",
        "RawNoexceptChecker",
        "RawPragmaChecker",
        "ReinterpretCastChecker",
        "RelativeIncludeChecker",
        "SerialPrintfChecker",
        "SimdIntrinsicsChecker",
        "SleepForChecker",
        "SingletonInHeadersChecker",
        "SpanFromPointerChecker",
        "StaticInHeaderChecker",
        "StdNamespaceChecker",
        "StdintTypeChecker",
        "SubdirNamespaceChecker",
        "TestAggregationChecker",
        "TestIncludePathsChecker",
        "TestPathStructureChecker",
        "ThreadLocalKeywordChecker",
        "UnitTestChecker",
        "UsingNamespaceChecker",
        "UsingNamespaceFlChecker",
        "UsingNamespaceFlInExamplesChecker",
        "WeakAttributeChecker",
    ]
}

pub fn create_checkers(
    selected: Option<&HashSet<String>>,
) -> Result<Vec<Box<dyn FileContentChecker>>, DynError> {
    let mut checkers: Vec<(&'static str, Box<dyn FileContentChecker>)> = vec![
        ("arduino_macro_usage", Box::new(ArduinoMacroUsageChecker)),
        ("asm_js_location", Box::new(AsmJsLocationChecker)),
        ("attribute", Box::new(AttributeChecker)),
        ("bare_allocation", Box::new(BareAllocationChecker)),
        ("bare_using", Box::new(BareUsingChecker)),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: true,
                scope: BannedHeadersScope::Fl,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: true,
                scope: BannedHeadersScope::Lib8tion,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: false,
                scope: BannedHeadersScope::FxSensorsPlatformsShared,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Platforms,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Examples,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: true,
                scope: BannedHeadersScope::ThirdParty,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Tests,
            }),
        ),
        ("banned_define", Box::new(BannedDefineChecker)),
        ("banned_macros", Box::new(BannedMacrosChecker)),
        ("banned_namespace", Box::new(BannedNamespaceChecker)),
        ("builtin_memcpy", Box::new(BuiltinMemcpyChecker)),
        ("cpp_hpp_includes", Box::new(CppHppIncludesChecker)),
        ("cpp_hpp_header_pair", Box::new(CppHppHeaderPairChecker)),
        ("cpp_include", Box::new(CppIncludeChecker)),
        ("ctype_global", Box::new(CtypeGlobalChecker)),
        ("enum_class", Box::new(EnumClassChecker)),
        ("esp_rom_printf", Box::new(EspRomPrintfChecker)),
        ("example_serial", Box::new(ExampleSerialChecker)),
        ("fastled_header_usage", Box::new(FastLEDHeaderUsageChecker)),
        ("fl_is_defined", Box::new(FlIsDefinedChecker)),
        ("headers_exist", Box::new(HeadersExistChecker)),
        (
            "include_after_namespace",
            Box::new(IncludeAfterNamespaceChecker),
        ),
        ("include_paths", Box::new(IncludePathsChecker)),
        ("impl_hpp_includes", Box::new(ImplHppIncludesChecker)),
        ("is_header_include", Box::new(IsHeaderIncludeChecker)),
        ("iwyu_pragma_block", Box::new(IwyuPragmaBlockChecker)),
        ("logging_in_iram", Box::new(LoggingInIramChecker)),
        ("member_style", Box::new(MemberStyleChecker)),
        (
            "namespace_fl_declaration",
            Box::new(NamespaceFlDeclarationChecker),
        ),
        ("namespace_includes", Box::new(NamespaceIncludesChecker)),
        ("namespace_platforms", Box::new(NamespacePlatformsChecker)),
        (
            "native_platform_defines",
            Box::new(NativePlatformDefinesChecker),
        ),
        (
            "noexcept_special_members",
            Box::new(NoexceptSpecialMembersChecker),
        ),
        ("numeric_limit_macros", Box::new(NumericLimitMacroChecker)),
        ("platform_includes", Box::new(PlatformIncludesChecker)),
        (
            "platforms_fl_namespace",
            Box::new(PlatformsFlNamespaceChecker),
        ),
        ("pragma_once", Box::new(PragmaOnceChecker)),
        ("platform_pragma", Box::new(PlatformPragmaChecker)),
        ("platform_trampoline", Box::new(PlatformTrampolineChecker)),
        ("raw_noexcept", Box::new(RawNoexceptChecker)),
        ("raw_pragma", Box::new(RawPragmaChecker)),
        ("reinterpret_cast", Box::new(ReinterpretCastChecker)),
        ("relative_include", Box::new(RelativeIncludeChecker)),
        ("serial_printf", Box::new(SerialPrintfChecker)),
        ("simd_intrinsics", Box::new(SimdIntrinsicsChecker)),
        ("sleep_for", Box::new(SleepForChecker)),
        ("singleton_in_headers", Box::new(SingletonInHeadersChecker)),
        ("span_from_pointer", Box::new(SpanFromPointerChecker)),
        ("static_in_headers", Box::new(StaticInHeaderChecker)),
        ("std_namespace", Box::new(StdNamespaceChecker)),
        ("stdint_type", Box::new(StdintTypeChecker)),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "net" }),
        ),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "video" }),
        ),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "task" }),
        ),
        ("test_aggregation", Box::new(TestAggregationChecker)),
        ("test_include_paths", Box::new(TestIncludePathsChecker)),
        ("test_path_structure", Box::new(TestPathStructureChecker)),
        ("thread_local_keyword", Box::new(ThreadLocalKeywordChecker)),
        ("unit_test", Box::new(UnitTestChecker)),
        ("using_namespace", Box::new(UsingNamespaceChecker)),
        ("using_namespace_fl", Box::new(UsingNamespaceFlChecker)),
        (
            "using_namespace_fl_in_examples",
            Box::new(UsingNamespaceFlInExamplesChecker),
        ),
        ("weak_attribute", Box::new(WeakAttributeChecker)),
    ];

    if let Some(selected) = selected {
        let known: HashSet<&str> = checkers.iter().map(|(name, _)| *name).collect();
        let unknown: Vec<&String> = selected
            .iter()
            .filter(|name| !known.contains(name.as_str()))
            .collect();
        if !unknown.is_empty() {
            return Err(format!("unknown Rust C++ checker(s): {unknown:?}").into());
        }
        checkers.retain(|(name, _)| selected.contains(*name));
    }

    Ok(checkers.into_iter().map(|(_, checker)| checker).collect())
}

pub fn run_cli<I>(args: I) -> Result<u8, DynError>
where
    I: IntoIterator<Item = String>,
{
    let config = CliConfig::parse(args)?;

    if config.show_help {
        return Ok(0);
    }

    if config.list_checkers {
        for checker in supported_checker_names() {
            println!("{checker}");
        }
        return Ok(0);
    }

    let selected = config.selected_checkers.as_ref();
    let checkers = create_checkers(selected)?;
    let files = collect_input_files(&config.project_root, &config.paths)?;
    let mut processor = MultiCheckerFileProcessor::new();
    let violations =
        processor.process_files_with_checkers(&files, &checkers, &config.project_root)?;

    match config.output_format {
        OutputFormat::Json => {
            println!("{}", serde_json::to_string_pretty(&violations)?);
        }
        OutputFormat::Text => print_text_results(&violations, &config.project_root),
    }

    Ok(if violations.is_empty() { 0 } else { 1 })
}

#[derive(Debug)]
struct CliConfig {
    output_format: OutputFormat,
    project_root: PathBuf,
    selected_checkers: Option<HashSet<String>>,
    show_help: bool,
    list_checkers: bool,
    paths: Vec<String>,
}

#[derive(Debug, Clone, Copy)]
enum OutputFormat {
    Json,
    Text,
}

impl CliConfig {
    fn parse<I>(args: I) -> Result<Self, DynError>
    where
        I: IntoIterator<Item = String>,
    {
        let mut output_format = OutputFormat::Text;
        let mut project_root = std::env::current_dir()?;
        let mut selected_checkers = HashSet::new();
        let mut list_checkers = false;
        let mut paths = Vec::new();

        let mut iter = args.into_iter();
        while let Some(arg) = iter.next() {
            match arg.as_str() {
                "--format" => {
                    let value = iter.next().ok_or("--format requires json or text")?;
                    output_format = match value.as_str() {
                        "json" => OutputFormat::Json,
                        "text" => OutputFormat::Text,
                        other => return Err(format!("unsupported --format value: {other}").into()),
                    };
                }
                "--project-root" => {
                    let value = iter.next().ok_or("--project-root requires a path")?;
                    project_root = PathBuf::from(value).canonicalize()?;
                }
                "--checker" => {
                    let value = iter.next().ok_or("--checker requires a checker name")?;
                    for checker in value.split(',') {
                        let checker = checker.trim();
                        if !checker.is_empty() {
                            selected_checkers.insert(checker.to_string());
                        }
                    }
                }
                "--list-checkers" => list_checkers = true,
                "--help" | "-h" => {
                    print_help();
                    return Ok(Self {
                        output_format,
                        project_root,
                        selected_checkers: None,
                        show_help: true,
                        list_checkers: false,
                        paths,
                    });
                }
                _ if arg.starts_with("--") => {
                    return Err(format!("unknown argument: {arg}").into());
                }
                _ => paths.push(arg),
            }
        }

        let selected_checkers = if selected_checkers.is_empty() {
            None
        } else {
            Some(selected_checkers)
        };

        Ok(Self {
            output_format,
            project_root,
            selected_checkers,
            show_help: false,
            list_checkers,
            paths,
        })
    }
}

fn print_help() {
    println!(
        "fastled-lint\n\
\n\
Usage:\n\
  fastled-lint [--format text|json] [--checker name[,name...]] [--project-root PATH] [files-or-globs...]\n\
\n\
When no files are supplied, scans src/, examples/, and tests/ under --project-root.\n\
Use --list-checkers to print the Rust-supported checker names."
    );
}

fn print_text_results(violations: &[LintViolation], project_root: &Path) {
    if violations.is_empty() {
        println!("All Rust C++ linting checks passed!");
        return;
    }

    let mut current_checker = "";
    for violation in violations {
        if violation.checker != current_checker {
            current_checker = &violation.checker;
            println!("\n[{current_checker}]");
        }
        let display_path = relative_display_path(&violation.path, project_root);
        println!("  {display_path}:{}: {}", violation.line, violation.message);
    }
}

fn collect_input_files(project_root: &Path, inputs: &[String]) -> Result<Vec<PathBuf>, DynError> {
    let mut files = BTreeSet::new();

    if inputs.is_empty() {
        for dir in ["src", "examples", "tests"] {
            let root = project_root.join(dir);
            if root.exists() {
                collect_directory_files(&root, &mut files);
            }
        }
    } else {
        for input in inputs {
            let mut input_files = BTreeSet::new();
            if input.contains('*') || input.contains('?') || input.contains('[') {
                let mut matched = false;
                for entry in glob(input)? {
                    matched = true;
                    let path = entry?;
                    collect_path(&path, &mut input_files, false);
                }
                if !matched || input_files.is_empty() {
                    return Err(format!("input pattern matched no files: {input}").into());
                }
            } else {
                let path = PathBuf::from(input);
                if !path.exists() {
                    return Err(format!("input path not found: {input}").into());
                }
                collect_path(&path, &mut input_files, true);
                if input_files.is_empty() {
                    return Err(format!("input path produced no lintable files: {input}").into());
                }
            }
            files.extend(input_files);
        }
    }

    Ok(files.into_iter().collect())
}

fn collect_path(path: &Path, files: &mut BTreeSet<PathBuf>, allow_any_file: bool) {
    if path.is_dir() {
        collect_directory_files(path, files);
    } else if path.is_file()
        && (allow_any_file || is_cpp_like_path(&normalize_path(&path_to_string(path))))
    {
        files.insert(path.to_path_buf());
    }
}

fn collect_directory_files(root: &Path, files: &mut BTreeSet<PathBuf>) {
    for entry in WalkDir::new(root)
        .into_iter()
        .filter_entry(|entry| should_visit_directory(entry.path()))
        .filter_map(Result::ok)
    {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        if parent_has_cpp_no_lint_marker(path) {
            continue;
        }
        if is_cpp_like_path(&normalize_path(&path_to_string(path))) {
            files.insert(path.to_path_buf());
        }
    }
}

fn should_visit_directory(path: &Path) -> bool {
    if !path.is_dir() {
        return true;
    }
    let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
        return true;
    };
    !name.starts_with(".build")
        && !matches!(
            name,
            "build" | ".venv" | ".cache" | "__pycache__" | "node_modules"
        )
}

fn parent_has_cpp_no_lint_marker(path: &Path) -> bool {
    path.parent()
        .map(|parent| parent.join(".cpp_no_lint").exists())
        .unwrap_or(false)
}

fn is_cpp_like_path(path: &str) -> bool {
    CPP_EXTENSIONS
        .iter()
        .any(|extension| path.ends_with(&format!(".{extension}")))
}

fn normalize_path(path: &str) -> String {
    let normalized = path.replace('\\', "/");
    if let Some(rest) = normalized.strip_prefix("//?/UNC/") {
        return format!("//{rest}");
    }
    if let Some(rest) = normalized.strip_prefix("//?/") {
        return rest.to_string();
    }
    normalized
}

fn path_to_string(path: &Path) -> String {
    path.to_string_lossy().to_string()
}

fn relative_display_path(path: &str, project_root: &Path) -> String {
    let normalized_root = normalize_path(&path_to_string(project_root));
    let normalized_path = normalize_path(path);
    normalized_path
        .strip_prefix(&format!("{normalized_root}/"))
        .unwrap_or(&normalized_path)
        .to_string()
}

fn is_under_dir(path: &str, dir: &str) -> bool {
    let normalized = normalize_path(path);
    normalized == dir
        || normalized.starts_with(&format!("{dir}/"))
        || normalized.contains(&format!("/{dir}/"))
}

fn is_under_project_subpath(path: &str, project_root: &Path, subpath: &str) -> bool {
    let normalized_path = normalize_path(path);
    let normalized_root = normalize_path(&path_to_string(project_root));
    let subpath = subpath.trim_matches('/');
    normalized_path == format!("{normalized_root}/{subpath}")
        || normalized_path.starts_with(&format!("{normalized_root}/{subpath}/"))
        || normalized_path == subpath
        || normalized_path.starts_with(&format!("{subpath}/"))
}

fn ends_with_any(path: &str, suffixes: &[&str]) -> bool {
    suffixes.iter().any(|suffix| path.ends_with(suffix))
}

fn is_excluded_file(path: &str) -> bool {
    EXCLUDED_FILES
        .iter()
        .any(|excluded| path.ends_with(excluded))
}

fn split_line_comment(line: &str) -> &str {
    line.split("//").next().unwrap_or(line)
}

fn strip_block_comments_from_line(line: &str, in_block_comment: &mut bool) -> String {
    let mut visible = String::new();
    let mut rest = line;

    loop {
        if *in_block_comment {
            let Some(end) = rest.find("*/") else {
                return visible;
            };
            rest = &rest[end + 2..];
            *in_block_comment = false;
            continue;
        }

        let Some(start) = rest.find("/*") else {
            visible.push_str(rest);
            return visible;
        };
        visible.push_str(&rest[..start]);
        rest = &rest[start + 2..];
        *in_block_comment = true;
    }
}

fn strip_string_literals(code: &str) -> String {
    let mut result = String::with_capacity(code.len());
    let mut quote: Option<char> = None;
    let mut escaped = false;

    for ch in code.chars() {
        match quote {
            None => {
                if ch == '"' || ch == '\'' {
                    quote = Some(ch);
                }
                result.push(ch);
            }
            Some(active_quote) => {
                if escaped {
                    escaped = false;
                    result.push(' ');
                } else if ch == '\\' {
                    escaped = true;
                    result.push(' ');
                } else if ch == active_quote {
                    quote = None;
                    result.push(ch);
                } else {
                    result.push(' ');
                }
            }
        }
    }

    result
}

fn regex_has_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__has_include\s*\(").unwrap())
}

fn regex_static_assert() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bstatic_assert\s*\(").unwrap())
}

fn regex_using_namespace_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+fl\s*;").unwrap())
}

fn regex_string_literal() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#""(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'"#).unwrap())
}

fn regex_new_alloc() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnew\s+[A-Za-z_:]").unwrap())
}

fn regex_delete() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bdelete\b").unwrap())
}

fn regex_malloc_family() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(malloc|calloc|realloc)\s*\(").unwrap())
}

fn regex_free() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfree\s*\(").unwrap())
}

fn regex_deleted_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*[;{]").unwrap())
}

fn regex_deleted_func_eol() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*$").unwrap())
}

fn regex_inline_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_template() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*template\s*<").unwrap())
}

fn regex_static_var() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"\bstatic\s+(?:const\s+)?[\w:]+(?:<[^>]+>)?\s+\w+\s*[=({]").unwrap()
    })
}

fn regex_static_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"static\s+[\w:]+(?:<[^>]+>)?\s+\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_include_path() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+"([^"]+)""#).unwrap())
}

fn regex_quoted_include_line() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+"([^"]+)""#).unwrap())
}

fn regex_builtin_memcpy() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__builtin_memcpy\s*\(").unwrap())
}

fn regex_weak_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"__attribute__\s*\(\s*\(\s*weak\s*\)\s*\)").unwrap())
}

fn regex_banned_namespace_fl_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfl::fl\b").unwrap())
}

fn regex_cpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+[<"]([^>"]+\.cpp)[>"]"#).unwrap())
}

fn regex_cpp_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.cpp\.hpp)[>"]"#).unwrap())
}

fn regex_include_any_path() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*[<"](.+?)[>"]"#).unwrap())
}

fn regex_iwyu_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#include\s+[<"]([^>"]+)[>"](?:\s*//\s*(.*))?"#).unwrap())
}

fn regex_iwyu_begin() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)^\s*//\s*IWYU\s+pragma:\s*begin_keep").unwrap())
}

fn regex_iwyu_end() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)^\s*//\s*IWYU\s+pragma:\s*end_keep").unwrap())
}

fn regex_iwyu_keep_inline() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)IWYU\s+pragma:\s*keep").unwrap())
}

fn regex_impl_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.impl\.hpp)[>"]"#).unwrap())
}

fn regex_asm_js_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:EM_JS|EM_ASYNC_JS|EM_ASM)\s*\(").unwrap())
}

fn regex_esp_rom_printf() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\besp_rom_printf\s*\(").unwrap())
}

fn regex_sleep_for() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bsleep_for\s*\(").unwrap())
}

fn regex_thread_local() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bthread_local\b").unwrap())
}

fn regex_span_data_size() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"span<[^>]+>\s*\([^)]*\.data\(\)[^)]*\.size\(\)[^)]*\)").unwrap()
    })
}

fn regex_relative_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]([^>"]*\.\..*)[>"]"#).unwrap())
}

fn regex_fastled_h_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]FastLED\.h[>"]"#).unwrap())
}

fn regex_fastled_internal_define() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*define\s+FASTLED_INTERNAL").unwrap())
}

fn regex_arduino_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_scoped_enum() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"::\s*(?:INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_enum_member() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:INPUT|OUTPUT|DEFAULT)\s*(?:=\s*\w+)?\s*[,}]").unwrap())
}

fn regex_arduino_preprocessor() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*(?:define|undef|ifdef|ifndef|if|elif|pragma|error|include)\b").unwrap()
    })
}

fn regex_standard_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\[\[\s*([a-z_]+)\s*\]\]").unwrap())
}

fn regex_alignas() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\balignas\s*\(").unwrap())
}

fn regex_preprocessor_if_elif() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*(?:if|elif)\b").unwrap())
}

fn regex_preprocessor_conditional() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b").unwrap())
}

fn regex_preprocessor_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^#\s*include\b").unwrap())
}

fn regex_namespace_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:namespace\s+\w+|namespace\s*\{)").unwrap())
}

fn regex_bare_using_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*using\s+(?:namespace\s+\w+(?:::\w+)*|\w+(?:::\w+)+)\s*;").unwrap()
    })
}

fn regex_bare_using_suppress() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*ok(?:ay)?\s+bare\s+using").unwrap())
}

fn regex_named_namespace_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s+\w+").unwrap())
}

fn regex_anon_namespace_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s*\{").unwrap())
}

fn regex_extern_c_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*extern\s+"C""#).unwrap())
}

fn regex_local_scope_keyword() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:class|struct|enum|union)\b").unwrap())
}

fn regex_preprocessor_line() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#").unwrap())
}

fn regex_any_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*[<"].*[>"]"#).unwrap())
}

fn regex_banned_header_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+[<"]([^>"]+)[>"]"#).unwrap())
}

fn regex_private_libcpp_header() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+["<](__[^_][^">]*)[">]"#).unwrap())
}

fn regex_namespace_include_using() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*using\s+namespace\s+\w+\s*;").unwrap())
}

fn regex_namespace_include_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s+\w+\s*\{").unwrap())
}

fn regex_namespace_include_directive() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*include").unwrap())
}

fn regex_named_enum() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\benum\s+([A-Za-z_]\w*)\s*[{:;]").unwrap())
}

fn regex_class_struct() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"\b(?:class|struct)\s+(?:[A-Za-z_]\w*\s+)*[A-Za-z_]\w*").unwrap()
    })
}

fn regex_noexcept_class_def() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:class|struct)\s+(\w+)").unwrap())
}

fn regex_noexcept_suppress_special() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:ok\s+no\s+FL_NOEXCEPT|nolint)\b").unwrap())
}

fn regex_has_noexcept_special() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:FL_NOEXCEPT|noexcept)\b").unwrap())
}

fn regex_destructor_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"~(\w+)\s*\(").unwrap())
}

fn regex_operator_assign_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\boperator\s*=\s*\(").unwrap())
}

fn regex_member_trailing_underscore() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"\b(?:const\s+)?(?:static\s+)?(?:volatile\s+)?(?:mutable\s+)?[\w:]+(?:<[^>]+>)?[\s*&]+\s*(\w{2,}_)\s*[;=,)]",
        )
        .unwrap()
    })
}

fn regex_member_m_underscore() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"\b(?:const\s+)?(?:static\s+)?(?:volatile\s+)?(?:mutable\s+)?[\w:]+(?:<[^>]+>)?[\s*&]+\s*(m_\w+)\s*[;=,)]",
        )
        .unwrap()
    })
}

fn regex_allow_include_after_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*allow-include-after-namespace").unwrap())
}

fn regex_nolint() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*nolint").unwrap())
}

fn regex_namespace_fl_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+fl\s*\{").unwrap())
}

fn regex_namespace_platform_singular() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+platform\s*\{").unwrap())
}

fn regex_subdir_namespace_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+([\w:]+)\s*\{").unwrap())
}

fn regex_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+\w+(?:::\w+)*\s*;").unwrap())
}

fn regex_define_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#define\s+\w+.*using\s+namespace").unwrap())
}

fn regex_force_use_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#if\s+.*FORCE_USE_NAMESPACE").unwrap())
}

fn regex_fl_is_token() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_IS_\w+\b").unwrap())
}

fn regex_defined_fl_is() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"!?\s*defined\s*\(\s*FL_IS_\w+\s*\)").unwrap())
}

fn regex_singleton() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingleton\s*<").unwrap())
}

fn regex_singleton_shared() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingletonShared\b").unwrap())
}

fn regex_friend_class() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfriend\s+class\b").unwrap())
}

fn regex_numeric_limit_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(&format!(r"\b({})\b", NUMERIC_LIMIT_MACROS.join("|"))).unwrap())
}

fn regex_platform_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*pragma\s+(?:GCC\s+diagnostic|clang\s+diagnostic|warning\s*\()")
            .unwrap()
    })
}

fn regex_raw_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b_Pragma\s*\(").unwrap())
}

fn regex_raw_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnoexcept\b").unwrap())
}

fn regex_define_fl_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#\s*define\s+FL_NOEXCEPT\b").unwrap())
}

fn regex_noexcept_suppression() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:ok\s+noexcept|nolint)\b").unwrap())
}

fn regex_std_usage() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"std::\w+").unwrap())
}

fn regex_serial_method() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSerial\.(\w+)\s*\(").unwrap())
}

fn regex_iram_function() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"FL_IRAM[\s\n]+(?:static\s+)?(?:inline\s+)?(?:virtual\s+)?(?:const\s+)?(?:(\w+(?:\s*<[^>]+>)?)\s+)?(?:__attribute__\s*\([^)]*\)\s+)?([\w:]+)\s*\([^)]*\)(?:\s*(?:const|override|final))?[^{]*\{",
        )
        .unwrap()
    })
}

fn regex_iram_banned_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(FL_WARN|FL_ERROR|FL_DBG|FL_ASSERT)\s*\(").unwrap())
}

fn regex_fl_log_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_LOG_\w+\s*\(").unwrap())
}

fn regex_unit_test_macro_call() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b([A-Z_]+)\s*\(").unwrap())
}

fn regex_deep_platform_header() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(#include\s+"(platforms/[^/]+/[^"]+\.h[^"]*)")"#).unwrap())
}

fn regex_deep_platform_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r#"^\s*#\s*include\s+[<"]platforms/([^/"]+)/([^"]+)[">]"#).unwrap()
    })
}

fn is_top_level_include(include_path: &str) -> bool {
    !include_path.contains('/') && !include_path.contains('\\')
}

fn include_path_looks_like_fastled_code(include_path: &str) -> bool {
    if include_path.ends_with(".hpp") || include_path.ends_with(".cpp") {
        return true;
    }

    let filename = include_path.rsplit('/').next().unwrap_or(include_path);
    let filename = filename.to_ascii_lowercase();
    [
        "clockless",
        "fastpin",
        "fastspi",
        "led_sysdefs",
        "compile_test",
        "spi_output",
        "is_",
    ]
    .iter()
    .any(|pattern| filename.contains(pattern))
}

fn is_external_sdk_header(include_path: &str) -> bool {
    if EXTERNAL_SDK_PREFIXES
        .iter()
        .any(|prefix| include_path.starts_with(prefix))
    {
        return true;
    }

    for prefix in AMBIGUOUS_INCLUDE_PREFIXES {
        if include_path.starts_with(prefix) {
            return !include_path_looks_like_fastled_code(include_path);
        }
    }

    false
}

fn is_fastled_platform_relative(include_path: &str) -> bool {
    if FASTLED_PLATFORM_SUBDIRS
        .iter()
        .any(|subdir| include_path.starts_with(subdir))
    {
        return true;
    }

    AMBIGUOUS_INCLUDE_PREFIXES.iter().any(|prefix| {
        include_path.starts_with(prefix) && include_path_looks_like_fastled_code(include_path)
    })
}

fn banned_subpath_replacement(include_path: &str) -> Option<String> {
    BANNED_INTERNAL_SUBPATHS
        .iter()
        .find_map(|(banned_prefix, replacement_prefix)| {
            include_path
                .strip_prefix(banned_prefix)
                .map(|suffix| format!("{replacement_prefix}{suffix}"))
        })
}

fn is_valid_include_path(include_path: &str) -> bool {
    if is_top_level_include(include_path) || is_external_sdk_header(include_path) {
        return true;
    }
    if banned_subpath_replacement(include_path).is_some() {
        return false;
    }
    VALID_INCLUDE_PREFIXES
        .iter()
        .any(|prefix| include_path.starts_with(prefix))
}

fn is_relative_include_path(include_path: &str) -> bool {
    include_path.starts_with("./") || include_path.starts_with("../")
}

fn header_exists_for_file(file_path: &str, include_path: &str) -> bool {
    if is_relative_include_path(include_path) || is_external_sdk_header(include_path) {
        return true;
    }

    let normalized_path = normalize_path(file_path);
    let project_root = if let Some(index) = normalized_path.find("/src/") {
        PathBuf::from(&normalized_path[..index])
    } else {
        PathBuf::from(".")
    };

    project_root.join("src").join(include_path).exists()
}

fn typo_include_suggestion(include_path: &str) -> Option<&'static str> {
    TYPO_INCLUDE_PREFIXES
        .iter()
        .find_map(|(typo, correct)| include_path.starts_with(typo).then_some(*correct))
}

fn standard_attribute_replacement(attribute: &str) -> Option<&'static str> {
    ATTRIBUTE_MAPPINGS
        .iter()
        .find_map(|(name, replacement)| (*name == attribute).then_some(*replacement))
}

fn numeric_limit_suggestion(macro_name: &str) -> &'static str {
    match macro_name {
        "UINT8_MAX" => "fl::numeric_limits<uint8_t>::max()",
        "UINT16_MAX" => "fl::numeric_limits<uint16_t>::max()",
        "UINT32_MAX" => "fl::numeric_limits<uint32_t>::max()",
        "UINT64_MAX" => "fl::numeric_limits<uint64_t>::max()",
        "INT8_MAX" => "fl::numeric_limits<int8_t>::max()",
        "INT8_MIN" => "fl::numeric_limits<int8_t>::min()",
        "INT16_MAX" => "fl::numeric_limits<int16_t>::max()",
        "INT16_MIN" => "fl::numeric_limits<int16_t>::min()",
        "INT32_MAX" => "fl::numeric_limits<int32_t>::max()",
        "INT32_MIN" => "fl::numeric_limits<int32_t>::min()",
        "INT64_MAX" => "fl::numeric_limits<int64_t>::max()",
        "INT64_MIN" => "fl::numeric_limits<int64_t>::min()",
        "SIZE_MAX" => "fl::numeric_limits<size_t>::max()",
        "UINTMAX_MAX" => "fl::numeric_limits<uintmax_t>::max()",
        "UINTPTR_MAX" => "fl::numeric_limits<uintptr_t>::max()",
        "INTMAX_MAX" => "fl::numeric_limits<intmax_t>::max()",
        "INTMAX_MIN" => "fl::numeric_limits<intmax_t>::min()",
        "INTPTR_MAX" => "fl::numeric_limits<intptr_t>::max()",
        "INTPTR_MIN" => "fl::numeric_limits<intptr_t>::min()",
        "UCHAR_MAX" => "fl::numeric_limits<unsigned char>::max()",
        "USHRT_MAX" => "fl::numeric_limits<unsigned short>::max()",
        "UINT_MAX" => "fl::numeric_limits<unsigned int>::max()",
        "ULONG_MAX" => "fl::numeric_limits<unsigned long>::max()",
        "ULLONG_MAX" => "fl::numeric_limits<unsigned long long>::max()",
        "CHAR_MAX" => "fl::numeric_limits<char>::max()",
        "CHAR_MIN" => "fl::numeric_limits<char>::min()",
        "SHRT_MAX" => "fl::numeric_limits<short>::max()",
        "SHRT_MIN" => "fl::numeric_limits<short>::min()",
        "INT_MAX" => "fl::numeric_limits<int>::max()",
        "INT_MIN" => "fl::numeric_limits<int>::min()",
        "LONG_MAX" => "fl::numeric_limits<long>::max()",
        "LONG_MIN" => "fl::numeric_limits<long>::min()",
        "LLONG_MAX" => "fl::numeric_limits<long long>::max()",
        "LLONG_MIN" => "fl::numeric_limits<long long>::min()",
        _ => "fl::numeric_limits<T>::max/min()",
    }
}

fn is_std_bridge_file(file_path: &str) -> bool {
    let normalized = normalize_path(file_path);
    STD_BRIDGE_FILE_WHITELIST
        .iter()
        .any(|bridge_path| normalized.ends_with(bridge_path))
}

fn line_has_only_allowed_std_symbols(line: &str) -> bool {
    let code_part = split_line_comment(line);
    if !code_part.contains("std::") {
        return true;
    }

    regex_std_usage().find_iter(code_part).all(|usage| {
        ALLOWED_STD_SYMBOLS
            .iter()
            .any(|allowed| *allowed == usage.as_str())
    })
}

fn serial_replacement(method: &str) -> &'static str {
    match method {
        "print" => "fl::print(...) or fl::cout << ...",
        "println" => "fl::println(...) or fl::cout << ... << fl::endl",
        "printf" => "fl::cout << ... (build typed values) or fl::println(formatted)",
        "write" => "fl::write_bytes(buf, size)",
        "read" => "fl::read()",
        "available" => "fl::available()",
        "peek" => "fl::peek()",
        "readStringUntil" => "fl::readLine(delim) (returns fl::optional<fl::string>)",
        "flush" => "fl::flush(timeoutMs)",
        "begin" => "fl::serial_begin(baudRate)",
        _ => "an fl:: variant",
    }
}

fn strip_inline_block_comments(line: &str) -> String {
    let mut result = String::new();
    let mut rest = line;
    while let Some(start) = rest.find("/*") {
        result.push_str(&rest[..start]);
        let after_start = &rest[start + 2..];
        let Some(end) = after_start.find("*/") else {
            result.push_str(&rest[start..]);
            return result;
        };
        rest = &after_start[end + 2..];
    }
    result.push_str(rest);
    result
}

fn strip_comments_preserving_lines(lines: &[String]) -> String {
    let mut in_block_comment = false;
    lines
        .iter()
        .map(|line| {
            let visible = strip_block_comments_from_line(line, &mut in_block_comment);
            split_line_comment(&visible).to_string()
        })
        .collect::<Vec<_>>()
        .join("\n")
}

fn is_ascii_word_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn contains_ascii_word(code: &str, word: &str) -> bool {
    let bytes = code.as_bytes();
    let mut search_start = 0;
    while let Some(relative_pos) = code[search_start..].find(word) {
        let pos = search_start + relative_pos;
        let end = pos + word.len();
        search_start = end;
        if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1]))
            && (end >= bytes.len() || !is_ascii_word_byte(bytes[end]))
        {
            return true;
        }
    }
    false
}

fn banned_header_recommendation(header: &str) -> &'static str {
    BANNED_HEADER_RECOMMENDATIONS
        .iter()
        .find_map(|(candidate, recommendation)| (*candidate == header).then_some(*recommendation))
        .unwrap_or("Use fl/ alternatives instead of standard library headers")
}

fn private_libcpp_header_recommendation(header: &str) -> &'static str {
    if let Some((_, recommendation)) = PRIVATE_LIBCPP_HEADER_MAPPINGS_RS
        .iter()
        .find(|(candidate, _)| *candidate == header)
    {
        return *recommendation;
    }
    PRIVATE_LIBCPP_HEADER_MAPPINGS_RS
        .iter()
        .find_map(|(prefix, recommendation)| header.starts_with(prefix).then_some(*recommendation))
        .unwrap_or("\"appropriate FastLED header\" (check ci/iwyu/stdlib.imp)")
}

fn banned_header_matches_exception(file_path: &str, header: &str) -> bool {
    let normalized = normalize_path(file_path);
    BANNED_HEADER_EXCEPTIONS
        .iter()
        .filter(|(exception_header, _)| *exception_header == header)
        .any(|(_, pattern)| normalized == *pattern || normalized.ends_with(&format!("/{pattern}")))
}

fn last_dot_extension(path: &str) -> &str {
    path.rsplit('.').next().unwrap_or(path)
}

fn namespace_include_snippet(line: &str) -> String {
    let text = line.trim();
    if text.chars().count() <= 50 {
        return text.to_string();
    }
    text.chars().take(47).collect::<String>() + "..."
}

fn namespace_include_message(
    include_snippet: &str,
    namespace_info: Option<(usize, &str)>,
) -> String {
    if let Some((line, snippet)) = namespace_info {
        format!("{include_snippet} (namespace declared at line {line}: {snippet})")
    } else {
        include_snippet.to_string()
    }
}

fn namespace_braces_are_balanced(lines: &[String]) -> bool {
    let stripped = strip_comments_preserving_lines(lines);
    stripped.matches('{').count() == stripped.matches('}').count()
}

fn namespace_include_thorough_violations(lines: &[String]) -> Vec<(usize, String)> {
    let stripped_content = strip_comments_preserving_lines(lines);
    let stripped_lines: Vec<&str> = stripped_content.split('\n').collect();
    let mut violations = Vec::new();
    let mut namespace_stack: Vec<(usize, String)> = Vec::new();
    let mut last_namespace: Option<(usize, String)> = None;

    for (index, original_line) in lines.iter().enumerate() {
        let line_number = index + 1;
        let line_no_comments = stripped_lines.get(index).copied().unwrap_or("");
        let stripped = line_no_comments.trim();
        if stripped.is_empty() {
            continue;
        }

        let open_braces = line_no_comments.matches('{').count();
        let close_braces = line_no_comments.matches('}').count();

        if stripped.contains("using namespace")
            && regex_namespace_include_using().is_match(stripped)
        {
            let info = (line_number, namespace_include_snippet(original_line));
            last_namespace = Some(info.clone());
            if namespace_stack.is_empty() {
                namespace_stack.push(info);
            }
        }

        if stripped.contains("namespace")
            && stripped.contains('{')
            && regex_namespace_include_open().is_match(stripped)
        {
            let info = (line_number, namespace_include_snippet(original_line));
            last_namespace = Some(info.clone());
            namespace_stack.push(info);
        }

        if stripped.contains("#include")
            && regex_namespace_include_directive().is_match(stripped)
            && !namespace_stack.is_empty()
        {
            // Includes inside namespace blocks are handled by IncludeAfterNamespaceChecker.
        } else if stripped.contains("#include")
            && regex_namespace_include_directive().is_match(stripped)
            && !original_line.contains("// nolint")
            && last_namespace.is_some()
        {
            let include_snippet = namespace_include_snippet(original_line);
            let namespace_info = last_namespace
                .as_ref()
                .map(|(namespace_line, snippet)| (*namespace_line, snippet.as_str()));
            violations.push((
                line_number,
                namespace_include_message(&include_snippet, namespace_info),
            ));
        }

        if close_braces > open_braces {
            for _ in 0..(close_braces - open_braces) {
                if !namespace_stack.is_empty() {
                    namespace_stack.pop();
                }
            }
        }
    }

    violations
}

fn ctype_functions() -> impl Iterator<Item = &'static str> {
    CTYPE_FUNCTIONS
        .iter()
        .chain(CSTRING_FUNCTIONS.iter())
        .copied()
}

fn ctype_header(func: &str) -> &'static str {
    if CTYPE_FUNCTIONS.contains(&func) {
        "fl/stl/cctype.h"
    } else {
        "fl/stl/cstring.h"
    }
}

fn find_ctype_calls(code_part: &str) -> Vec<(&'static str, bool)> {
    let bytes = code_part.as_bytes();
    let mut calls = Vec::new();

    for func in ctype_functions() {
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(func) {
            let pos = search_start + relative_pos;
            let end = pos + func.len();
            search_start = end;

            if pos > 0 && is_ascii_word_byte(bytes[pos - 1]) {
                continue;
            }
            if end < bytes.len() && is_ascii_word_byte(bytes[end]) {
                continue;
            }

            let mut call_pos = end;
            while call_pos < bytes.len() && bytes[call_pos].is_ascii_whitespace() {
                call_pos += 1;
            }
            if call_pos >= bytes.len() || bytes[call_pos] != b'(' {
                continue;
            }

            let prefix = &code_part[..pos];
            if prefix.ends_with("fl::") {
                continue;
            }
            let is_global_qualified = pos >= 2
                && &bytes[pos - 2..pos] == b"::"
                && (pos == 2 || !is_ascii_word_byte(bytes[pos - 3]));
            calls.push((func, is_global_qualified));
        }
    }

    calls
}

fn find_stdint_matches(code_part: &str) -> Vec<&'static str> {
    let bytes = code_part.as_bytes();
    let mut matches = Vec::new();

    for (type_name, _) in STDINT_TYPE_MAPPINGS {
        if *type_name == "unsigned int" || *type_name == "signed int" {
            continue;
        }
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(type_name) {
            let pos = search_start + relative_pos;
            let end = pos + type_name.len();
            search_start = end;

            if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1]))
                && (end >= bytes.len() || !is_ascii_word_byte(bytes[end]))
            {
                matches.push(*type_name);
            }
        }
    }

    for type_name in ["unsigned int", "signed int"] {
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(type_name) {
            let pos = search_start + relative_pos;
            let end = pos + type_name.len();
            search_start = end;

            let Some(next) = bytes.get(end).copied() else {
                continue;
            };
            if !matches!(next, b' ' | b'*' | b'&' | b',' | b')' | b']') {
                continue;
            }
            if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1])) && !is_ascii_word_byte(next) {
                matches.push(type_name);
            }
        }
    }

    matches
}

fn stdint_fl_type(type_name: &str) -> &'static str {
    STDINT_TYPE_MAPPINGS
        .iter()
        .find_map(|(name, replacement)| (*name == type_name).then_some(*replacement))
        .unwrap_or("u32")
}

fn unit_test_fl_macro(macro_name: &str) -> Option<&'static str> {
    UNIT_TEST_BANNED_MACROS
        .iter()
        .find_map(|(bare, replacement)| (*bare == macro_name).then_some(*replacement))
}

fn project_relative_guess(path: &str) -> String {
    let normalized = normalize_path(path);
    for marker in ["/src/", "/tests/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return normalized[index + 1..].to_string();
        }
    }
    for prefix in ["src/", "tests/", "examples/"] {
        if normalized.starts_with(prefix) {
            return normalized;
        }
    }
    normalized
}

fn project_root_prefix_for_file(path: &str) -> String {
    let normalized = normalize_path(path);
    for marker in ["/tests/", "/src/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return normalized[..index].to_string();
        }
    }
    String::new()
}

fn join_project_path(root_prefix: &str, rel_path: &str) -> PathBuf {
    if root_prefix.is_empty() {
        PathBuf::from(rel_path)
    } else {
        PathBuf::from(format!(
            "{root_prefix}/{}",
            rel_path.trim_start_matches('/')
        ))
    }
}

fn project_relative_path(path: &str) -> Option<String> {
    let normalized = normalize_path(path);
    for marker in ["/tests/", "/src/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return Some(normalized[index + 1..].to_string());
        }
    }
    for prefix in ["tests/", "src/", "examples/"] {
        if normalized.starts_with(prefix) {
            return Some(normalized);
        }
    }
    None
}

fn tests_relative_path(path: &str) -> Option<String> {
    let normalized = normalize_path(path);
    if let Some(index) = normalized.find("/tests/") {
        return Some(normalized[index + "/tests/".len()..].to_string());
    }
    normalized
        .strip_prefix("tests/")
        .map(|value| value.to_string())
}

fn path_without_extension(path: &str) -> String {
    for suffix in [".cpp.hpp", ".cpp", ".hpp", ".h"] {
        if let Some(stripped) = path.strip_suffix(suffix) {
            return stripped.to_string();
        }
    }
    path.to_string()
}

fn python_path_display(rel_path: &str) -> String {
    if cfg!(windows) {
        rel_path.replace('/', "\\")
    } else {
        rel_path.to_string()
    }
}

fn is_under_config_excluded_test_dir(path: &str) -> bool {
    let Some(project_rel) = project_relative_path(path) else {
        return false;
    };
    TEST_CONFIG_EXCLUDED_DIRS
        .iter()
        .any(|dir| project_rel == *dir || project_rel.strip_prefix(&format!("{dir}/")).is_some())
}

fn top_level_headers(root_prefix: &str, dir: &str) -> HashSet<String> {
    let mut headers = HashSet::new();
    let root = join_project_path(root_prefix, dir);
    let Ok(entries) = fs::read_dir(root) else {
        return headers;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if !path.is_file() {
            continue;
        }
        let name = path
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("");
        if name.ends_with(".h") || name.ends_with(".hpp") {
            headers.insert(name.to_string());
        }
    }
    headers
}

fn all_test_header_filenames(root_prefix: &str) -> HashSet<String> {
    let root = join_project_path(root_prefix, "tests");
    let mut filenames = HashSet::new();
    if !root.exists() {
        return filenames;
    }
    for entry in WalkDir::new(root).into_iter().filter_map(Result::ok) {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        let path_str = normalize_path(&path_to_string(path));
        if !ends_with_any(&path_str, &[".h", ".hpp", ".cpp.hpp"]) {
            continue;
        }
        if let Some(name) = path.file_name().and_then(|value| value.to_str()) {
            filenames.insert(name.to_string());
        }
    }
    filenames
}

fn source_mirror_dir_has_headers(root_prefix: &str, test_dir_path: &str) -> bool {
    let src_dir = join_project_path(root_prefix, &format!("src/{test_dir_path}"));
    let Ok(entries) = fs::read_dir(src_dir) else {
        return false;
    };
    entries.flatten().any(|entry| {
        let path = entry.path();
        path.is_file()
            && path
                .file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|name| {
                    name.ends_with(".h") || name.ends_with(".hpp") || name.ends_with(".cpp.hpp")
                })
    })
}

fn parse_aggregator_includes(path: &Path) -> BTreeSet<String> {
    let mut includes = BTreeSet::new();
    let Ok(content) = fs::read_to_string(path) else {
        return includes;
    };
    for line in content.lines() {
        if let Some(capture) = regex_quoted_include_line().captures(line) {
            includes.insert(capture[1].to_string());
        }
    }
    includes
}

fn test_aggregator_rel_for_dir(excluded_dir_rel: &str) -> String {
    format!("{excluded_dir_rel}.cpp")
}

fn resolve_test_include(root_prefix: &str, aggregator_rel: &str, include_path: &str) -> String {
    let aggregator_parent = aggregator_rel
        .rsplit_once('/')
        .map_or("", |(parent, _)| parent);
    let from_aggregator = if aggregator_parent.is_empty() {
        include_path.to_string()
    } else {
        format!("{aggregator_parent}/{include_path}")
    };
    let candidate = join_project_path(root_prefix, &from_aggregator);
    if candidate.exists() {
        return normalize_path(&path_to_string(&candidate));
    }
    normalize_path(&path_to_string(&join_project_path(
        root_prefix,
        include_path,
    )))
}

fn collect_test_aggregation_included_files(
    root_prefix: &str,
    excluded_dir_rel: &str,
) -> (Option<String>, BTreeSet<String>) {
    let direct_aggregator_rel = test_aggregator_rel_for_dir(excluded_dir_rel);
    let primary_aggregator = join_project_path(root_prefix, &direct_aggregator_rel)
        .exists()
        .then_some(direct_aggregator_rel.clone());
    let mut included_files = BTreeSet::new();
    let mut check_dir = excluded_dir_rel.to_string();

    while check_dir.starts_with("tests/") && check_dir != "tests" {
        let aggregator_rel = test_aggregator_rel_for_dir(&check_dir);
        let aggregator = join_project_path(root_prefix, &aggregator_rel);
        if aggregator.exists() {
            for include in parse_aggregator_includes(&aggregator) {
                let resolved = resolve_test_include(root_prefix, &aggregator_rel, &include);
                if Path::new(&resolved).exists() {
                    included_files.insert(resolved);
                }
            }
        }
        let Some((parent, _)) = check_dir.rsplit_once('/') else {
            break;
        };
        check_dir = parent.to_string();
    }

    (primary_aggregator, included_files)
}

fn test_aggregation_check_single_file(file_path: &str) -> Vec<String> {
    let normalized = normalize_path(file_path);
    let root_prefix = project_root_prefix_for_file(&normalized);
    let Some(project_rel) = project_relative_path(&normalized) else {
        return Vec::new();
    };
    let resolved = normalize_path(&path_to_string(&join_project_path(
        &root_prefix,
        &project_rel,
    )));
    let mut violations = Vec::new();

    for excluded_dir in TEST_AGGREGATED_DIRS {
        let aggregator_rel = test_aggregator_rel_for_dir(excluded_dir);
        let aggregator_path = join_project_path(&root_prefix, &aggregator_rel);
        let aggregator_norm = normalize_path(&path_to_string(&aggregator_path));

        if aggregator_path.exists() && aggregator_norm == resolved {
            let (_, included_files) =
                collect_test_aggregation_included_files(&root_prefix, excluded_dir);
            let excluded_path = join_project_path(&root_prefix, excluded_dir);
            if excluded_path.exists() {
                for entry in WalkDir::new(&excluded_path)
                    .into_iter()
                    .filter_map(Result::ok)
                {
                    if !entry.file_type().is_file() {
                        continue;
                    }
                    let hpp_path = normalize_path(&path_to_string(entry.path()));
                    if !hpp_path.ends_with(".hpp") || included_files.contains(&hpp_path) {
                        continue;
                    }
                    let rel_file = project_relative_path(&hpp_path).unwrap_or(hpp_path);
                    violations.push(format!("{}: orphaned {}", aggregator_rel, rel_file));
                }
            }
            for include in parse_aggregator_includes(&aggregator_path) {
                let inc_resolved = resolve_test_include(&root_prefix, &aggregator_rel, &include);
                if project_relative_path(&inc_resolved)
                    .is_some_and(|rel| rel.starts_with(&format!("{excluded_dir}/")))
                    && include.ends_with(".cpp")
                {
                    violations.push(format!(
                        "{}: #include \"{include}\" should use .hpp",
                        aggregator_rel
                    ));
                }
            }
            return violations;
        }

        if project_rel == *excluded_dir
            || project_rel
                .strip_prefix(&format!("{excluded_dir}/"))
                .is_some()
        {
            if !project_rel.ends_with(".hpp") {
                continue;
            }
            let (aggregator, included_files) =
                collect_test_aggregation_included_files(&root_prefix, excluded_dir);
            let Some(aggregator_rel) = aggregator else {
                violations.push(format!("Missing aggregator: {}.cpp", excluded_dir));
                return violations;
            };
            if !included_files.contains(&resolved) {
                violations.push(format!("{}: orphaned {}", aggregator_rel, project_rel));
            }
            return violations;
        }
    }

    violations
}

fn required_fl_is_header(fl_is_macro: &str) -> Option<&'static str> {
    FL_IS_PREFIX_TO_HEADER
        .iter()
        .find_map(|(prefix, header)| fl_is_macro.starts_with(prefix).then_some(*header))
}

fn iwyu_is_top_level_platform_header(header_path: &str) -> bool {
    let Some(remainder) = header_path.strip_prefix("platforms/") else {
        return false;
    };
    !remainder.contains('/')
}

fn iwyu_classify_header(header_path: &str, include_line: &str) -> &'static str {
    if include_line.contains('<') && include_line.contains('>') {
        return "system";
    }
    if IWYU_INTERNAL_HEADER_PREFIXES
        .iter()
        .any(|prefix| header_path.starts_with(prefix))
    {
        return "internal";
    }
    if !header_path.contains('/') && ends_with_any(header_path, &[".h", ".hpp"]) {
        return "internal";
    }
    if header_path.starts_with("platforms/") {
        return "platform";
    }
    "system"
}

fn iwyu_format_violation(_header_path: &str, header_type: &str, include_line: &str) -> String {
    let type_desc = if header_type == "platform" {
        "Platform"
    } else {
        "System"
    };
    format!(
        "{type_desc} header not wrapped in IWYU pragma block: {include_line}\n  Wrap in:\n    // IWYU pragma: begin_keep\n    {include_line}\n    // IWYU pragma: end_keep\n  Or add inline pragma:\n    {include_line}  // IWYU pragma: keep"
    )
}

fn python_capitalize(part: &str) -> String {
    let mut chars = part.chars();
    let Some(first) = chars.next() else {
        return String::new();
    };
    let mut result = first.to_uppercase().collect::<String>();
    result.push_str(&chars.as_str().to_lowercase());
    result
}

fn convert_google_to_m_prefix(var_name: &str) -> String {
    let Some(base_name) = var_name.strip_suffix('_') else {
        return var_name.to_string();
    };
    let mut base_chars = base_name.chars();
    let Some(first) = base_chars.next() else {
        return "m".to_string();
    };
    if base_chars.as_str().chars().any(char::is_uppercase) {
        let first_upper = first.to_uppercase().collect::<String>();
        return format!("m{first_upper}{}", base_chars.as_str());
    }
    let converted = base_name
        .split('_')
        .filter(|part| !part.is_empty())
        .map(python_capitalize)
        .collect::<String>();
    format!("m{converted}")
}

fn convert_m_underscore_to_m_prefix(var_name: &str) -> String {
    let Some(remainder) = var_name.strip_prefix("m_") else {
        return var_name.to_string();
    };
    if remainder.is_empty() {
        return var_name.to_string();
    }
    if remainder.contains('_') {
        let converted = remainder
            .split('_')
            .filter(|part| !part.is_empty())
            .map(python_capitalize)
            .collect::<String>();
        return format!("m{converted}");
    }
    let mut chars = remainder.chars();
    let Some(first) = chars.next() else {
        return "m".to_string();
    };
    let first_upper = first.to_uppercase().collect::<String>();
    format!("m{first_upper}{}", chars.as_str())
}

fn is_inside_parens(code_part: &str, pos: usize) -> bool {
    let mut depth = 0_i32;
    for ch in code_part[..pos].chars() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
        }
    }
    depth > 0
}

fn find_close_paren(text: &str, open_pos: usize) -> Option<usize> {
    let mut depth = 1_i32;
    for (offset, ch) in text[open_pos + 1..].char_indices() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
            if depth == 0 {
                return Some(open_pos + 1 + offset);
            }
        }
    }
    None
}

fn find_close_paren_multiline(
    lines: &[String],
    start_idx: usize,
    open_col: usize,
) -> Option<(usize, usize)> {
    let line = &lines[start_idx];
    let mut depth = 1_i32;
    for (offset, ch) in line[open_col + 1..].char_indices() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
            if depth == 0 {
                return Some((start_idx, open_col + 1 + offset));
            }
        }
    }
    for lookahead in 1..15 {
        let idx = start_idx + lookahead;
        if idx >= lines.len() {
            break;
        }
        for (col, ch) in lines[idx].char_indices() {
            if ch == '(' {
                depth += 1;
            } else if ch == ')' {
                depth -= 1;
                if depth == 0 {
                    return Some((idx, col));
                }
            }
        }
    }
    None
}

fn compute_block_comment_mask(lines: &[String]) -> Vec<bool> {
    let mut mask = Vec::with_capacity(lines.len());
    let mut inside = false;
    for line in lines {
        let line_inside = inside;
        let bytes = line.as_bytes();
        let mut index = 0;
        while index + 1 < bytes.len() {
            if inside {
                if bytes[index] == b'*' && bytes[index + 1] == b'/' {
                    inside = false;
                    index += 2;
                    continue;
                }
            } else if bytes[index] == b'/' && bytes[index + 1] == b'*' {
                inside = true;
                index += 2;
                continue;
            } else if bytes[index] == b'/' && bytes[index + 1] == b'/' {
                break;
            }
            index += 1;
        }
        mask.push(line_inside);
    }
    mask
}

fn strip_line_comment_code(line: &str) -> String {
    let mut in_string = false;
    let mut in_char = false;
    let mut previous = '\0';
    let chars: Vec<(usize, char)> = line.char_indices().collect();
    for (idx, ch) in &chars {
        if *ch == '"' && !in_char && previous != '\\' {
            in_string = !in_string;
        } else if *ch == '\'' && !in_string && previous != '\\' {
            in_char = !in_char;
        } else if !in_string && !in_char && *ch == '/' {
            let next_is_slash = line[*idx + ch.len_utf8()..].starts_with('/');
            if next_is_slash {
                return line[..*idx].to_string();
            }
        }
        previous = *ch;
    }
    line.to_string()
}

fn classify_noexcept_line(
    line: &str,
    class_names: &HashSet<String>,
) -> Option<(&'static str, usize)> {
    let code = strip_line_comment_code(line);
    let stripped = code.trim();
    if stripped.is_empty() || stripped.starts_with('#') {
        return None;
    }

    if let Some(matched) = regex_destructor_decl().find(&code) {
        let tilde_pos = matched.start();
        let prefix = code[..tilde_pos].trim_end();
        if !prefix.ends_with("->") && !prefix.ends_with('.') {
            let paren = code[matched.start()..].find('(')? + matched.start();
            let close = find_close_paren(&code, paren)?;
            let inside = code[paren + 1..close].trim();
            if inside.is_empty() || inside == "void" {
                return Some(("destructor", paren));
            }
        }
    }

    if let Some(matched) = regex_operator_assign_decl().find(&code) {
        let paren = code[matched.start()..].find('(')? + matched.start();
        return Some(("assignment operator", paren));
    }

    for name in class_names {
        let pattern = format!(r"\b{}\s*\(", regex::escape(name));
        let Ok(regex) = Regex::new(&pattern) else {
            continue;
        };
        for matched in regex.find_iter(&code) {
            let prefix = code[..matched.start()].trim();
            if !NOEXCEPT_CTOR_QUALS.contains(&prefix) {
                continue;
            }
            let paren = matched.end() - 1;
            let rest = code[paren + 1..].trim_start();
            let copy_prefix = format!("const {name}");
            if rest.starts_with(&copy_prefix) {
                let after = rest[copy_prefix.len()..].trim_start();
                if after.starts_with('&') {
                    return Some(("copy constructor", paren));
                }
            }
            if rest.starts_with(name) {
                let after = rest[name.len()..].trim_start();
                if after.starts_with("&&") {
                    return Some(("move constructor", paren));
                }
            }
            if rest.starts_with(')')
                || rest.starts_with("void") && rest[4..].trim_start().starts_with(')')
            {
                return Some(("default constructor", paren));
            }
        }
    }
    None
}

fn join_multiline_signature(lines: &[String], start: usize) -> Option<String> {
    let line = strip_line_comment_code(&lines[start]);
    if !line.contains('(') {
        return None;
    }
    let mut depth = 0_i32;
    for ch in line.chars() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
        }
    }
    if depth <= 0 {
        return None;
    }
    let mut joined = line;
    for offset in 1..10 {
        let idx = start + offset;
        if idx >= lines.len() {
            break;
        }
        let next = strip_line_comment_code(&lines[idx]).trim().to_string();
        joined.push(' ');
        joined.push_str(&next);
        for ch in next.chars() {
            if ch == '(' {
                depth += 1;
            } else if ch == ')' {
                depth -= 1;
            }
        }
        if depth <= 0 {
            return Some(joined);
        }
    }
    None
}

fn signature_has_noexcept(lines: &[String], start: usize, open_paren: usize) -> bool {
    let Some((close_line, close_col)) = find_close_paren_multiline(lines, start, open_paren) else {
        return false;
    };
    let tail = &lines[close_line][close_col + 1..];
    if regex_has_noexcept_special().is_match(tail) {
        return true;
    }
    for offset in 1..6 {
        let idx = close_line + offset;
        if idx >= lines.len() {
            break;
        }
        let next = lines[idx].trim();
        if regex_has_noexcept_special().is_match(next) {
            return true;
        }
        if next.starts_with('{') || next.ends_with('{') || next == "};" {
            break;
        }
        if next.starts_with(':') && !next.starts_with("::") {
            break;
        }
        if next.ends_with(';') {
            break;
        }
    }
    false
}

fn platform_trampoline_suggestion(include_path: &str) -> &'static str {
    DEEP_PLATFORM_REPLACEMENTS
        .iter()
        .find_map(|(pattern, replacement)| include_path.contains(pattern).then_some(*replacement))
        .unwrap_or("platforms/*.h (appropriate trampoline)")
}

struct SerialPrintfChecker;

impl FileContentChecker for SerialPrintfChecker {
    fn name(&self) -> &'static str {
        "SerialPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            let code_part = split_line_comment(line);
            if code_part.contains("Serial.printf") {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct UsingNamespaceFlInExamplesChecker;

impl FileContentChecker for UsingNamespaceFlInExamplesChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlInExamplesChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(
                file_path,
                &[".cpp", ".h", ".hpp", ".ino", ".c", ".cc", ".cxx"],
            )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        file_content
            .lines
            .iter()
            .enumerate()
            .filter_map(|(index, line)| {
                if regex_using_namespace_fl().is_match(line) {
                    Some((index + 1, line.trim_end().to_string()))
                } else {
                    None
                }
            })
            .collect()
    }
}

struct BannedMacrosChecker;

impl FileContentChecker for BannedMacrosChecker {
    fn name(&self) -> &'static str {
        "BannedMacrosChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("has_include.h")
            || file_path.ends_with("compiler_control.h")
            || file_path.ends_with("cpp_compat.h")
            || file_path.ends_with("static_assert.h")
        {
            return false;
        }
        if is_under_dir(file_path, "third_party") {
            return false;
        }
        if ends_with_any(file_path, &["doctest.h", "catch.hpp", "gtest.h"]) {
            return false;
        }
        if ends_with_any(file_path, &[".md", ".txt"]) {
            return false;
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_HAS_INCLUDE") {
                continue;
            }

            let code_no_strings = strip_string_literals(code_part);
            if !code_no_strings.contains("__has_include")
                && !code_no_strings.contains("static_assert")
            {
                continue;
            }

            if regex_has_include().is_match(&code_no_strings) {
                if code_no_strings.contains("#ifndef") || code_no_strings.contains("#ifdef") {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_HAS_INCLUDE instead of __has_include: {stripped}\n      \
Rationale: __has_include is not universally supported. FL_HAS_INCLUDE provides a portable wrapper with fallback for older compilers.\n      \
Include 'fl/stl/has_include.h' and replace __has_include(...) with FL_HAS_INCLUDE(...)"
                    ),
                ));
            }

            if regex_static_assert().is_match(&code_no_strings) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_STATIC_ASSERT instead of raw static_assert: {stripped}\n      \
Rationale: FL_STATIC_ASSERT keeps compile-time assertions portable on old embedded toolchains.\n      \
Include 'fl/stl/static_assert.h' when needed and replace static_assert(...) with FL_STATIC_ASSERT(...)"
                    ),
                ));
            }
        }

        violations
    }
}

struct BareAllocationChecker;

impl FileContentChecker for BareAllocationChecker {
    fn name(&self) -> &'static str {
        "BareAllocationChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let lower = file_path.to_ascii_lowercase();
        if lower.contains("third_party") || lower.contains("thirdparty") {
            return false;
        }
        !BARE_ALLOCATION_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| file_path.ends_with(suffix))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            if line.contains("// ok bare allocation") || line.contains("// okay bare allocation") {
                continue;
            }
            if !line.contains("new")
                && !line.contains("delete")
                && !line.contains("malloc")
                && !line.contains("calloc")
                && !line.contains("realloc")
                && !line.contains("free")
            {
                continue;
            }

            let code_part = split_line_comment(line);
            let code_part = regex_string_literal().replace_all(code_part, "\"\"");

            if code_part.contains("operator new") || code_part.contains("operator delete") {
                continue;
            }
            if regex_deleted_func().is_match(&code_part)
                || regex_deleted_func_eol().is_match(code_part.trim_end())
            {
                continue;
            }

            if regex_new_alloc().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'new': {stripped} — {}", suggestion_new()),
                ));
                continue;
            }

            if regex_delete().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'delete': {stripped} — {}", suggestion_delete()),
                ));
                continue;
            }

            if let Some(mat) = regex_malloc_family()
                .find_iter(&code_part)
                .find(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                let func = regex_malloc_family()
                    .captures(mat.as_str())
                    .and_then(|captures| captures.get(1))
                    .map(|capture| capture.as_str())
                    .unwrap_or("malloc");
                violations.push((
                    index + 1,
                    format!("bare '{func}': {stripped} — {}", suggestion_malloc()),
                ));
                continue;
            }

            if regex_free()
                .find_iter(&code_part)
                .any(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                violations.push((
                    index + 1,
                    format!("bare 'free': {stripped} — {}", suggestion_free()),
                ));
            }
        }

        violations
    }
}

fn has_forbidden_prefix(code: &str, start: usize) -> bool {
    if start == 0 {
        return false;
    }
    let prefix = &code[..start];
    prefix.ends_with("::") || prefix.ends_with('.')
}

fn suggestion_new() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead of bare 'new', or add '// ok bare allocation' to suppress"
}

fn suggestion_delete() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead of bare 'delete', or add '// ok bare allocation' to suppress"
}

fn suggestion_malloc() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead, or fl::malloc() if raw memory is required, or add '// ok bare allocation' to suppress"
}

fn suggestion_free() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead, or fl::free() if raw memory is required, or add '// ok bare allocation' to suppress"
}

struct StaticInHeaderChecker;

impl FileContentChecker for StaticInHeaderChecker {
    fn name(&self) -> &'static str {
        "StaticInHeaderChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        if file_path.ends_with(".cpp.hpp") {
            return false;
        }
        !is_excluded_file(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("static") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut brace_depth: isize = 0;
        let mut in_function = false;
        let mut in_template_function = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            let open_braces = code_part.matches('{').count() as isize;
            let close_braces = code_part.matches('}').count() as isize;

            if code_part.contains("template") && regex_template().is_match(code_part) {
                in_template_function = true;
            }

            if code_part.contains('(')
                && code_part.contains('{')
                && regex_inline_func().is_match(code_part)
            {
                in_function = true;
                brace_depth = open_braces - close_braces;
            } else if in_function {
                brace_depth += open_braces - close_braces;
            }

            if in_function && brace_depth <= 0 {
                in_function = false;
                in_template_function = false;
                brace_depth = 0;
            }

            if in_function
                && brace_depth > 0
                && !in_template_function
                && code_part.contains("static")
                && regex_static_var().is_match(code_part)
                && !regex_static_func().is_match(code_part)
                && !line.contains("// okay static in header")
            {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct IncludePathsChecker;

impl FileContentChecker for IncludePathsChecker {
    fn name(&self) -> &'static str {
        "IncludePathsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".c"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }

        is_under_project_subpath(file_path, project_root, "src/fl")
            || is_under_project_subpath(file_path, project_root, "src/platforms")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if comment_part
                .to_ascii_lowercase()
                .contains("ok include path")
            {
                continue;
            }

            let Some(captures) = regex_include_path().captures(code_part) else {
                continue;
            };
            let include_path = captures.get(1).map(|match_| match_.as_str()).unwrap_or("");

            if include_path.starts_with('<') {
                continue;
            }

            if is_valid_include_path(include_path)
                && !header_exists_for_file(&file_content.path, include_path)
            {
                violations.push((
                    index + 1,
                    format!(
                        "Header not found: #include \"{include_path}\"\n  Expected: src/{include_path}\n"
                    ),
                ));
                continue;
            }

            if is_valid_include_path(include_path) {
                continue;
            }

            if let Some(banned_replacement) = banned_subpath_replacement(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Use #include \"{banned_replacement}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_relative_include_path(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Relative paths (../ or ./) are not allowed. Use paths relative to src/ instead (e.g., \"fl/foo.h\" or \"platforms/bar.h\"). Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_fastled_platform_relative(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - FastLED platform includes must use \"platforms/\" prefix. Use #include \"platforms/{include_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if let Some(typo_correction) = typo_include_suggestion(include_path) {
                let typo_prefix = include_path
                    .split_once('/')
                    .map_or(include_path, |(prefix, _)| prefix);
                let typo_prefix = format!("{typo_prefix}/");
                let rest_of_path = include_path.strip_prefix(&typo_prefix).unwrap_or("");
                let corrected_path = format!("{typo_correction}{rest_of_path}");
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - \"{typo_prefix}\" looks like a typo. Use #include \"{corrected_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            }
        }

        violations
    }
}

struct BuiltinMemcpyChecker;

impl FileContentChecker for BuiltinMemcpyChecker {
    fn name(&self) -> &'static str {
        "BuiltinMemcpyChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("compiler_control.h") {
            return false;
        }
        !normalize_path(file_path).contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_BUILTIN_MEMCPY") {
                continue;
            }
            if !code_part.contains("__builtin_memcpy") {
                continue;
            }
            if regex_builtin_memcpy().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_BUILTIN_MEMCPY instead of __builtin_memcpy: {stripped}\n      \
Rationale: FL_BUILTIN_MEMCPY wraps __builtin_memcpy for portability.\n      \
Include 'fl/stl/compiler_control.h' and replace __builtin_memcpy(...) with FL_BUILTIN_MEMCPY(...)"
                    ),
                ));
            }
        }

        violations
    }
}

struct WeakAttributeChecker;

impl FileContentChecker for WeakAttributeChecker {
    fn name(&self) -> &'static str {
        "WeakAttributeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !file_path.ends_with("compiler_control.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_LINK_WEAK") {
                continue;
            }
            if regex_weak_attribute().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!("Use FL_LINK_WEAK instead of __attribute__((weak)): {stripped}"),
                ));
            }
        }

        violations
    }
}

struct BannedDefineChecker;

impl FileContentChecker for BannedDefineChecker {
    fn name(&self) -> &'static str {
        "BannedDefineChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cpp.hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const WRONG_DEFINES: &[(&str, &str)] = &[
            ("#if ESP32", "Use #ifdef ESP32 instead of #if ESP32"),
            (
                "#if defined(FASTLED_RMT5)",
                "Use #ifdef FASTLED_RMT5 instead of #if defined(FASTLED_RMT5)",
            ),
            (
                "#if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
                "Use #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI instead of #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
            ),
        ];

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            for (needle, message) in WRONG_DEFINES {
                if code_part.contains(needle) {
                    violations.push((index + 1, (*message).to_string()));
                }
            }
        }

        violations
    }
}

struct BannedNamespaceChecker;

impl FileContentChecker for BannedNamespaceChecker {
    fn name(&self) -> &'static str {
        "BannedNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let skip_patterns = [".build", ".pio", ".venv", "third_party", "vendor"];
        if skip_patterns
            .iter()
            .any(|pattern| file_path.contains(pattern))
        {
            return false;
        }
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("fl::fl") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") {
                continue;
            }
            let code_part = split_line_comment(line);
            if !code_part.contains("fl::fl") {
                continue;
            }
            if regex_banned_namespace_fl_fl().is_match(code_part) {
                violations.push((index + 1, stripped.to_string()));
            }
        }
        violations
    }
}

struct CppIncludeChecker;

impl FileContentChecker for CppIncludeChecker {
    fn name(&self) -> &'static str {
        "CppIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) && !is_excluded_file(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.to_ascii_lowercase().contains("// ok cpp include"))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if let Some(captures) = regex_cpp_include().captures(code_part) {
                if let Some(cpp_file) = captures.get(1) {
                    violations.push((index + 1, format!("#include \"{}\"", cpp_file.as_str())));
                }
            }
        }

        violations
    }
}

struct CppHppIncludesChecker;

impl FileContentChecker for CppHppIncludesChecker {
    fn name(&self) -> &'static str {
        "CppHppIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let is_src = is_under_project_subpath(file_path, project_root, "src");
        let is_tests = is_under_project_subpath(file_path, project_root, "tests");

        if !is_src && !is_tests {
            return false;
        }

        if is_src && !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        if is_tests && !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }

        if is_src {
            if file_path.ends_with("_build.hpp")
                || file_path.ends_with("_build.cpp")
                || file_path.ends_with("_build.cpp.hpp")
            {
                return false;
            }
            if is_under_project_subpath(file_path, project_root, "src/fl/build") {
                return false;
            }
        }

        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let normalized = normalize_path(&file_content.path);
        let is_test = normalized.contains("/tests/") || normalized.starts_with("tests/");

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            let Some(captures) = regex_cpp_hpp_include().captures(code_part) else {
                continue;
            };
            let included_file = captures.get(1).map(|m| m.as_str()).unwrap_or("");
            if is_test {
                let h_header = included_file
                    .strip_suffix(".cpp.hpp")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or_else(|| included_file.to_string());
                let h_header = h_header
                    .strip_suffix(".impl.h")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or(h_header);
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - Including *.cpp.hpp files in tests is banned (hard ban, no opt-out). Include the public header instead: #include \"{h_header}\""
                    ),
                ));
            } else {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - *.cpp.hpp files should ONLY be included by _build.* files (hard ban, no opt-out). Found include of '{included_file}' in non-build file. Move this include to the appropriate _build.hpp file."
                    ),
                ));
            }
        }

        violations
    }
}

struct ImplHppIncludesChecker;

impl FileContentChecker for ImplHppIncludesChecker {
    fn name(&self) -> &'static str {
        "ImplHppIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "tests")
        {
            return false;
        }
        if !ends_with_any(
            file_path,
            &[".cpp", ".h", ".hpp", ".cpp.hpp", ".impl.cpp.hpp"],
        ) {
            return false;
        }
        !file_path.ends_with(".impl.cpp.hpp")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if let Some(captures) = regex_impl_hpp_include().captures(code_part) {
                let included_file = captures.get(1).map(|m| m.as_str()).unwrap_or("");
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - *.impl.hpp files must ONLY be included by *.impl.cpp.hpp router files. Found include of '{included_file}' in non-router file."
                    ),
                ));
            }
        }

        violations
    }
}

struct AsmJsLocationChecker;

impl FileContentChecker for AsmJsLocationChecker {
    fn name(&self) -> &'static str {
        "AsmJsLocationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        if normalized.ends_with(".js.cpp.hpp") {
            return Vec::new();
        }
        if !["EM_JS(", "EM_ASYNC_JS(", "EM_ASM("]
            .iter()
            .any(|token| file_content.content.contains(token))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if in_multiline_comment && line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code = stripped.split("//").next().unwrap_or(stripped).trim();
            if code.is_empty() {
                continue;
            }
            if regex_asm_js_macro().is_match(code) {
                violations.push((
                    index + 1,
                    format!(
                        "Emscripten JS glue macros must live in a *.js.cpp.hpp file: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct ReinterpretCastChecker;

impl FileContentChecker for ReinterpretCastChecker {
    fn name(&self) -> &'static str {
        "ReinterpretCastChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        !file_path.contains("third_party") && !file_path.contains("thirdparty")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("reinterpret_cast") {
                continue;
            }
            if code_part.contains("fl::reinterpret_cast_") {
                continue;
            }
            if line.contains("// ok reinterpret cast") || line.contains("// okay reinterpret cast")
            {
                continue;
            }
            violations.push((index + 1, stripped.to_string()));
        }

        violations
    }
}

struct PragmaOnceChecker;

impl FileContentChecker for PragmaOnceChecker {
    fn name(&self) -> &'static str {
        "PragmaOnceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if file_path.ends_with(".cpp.hpp") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        !normalized.contains("/third_party/") && !normalized.contains("/platforms/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let has_pragma_once = file_content.content.contains("#pragma once");
        let is_header = ends_with_any(&file_content.path, &[".h", ".hpp"]);
        let is_cpp = file_content.path.ends_with(".cpp");

        if is_header && !has_pragma_once {
            return vec![(
                1,
                "Missing #pragma once directive. Add '#pragma once' at the top of the header file."
                    .to_string(),
            )];
        }

        if is_cpp && has_pragma_once {
            for (index, line) in file_content.lines.iter().enumerate() {
                if line.contains("#pragma once") {
                    return vec![(
                        index + 1,
                        "Incorrect #pragma once in .cpp file. Remove '#pragma once' from source files."
                            .to_string(),
                    )];
                }
            }
        }

        Vec::new()
    }
}

struct EspRomPrintfChecker;

impl FileContentChecker for EspRomPrintfChecker {
    fn name(&self) -> &'static str {
        "EspRomPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !normalize_path(file_path).contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if !code_part.contains("esp_rom_printf") {
                continue;
            }
            if !regex_esp_rom_printf().is_match(code_part) {
                continue;
            }
            if comment_part
                .to_ascii_lowercase()
                .contains("ok esp_rom_printf")
            {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Avoid `esp_rom_printf` — use FastLED logging (FL_LOG_INFO, FL_WARN, FL_PRINT) or fl::io instead: {stripped}\n      Rationale: esp_rom_printf bypasses FastLED's logging facilities, blocks on a slow peripheral, and proliferates platform-specific I/O into general code.\n      If this call is genuinely required (e.g. early bootstrap before logging is up), suppress with `// ok esp_rom_printf - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

struct SleepForChecker;

impl FileContentChecker for SleepForChecker {
    fn name(&self) -> &'static str {
        "SleepForChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        if file_path.contains("third_party") || file_path.contains("thirdparty") {
            return false;
        }
        let normalized = normalize_path(file_path);
        !SLEEP_FOR_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            if line.contains("// ok sleep for") || line.contains("// okay sleep for") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("sleep_for") {
                continue;
            }
            if regex_sleep_for().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "⚠️  CRITICAL: sleep_for() BLOCKS async/scheduler pumping! Async operations HANG, tasks FREEZE, UI becomes UNRESPONSIVE. USE fl::delay(ms) INSTEAD! Only suppress with '// ok sleep for' in core infrastructure: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct ThreadLocalKeywordChecker;

impl FileContentChecker for ThreadLocalKeywordChecker {
    fn name(&self) -> &'static str {
        "ThreadLocalKeywordChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("thread_local") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            let code_without_strings = strip_string_literals(code_part);
            if regex_thread_local().is_match(&code_without_strings)
                && !line.contains("// ok thread_local")
            {
                violations.push((
                    index + 1,
                    format!(
                        "❌ Raw 'thread_local' keyword is banned — use fl::SingletonThreadLocal<T>::instance() instead (portable, never-destroyed, LSAN-safe): {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct SpanFromPointerChecker;

impl FileContentChecker for SpanFromPointerChecker {
    fn name(&self) -> &'static str {
        "SpanFromPointerChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !file_path.contains("third_party") && !file_path.contains("thirdparty")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("span<") && !file_content.content.contains("span <") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            if line.contains("// ok span from pointer") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains(".data()") || !code_part.contains(".size()") {
                continue;
            }
            if !code_part.contains("span<") && !code_part.contains("span <") {
                continue;
            }
            if regex_span_data_size().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped}  →  Use span<T>(container) instead of span<T>(container.data(), container.size()). Suppress with '// ok span from pointer'"
                    ),
                ));
            }
        }

        violations
    }
}

struct RelativeIncludeChecker;

impl FileContentChecker for RelativeIncludeChecker {
    fn name(&self) -> &'static str {
        "RelativeIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        is_under_project_subpath(file_path, project_root, "src")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".cxx", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const ALLOWED_RELATIVE_INCLUDE_FILES: &[&str] = &[
            "src/platforms/win/run_example.hpp",
            "src/platforms/posix/run_example.hpp",
            "src/platforms/apple/run_example.hpp",
        ];

        let normalized_path = normalize_path(&file_content.path);
        if ALLOWED_RELATIVE_INCLUDE_FILES
            .iter()
            .any(|allowed_file| normalized_path.ends_with(allowed_file))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let visible_line = strip_block_comments_from_line(line, &mut in_block_comment);
            if regex_relative_include().is_match(&visible_line)
                && !line.contains("// ok relative include")
            {
                violations.push((index + 1, format!("Relative include: {}", line.trim())));
            }
        }

        violations
    }
}

struct FastLEDHeaderUsageChecker;

impl FileContentChecker for FastLEDHeaderUsageChecker {
    fn name(&self) -> &'static str {
        "FastLEDHeaderUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !matches!(basename, "FastLED.h" | "fastspi.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        let visible_lines: Vec<String> = file_content
            .lines
            .iter()
            .map(|line| strip_block_comments_from_line(line, &mut in_block_comment))
            .collect();

        for (index, visible_line) in visible_lines.iter().enumerate() {
            if !regex_fastled_h_include().is_match(visible_line) {
                continue;
            }
            let line = &file_content.lines[index];
            let lower = line.to_ascii_lowercase();
            if lower.contains("// ok include") {
                continue;
            }

            let lookback = (index).min(5);
            let has_internal_define = (1..=lookback)
                .map(|offset| &visible_lines[index - offset])
                .any(|prev_line| regex_fastled_internal_define().is_match(prev_line));
            if !has_internal_define {
                violations.push((
                    index + 1,
                    format!(
                        "Use 'fl/system/fastled.h' instead of 'FastLED.h': {}",
                        line.trim()
                    ),
                ));
            }
        }

        violations
    }
}

struct ArduinoMacroUsageChecker;

impl FileContentChecker for ArduinoMacroUsageChecker {
    fn name(&self) -> &'static str {
        "ArduinoMacroUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if is_under_dir(file_path, "platforms") || is_under_dir(file_path, "third_party") {
            return false;
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if regex_arduino_preprocessor().is_match(code_part)
                || regex_arduino_scoped_enum().is_match(code_part)
                || regex_arduino_enum_member().is_match(code_part)
            {
                continue;
            }

            for name in ARDUINO_BANNED_MACROS {
                let Some(found) = regex_arduino_macro()
                    .captures_iter(code_part)
                    .find(|capture| capture.get(1).is_some_and(|m| m.as_str() == *name))
                else {
                    continue;
                };
                let macro_name = found.get(1).unwrap().as_str();
                violations.push((
                    index + 1,
                    format!(
                        "Banned Arduino macro '{macro_name}' used: {stripped}\n      These macros pollute the global namespace and conflict with Windows headers.\n      Use platform-specific APIs or define local constants instead."
                    ),
                ));
            }
        }

        violations
    }
}

struct AttributeChecker;

impl FileContentChecker for AttributeChecker {
    fn name(&self) -> &'static str {
        "AttributeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("compiler_control.h") {
            return false;
        }
        if is_under_dir(&normalized, "third_party") {
            return false;
        }
        !ends_with_any(&normalized, &["doctest.h", "catch.hpp", "gtest.h"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_") {
                continue;
            }
            if !code_part.contains("[[") && !code_part.contains("alignas") {
                continue;
            }

            if code_part.contains("[[") {
                for capture in regex_standard_attribute().captures_iter(code_part) {
                    let Some(attribute) = capture.get(1).map(|m| m.as_str()) else {
                        continue;
                    };
                    if let Some(fl_macro) = standard_attribute_replacement(attribute) {
                        violations.push((
                            index + 1,
                            format!("Use {fl_macro} instead of [[{attribute}]]: {stripped}"),
                        ));
                    }
                }
            }

            if code_part.contains("alignas")
                && regex_alignas().is_match(code_part)
                && !code_part.contains("FL_ALIGNAS")
            {
                violations.push((
                    index + 1,
                    format!("Use FL_ALIGNAS instead of alignas: {stripped}"),
                ));
            }
        }

        violations
    }
}

struct FlIsDefinedChecker;

impl FileContentChecker for FlIsDefinedChecker {
    fn name(&self) -> &'static str {
        "FlIsDefinedChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("FL_IS_") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment {
                continue;
            }

            let stripped = line.trim();
            if stripped.starts_with("//") || !regex_preprocessor_if_elif().is_match(stripped) {
                continue;
            }

            let code_part = split_line_comment(line);
            if !regex_fl_is_token().is_match(code_part) {
                continue;
            }
            let stripped_code = regex_defined_fl_is().replace_all(code_part, "");
            for token in regex_fl_is_token().find_iter(&stripped_code) {
                violations.push((
                    index + 1,
                    format!(
                        "Bare '{}' in preprocessor conditional. FL_IS_* macros are defined/undefined (no value). Use '#ifdef {}' or '#if defined({})' instead.",
                        token.as_str(),
                        token.as_str(),
                        token.as_str()
                    ),
                ));
            }
        }

        violations
    }
}

struct NumericLimitMacroChecker;

impl FileContentChecker for NumericLimitMacroChecker {
    fn name(&self) -> &'static str {
        "NumericLimitMacroChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !NUMERIC_LIMIT_EXCLUDED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            if line.contains("// okay numeric limit macro") {
                continue;
            }
            let code_part = split_line_comment(line);
            if !code_part.contains("_MAX") && !code_part.contains("_MIN") {
                continue;
            }

            let Some(capture) = regex_numeric_limit_macro().captures(code_part) else {
                continue;
            };
            let macro_name = capture.get(1).unwrap().as_str();
            let suggestion = numeric_limit_suggestion(macro_name);
            violations.push((index + 1, format!("{stripped} (use {suggestion} instead)")));
        }

        violations
    }
}

struct PlatformPragmaChecker;

impl FileContentChecker for PlatformPragmaChecker {
    fn name(&self) -> &'static str {
        "PlatformPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !is_under_dir(&normalized, "third_party")
            && !is_under_dir(&normalized, "platforms")
            && !normalized.ends_with("compiler_control.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            if index >= 1 && file_content.lines[index - 1].contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if line.contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if regex_platform_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw platform-specific pragma: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(<name>) / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h.\n      If the FL macros cannot express this pragma, place FL_ALLOW_PLATFORM_PRAGMA on the preceding line as an escape hatch."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawPragmaChecker;

impl FileContentChecker for RawPragmaChecker {
    fn name(&self) -> &'static str {
        "RawPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("compiler_control.h") && !is_under_dir(&normalized, "third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            if regex_raw_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw _Pragma() usage: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING_* / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h instead.\n      _Pragma() is not portable across all target compilers."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawNoexceptChecker;

impl FileContentChecker for RawNoexceptChecker {
    fn name(&self) -> &'static str {
        "RawNoexceptChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp", ".cpp", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("fl/stl/noexcept.h") && !is_under_dir(&normalized, "third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("noexcept") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code = split_line_comment(stripped).trim();
            if code.is_empty()
                || regex_preprocessor_include().is_match(code)
                || regex_define_fl_noexcept().is_match(code)
                || !regex_raw_noexcept().is_match(code)
            {
                continue;
            }

            let temp = code.replace("FL_NOEXCEPT", "");
            let remaining: Vec<_> = regex_raw_noexcept().find_iter(&temp).collect();
            let all_operator_form = remaining
                .iter()
                .all(|m| temp[m.end()..].trim_start().starts_with('('));
            if !remaining.is_empty() && all_operator_form {
                continue;
            }
            if regex_noexcept_suppression().is_match(line) {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Raw 'noexcept' keyword — use FL_NOEXCEPT macro instead (defined in fl/stl/noexcept.h, currently a noop everywhere for cross-platform compatibility): {stripped}"
                ),
            ));
        }

        violations
    }
}

struct SingletonInHeadersChecker;

impl FileContentChecker for SingletonInHeadersChecker {
    fn name(&self) -> &'static str {
        "SingletonInHeadersChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("fl/stl/singleton.h") {
            return false;
        }
        if normalized.ends_with(".hpp") {
            return false;
        }
        !is_excluded_file(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let is_cpp_hpp = file_content.path.ends_with(".cpp.hpp");
        let is_private_header = file_content
            .lines
            .iter()
            .take(50)
            .any(|line| line.contains("IWYU pragma: private"));
        if is_cpp_hpp || is_private_header {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("Singleton") {
                continue;
            }
            if is_cpp_hpp {
                if regex_singleton_shared().is_match(code_part) {
                    violations.push((
                        index + 1,
                        format!("Use Singleton<T> instead of SingletonShared<T> in .cpp.hpp: {stripped}"),
                    ));
                }
            } else if regex_singleton().is_match(code_part) {
                if regex_friend_class().is_match(code_part) {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use SingletonShared<T> instead of Singleton<T> in headers: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct StdNamespaceChecker;

impl FileContentChecker for StdNamespaceChecker {
    fn name(&self) -> &'static str {
        "StdNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "tests")
        {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.contains("third_party") || normalized.contains("thirdparty") {
            return false;
        }
        !is_std_bridge_file(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("std::")
                || line.contains("// okay std namespace")
                || line_has_only_allowed_std_symbols(line)
            {
                continue;
            }
            violations.push((index + 1, stripped.to_string()));
        }

        violations
    }
}

struct ExampleSerialChecker;

impl FileContentChecker for ExampleSerialChecker {
    fn name(&self) -> &'static str {
        "ExampleSerialChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        normalize_path(file_path).contains("examples/AutoResearch/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if !code_part.contains("Serial.") {
                continue;
            }
            let Some(capture) = regex_serial_method().captures(code_part) else {
                continue;
            };
            let method = capture.get(1).unwrap().as_str();
            if ALLOWED_SERIAL_METHODS.contains(&method)
                || !FORBIDDEN_SERIAL_METHODS.contains(&method)
            {
                continue;
            }
            if comment_part.to_ascii_lowercase().contains("ok serial") {
                continue;
            }

            let replacement = serial_replacement(method);
            violations.push((
                index + 1,
                format!(
                    "Avoid `Serial.{method}(...)` in enforced examples — use `{replacement}` instead.\n      Rationale: fl:: wrappers carry the non-blocking HWCDC fixes from FastLED #2669 (setTxTimeoutMs=0, guarded flush, host-presence skip). Raw `Serial.{method}` bypasses them.\n      Line: {stripped}\n      If this call is genuinely required (platform-specific config with no fl:: equivalent), suppress with `// ok serial - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

struct IncludeAfterNamespaceChecker;

impl FileContentChecker for IncludeAfterNamespaceChecker {
    fn name(&self) -> &'static str {
        "IncludeAfterNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if INCLUDE_AFTER_NAMESPACE_SKIP_PATTERNS
            .iter()
            .any(|pattern| file_path.contains(pattern))
        {
            return false;
        }
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .any(|line| regex_allow_include_after_namespace().is_match(line))
        {
            return Vec::new();
        }

        let mut namespace_started = false;
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            if regex_namespace_declaration().is_match(line) {
                namespace_started = true;
                continue;
            }
            if namespace_started && regex_any_include().is_match(line) {
                if regex_nolint().is_match(line) {
                    continue;
                }
                violations.push((index + 1, line.trim_end_matches('\n').to_string()));
            }
        }

        violations
    }
}

struct NamespaceFlDeclarationChecker;

impl FileContentChecker for NamespaceFlDeclarationChecker {
    fn name(&self) -> &'static str {
        "NamespaceFlDeclarationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let path = Path::new(file_path);
        if path
            .parent()
            .and_then(Path::file_name)
            .and_then(|name| name.to_str())
            != Some("src")
        {
            return false;
        }
        let Ok(src_root) = project_root.join("src").canonicalize() else {
            return false;
        };
        let Ok(parent) = path
            .parent()
            .unwrap_or_else(|| Path::new(""))
            .canonicalize()
        else {
            return false;
        };
        if parent != src_root {
            return false;
        }
        matches!(
            path.extension().and_then(|value| value.to_str()),
            Some("h" | "cpp")
        )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.trim().starts_with("//") {
                continue;
            }
            if regex_namespace_fl_declaration().is_match(line) {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceFlChecker;

impl FileContentChecker for UsingNamespaceFlChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        if normalized.contains("FastLED.h") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.starts_with("//") {
                continue;
            }
            if line.contains("using namespace fl;") {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceChecker;

impl FileContentChecker for UsingNamespaceChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".hxx", ".hh"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            let without_line_comment = split_line_comment(line);
            let line_clean = strip_inline_block_comments(without_line_comment);
            if !regex_using_namespace().is_match(&line_clean) {
                continue;
            }
            if regex_define_using_namespace().is_match(&line_clean) {
                continue;
            }

            let mut is_conditional = false;
            for previous_index in index.saturating_sub(5)..index {
                if regex_force_use_namespace().is_match(file_content.lines[previous_index].trim()) {
                    is_conditional = true;
                    break;
                }
            }
            if !is_conditional {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct PlatformIncludesChecker;

impl FileContentChecker for PlatformIncludesChecker {
    fn name(&self) -> &'static str {
        "PlatformIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized) {
            return false;
        }
        if normalized.contains("/platforms/") || normalized.contains("/tests/platforms/") {
            return false;
        }
        PLATFORM_INCLUDE_LOCATIONS
            .iter()
            .any(|location| normalized.contains(location))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            let comment = comment_part.to_ascii_lowercase();
            let has_exception = comment.contains("ok platform headers")
                || comment.contains("ok -- platform headers");
            if has_exception {
                continue;
            }

            let Some(capture) = regex_deep_platform_header().captures(code_part) else {
                continue;
            };
            let include_statement = capture.get(1).unwrap().as_str().trim();
            violations.push((
                index + 1,
                format!(
                    "Forbidden deep platform header: {include_statement} - Code in fl/** must use top-level platform headers (platforms/*.h) or fl/ alternatives instead of deep platform-specific headers. Deep includes (platforms/{{platform}}/**/*.h) bypass the trampoline architecture. If this include is necessary, add '// ok platform headers' comment to suppress. See src/platforms/README.md for architecture details."
                ),
            ));
        }

        violations
    }
}

struct PlatformTrampolineChecker;

impl FileContentChecker for PlatformTrampolineChecker {
    fn name(&self) -> &'static str {
        "PlatformTrampolineChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        let Some(parts_after_src) = normalized.split("/src/").nth(1) else {
            return false;
        };
        let parts: Vec<&str> = parts_after_src.split('/').collect();
        if parts.first() == Some(&"platforms") {
            return false;
        }
        if parts.first() == Some(&"fl") {
            return true;
        }
        parts.len() == 1 && ends_with_any(&normalized, &[".cpp", ".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            let comment = comment_part.to_ascii_lowercase();
            let has_exception = comment.contains("ok platform headers")
                || comment.contains("ok -- platform headers");
            if has_exception {
                continue;
            }

            let Some(capture) = regex_deep_platform_include().captures(code_part) else {
                continue;
            };
            let include_path = format!(
                "platforms/{}/{}",
                capture.get(1).unwrap().as_str(),
                capture.get(2).unwrap().as_str()
            );
            let suggestion = platform_trampoline_suggestion(&include_path);
            violations.push((
                index + 1,
                format!(
                    "Forbidden deep platform header: #include \"{include_path}\" - Use \"{suggestion}\" instead (platform trampolines only). Deep includes bypass the trampoline architecture. If necessary, add \"// ok platform headers\" comment to suppress."
                ),
            ));
        }

        violations
    }
}

struct SimdIntrinsicsChecker;

impl FileContentChecker for SimdIntrinsicsChecker {
    fn name(&self) -> &'static str {
        "SimdIntrinsicsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
        {
            return false;
        }
        !normalized.contains("/platforms/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            if line.contains("// ok platform simd") {
                continue;
            }

            let code_part = split_line_comment(line);
            for (pattern, description) in SIMD_PATTERNS {
                if code_part.contains(pattern) {
                    violations.push((index + 1, format!("{stripped}  [{description}]")));
                    break;
                }
            }
        }

        violations
    }
}

struct CppHppHeaderPairChecker;

impl FileContentChecker for CppHppHeaderPairChecker {
    fn name(&self) -> &'static str {
        "CppHppHeaderPairChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !normalized.ends_with(".cpp.hpp") {
            return false;
        }
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !basename.starts_with("_build")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.contains("// ok no header"))
        {
            return Vec::new();
        }
        let Some(header_path) = file_content.path.strip_suffix(".cpp.hpp") else {
            return Vec::new();
        };
        let expected_header = format!("{header_path}.h");
        if Path::new(&expected_header).exists() {
            return Vec::new();
        }
        let mut expected_display = project_relative_guess(&expected_header);
        if cfg!(windows) {
            expected_display = expected_display.replace('/', "\\");
        }
        vec![(
            1,
            format!("No corresponding header found: expected {expected_display}"),
        )]
    }
}

struct IsHeaderIncludeChecker;

impl FileContentChecker for IsHeaderIncludeChecker {
    fn name(&self) -> &'static str {
        "IsHeaderIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/platforms") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        if file_name.starts_with("is_") {
            return false;
        }
        let file_name_lower = file_name.to_lowercase();
        !file_name_lower.contains("compile_test") && !file_name_lower.contains("core_detection")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut macros = BTreeSet::new();
        let mut in_block_comment = false;

        for line in &file_content.lines {
            if line.contains("/*") {
                in_block_comment = true;
            }
            if line.contains("*/") {
                in_block_comment = false;
                continue;
            }
            if in_block_comment {
                continue;
            }
            let stripped = line.trim();
            if stripped.starts_with("//") || !regex_preprocessor_conditional().is_match(stripped) {
                continue;
            }
            let code_part = split_line_comment(line);
            for token in regex_fl_is_token().find_iter(code_part) {
                macros.insert(token.as_str().to_string());
            }
        }

        if macros.is_empty() {
            return Vec::new();
        }

        let mut included_names = HashSet::new();
        for line in &file_content.lines {
            if let Some(capture) = regex_include_any_path().captures(line) {
                let include_path = &capture[1];
                let include_name = include_path.rsplit('/').next().unwrap_or(include_path);
                included_names.insert(include_name.to_string());
            }
        }
        let has_is_platform = included_names.contains("is_platform.h");

        let mut required: BTreeMap<&str, BTreeSet<String>> = BTreeMap::new();
        for macro_name in macros {
            if let Some(header) = required_fl_is_header(&macro_name) {
                required.entry(header).or_default().insert(macro_name);
            }
        }

        let mut violations = Vec::new();
        for (header, macros) in required {
            if included_names.contains(header) || has_is_platform {
                continue;
            }
            let macros_str = macros.into_iter().collect::<Vec<_>>().join(", ");
            violations.push((
                0,
                format!(
                    "File uses {macros_str} but does not include '{header}' or 'is_platform.h'. Add: #include \"{header}\""
                ),
            ));
        }
        violations
    }
}

struct IwyuPragmaBlockChecker;

impl FileContentChecker for IwyuPragmaBlockChecker {
    fn name(&self) -> &'static str {
        "IwyuPragmaBlockChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if !ends_with_any(&normalized, &[".h", ".hpp", ".hh", ".hxx"]) {
            return false;
        }
        !normalized.contains("/third_party/") && !normalized.contains("/platforms/stub/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized_path = normalize_path(&file_content.path);
        let is_fl_file = normalized_path.contains("/src/fl/");
        let mut violations = Vec::new();
        let mut in_pragma_block = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if regex_iwyu_begin().is_match(line) {
                in_pragma_block = true;
                continue;
            }
            if regex_iwyu_end().is_match(line) {
                in_pragma_block = false;
                continue;
            }

            let Some(capture) = regex_iwyu_include().captures(line) else {
                continue;
            };
            let header_path = &capture[1];
            let comment = capture.get(2).map_or("", |value| value.as_str());
            if in_pragma_block || regex_iwyu_keep_inline().is_match(comment) {
                continue;
            }

            let header_type = iwyu_classify_header(header_path, line);
            if header_type == "internal" {
                continue;
            }
            if header_type == "platform" {
                if iwyu_is_top_level_platform_header(header_path) || !is_fl_file {
                    continue;
                }
                let message = iwyu_format_violation(header_path, header_type, line.trim());
                violations.push((index + 1, message));
            } else if header_type == "system" {
                let message = iwyu_format_violation(header_path, header_type, line.trim());
                violations.push((index + 1, message));
            }
        }

        violations
    }
}

struct MemberStyleChecker;

impl FileContentChecker for MemberStyleChecker {
    fn name(&self) -> &'static str {
        "MemberStyleChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp", ".cpp"]) {
            return false;
        }
        if is_excluded_file(&normalized) || normalized.contains("third_party") {
            return false;
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let mut code_part = split_line_comment(line).to_string();
            if code_part.trim().is_empty() {
                continue;
            }

            let has_trailing_underscore = code_part.contains("_;")
                || code_part.contains("_=")
                || code_part.contains("_,")
                || code_part.contains("_)")
                || code_part.contains("_ ");
            let has_m_underscore = code_part.contains("m_");
            if !has_trailing_underscore && !has_m_underscore {
                continue;
            }

            if code_part.contains('"') {
                code_part = regex_string_literal()
                    .replace_all(&code_part, "\"\"")
                    .to_string();
            }

            if has_trailing_underscore {
                let code_for_positions = code_part.clone();
                for capture in regex_member_trailing_underscore().captures_iter(&code_part) {
                    let Some(var_match) = capture.get(1) else {
                        continue;
                    };
                    let var_name = var_match.as_str();
                    if is_inside_parens(&code_for_positions, var_match.start()) {
                        continue;
                    }
                    if code_part.trim_start().starts_with('#') {
                        continue;
                    }
                    if var_name.contains("__")
                        || code_part.contains("COUNTER")
                        || code_part.contains("LINE")
                    {
                        continue;
                    }
                    let base = var_name.trim_end_matches('_');
                    if base
                        .chars()
                        .all(|ch| !ch.is_alphabetic() || ch.is_uppercase())
                        && var_name.chars().count() > 2
                    {
                        continue;
                    }
                    let suggested_name = convert_google_to_m_prefix(var_name);
                    violations.push((
                        index + 1,
                        format!(
                            "{var_name} -> {suggested_name}: {}",
                            stripped.chars().take(80).collect::<String>()
                        ),
                    ));
                }
            }

            if has_m_underscore {
                let code_for_positions = code_part.clone();
                for capture in regex_member_m_underscore().captures_iter(&code_part) {
                    let Some(var_match) = capture.get(1) else {
                        continue;
                    };
                    let var_name = var_match.as_str();
                    if is_inside_parens(&code_for_positions, var_match.start()) {
                        continue;
                    }
                    if code_part.trim_start().starts_with('#') {
                        continue;
                    }
                    if var_name.contains("__")
                        || code_part.contains("COUNTER")
                        || code_part.contains("LINE")
                    {
                        continue;
                    }
                    let suggested_name = convert_m_underscore_to_m_prefix(var_name);
                    violations.push((
                        index + 1,
                        format!(
                            "{var_name} -> {suggested_name}: {}",
                            stripped.chars().take(80).collect::<String>()
                        ),
                    ));
                }
            }
        }

        violations
    }
}

struct BareUsingChecker;

impl FileContentChecker for BareUsingChecker {
    fn name(&self) -> &'static str {
        "BareUsingChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if basename == "FastLED.h" {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut scope_stack: Vec<&str> = Vec::new();
        let mut in_block_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if in_block_comment {
                if line.contains("*/") {
                    in_block_comment = false;
                }
                continue;
            }

            let line_before_line_comment = split_line_comment(line);
            if let Some(start_pos) = line_before_line_comment.find("/*") {
                let rest = &line[start_pos + 2..];
                if !rest.contains("*/") {
                    in_block_comment = true;
                }
                continue;
            }

            if regex_preprocessor_line().is_match(line) {
                continue;
            }

            let without_strings = strip_string_literals(line);
            let clean = split_line_comment(&without_strings).to_string();
            let mut open_braces = clean.matches('{').count();
            let close_braces = clean.matches('}').count();
            let mut scopes_to_push: Vec<&str> = Vec::new();

            if open_braces > 0 {
                if regex_anon_namespace_open().is_match(&clean) {
                    scopes_to_push.push("local");
                    open_braces -= 1;
                } else if regex_named_namespace_open().is_match(&clean) {
                    scopes_to_push.push("namespace");
                    open_braces -= 1;
                } else if regex_extern_c_open().is_match(&clean) {
                    scopes_to_push.push("namespace");
                    open_braces -= 1;
                } else if regex_local_scope_keyword().is_match(&clean) {
                    scopes_to_push.push("local");
                    open_braces -= 1;
                }
                scopes_to_push.extend(std::iter::repeat("local").take(open_braces));
            }

            scope_stack.extend(scopes_to_push);
            for _ in 0..close_braces {
                if !scope_stack.is_empty() {
                    scope_stack.pop();
                }
            }

            if regex_bare_using_decl().is_match(&clean) {
                if regex_bare_using_suppress().is_match(line) {
                    continue;
                }
                if scope_stack.iter().all(|scope| *scope == "namespace") {
                    violations.push((index + 1, line.trim().to_string()));
                }
            }
        }

        violations
    }
}

struct CtypeGlobalChecker;

impl FileContentChecker for CtypeGlobalChecker {
    fn name(&self) -> &'static str {
        "CtypeGlobalChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src")
            && !is_under_project_subpath(&normalized, project_root, "tests")
        {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
        {
            return false;
        }
        !CTYPE_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !ctype_functions().any(|func| file_content.content.contains(func)) {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut fl_namespace_depth = 0_i32;
        let mut brace_depth_at_fl_namespace: Vec<i32> = Vec::new();
        let mut brace_depth = 0_i32;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("namespace")
                && regex_namespace_fl_declaration().is_match(code_part)
            {
                fl_namespace_depth += 1;
                brace_depth_at_fl_namespace.push(brace_depth);
            }
            for ch in code_part.chars() {
                if ch == '{' {
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if brace_depth_at_fl_namespace
                        .last()
                        .is_some_and(|depth| brace_depth == *depth)
                    {
                        fl_namespace_depth -= 1;
                        brace_depth_at_fl_namespace.pop();
                    }
                }
            }

            if line.contains("// ok ctype") || line.contains("// okay ctype") {
                continue;
            }
            if !ctype_functions().any(|func| code_part.contains(func)) {
                continue;
            }

            let calls = find_ctype_calls(code_part);
            if calls.is_empty() {
                continue;
            }

            if fl_namespace_depth > 0 {
                let global_funcs: BTreeSet<&str> = calls
                    .iter()
                    .filter_map(|(func, is_global)| (*is_global).then_some(*func))
                    .collect();
                for func in global_funcs {
                    let header = ctype_header(func);
                    violations.push((
                        index + 1,
                        format!(
                            "Use fl::{func}() instead of ::{func}() — see {header}: {stripped}"
                        ),
                    ));
                }
                continue;
            }

            let funcs: BTreeSet<&str> = calls.iter().map(|(func, _)| *func).collect();
            for func in funcs {
                let header = ctype_header(func);
                violations.push((
                    index + 1,
                    format!(
                        "Use fl::{func}() instead of {func}() or ::{func}() — see {header}: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct StdintTypeChecker;

impl FileContentChecker for StdintTypeChecker {
    fn name(&self) -> &'static str {
        "StdintTypeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        let filename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if STDINT_EXCLUDED_FILENAMES.contains(&filename) {
            return false;
        }
        if normalized.contains("/third_party/") || normalized.contains("/fl/stl/") {
            return false;
        }
        if normalized.contains("/platforms/") {
            if filename.starts_with("int_") && filename.ends_with(".h") {
                return false;
            }
            if filename == "Arduino.h" || filename == "Arduino.cpp.hpp" {
                return false;
            }
            if normalized.contains("/stub/") || normalized.contains("/test/") {
                return false;
            }
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("int") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut in_extern_c_block = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if !in_multiline_comment
                && !line.contains("int")
                && !line.contains("/*")
                && !line.contains("*/")
            {
                continue;
            }

            let trimmed_start = line.trim_start_matches(|ch| ch == ' ' || ch == '\t');
            let is_line_comment = trimmed_start.starts_with("//");
            if is_line_comment && !in_multiline_comment {
                continue;
            }

            if line.contains("extern \"C\"") {
                if line.contains('{') {
                    in_extern_c_block = true;
                } else if line.contains(';') {
                    continue;
                }
            }
            if in_extern_c_block && line.contains('}') {
                in_extern_c_block = false;
                continue;
            }
            if in_extern_c_block {
                continue;
            }

            if in_multiline_comment {
                if line.contains("*/") {
                    in_multiline_comment = false;
                }
                continue;
            }

            let code_for_comment_tracking = split_line_comment(line);
            if let Some(open_pos) = code_for_comment_tracking.find("/*") {
                let close_pos = code_for_comment_tracking[open_pos + 2..].find("*/");
                if close_pos.is_none() {
                    in_multiline_comment = true;
                    continue;
                }
            }

            if !line.contains("int") {
                continue;
            }

            let mut code_part = split_line_comment(line).to_string();
            while let Some(open_pos) = code_part.find("/*") {
                let Some(close_relative) = code_part[open_pos + 2..].find("*/") else {
                    code_part.truncate(open_pos);
                    break;
                };
                let close_pos = open_pos + 2 + close_relative;
                code_part.replace_range(open_pos..close_pos + 2, " ");
            }

            for match_type in find_stdint_matches(&code_part) {
                let fl_type = stdint_fl_type(match_type);
                violations.push((
                    index + 1,
                    format!(
                        "Use '{fl_type}' from fl/stl/int.h instead of '{match_type}': {}",
                        line.trim()
                    ),
                ));
            }
        }

        violations
    }
}

struct SubdirNamespaceChecker {
    subdir: &'static str,
}

impl FileContentChecker for SubdirNamespaceChecker {
    fn name(&self) -> &'static str {
        "SubdirNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let subpath = format!("src/fl/{}", self.subdir);
        is_under_project_subpath(file_path, project_root, &subpath) && file_path.ends_with(".h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let expected = ["fl", self.subdir];
        let mut found_parts: HashSet<String> = HashSet::new();
        let mut has_any_namespace = false;

        for line in &file_content.lines {
            let stripped = line.trim();
            if stripped.starts_with("//") || stripped.starts_with("/*") || stripped.starts_with('*')
            {
                continue;
            }
            let code = split_line_comment(stripped);
            for capture in regex_subdir_namespace_decl().captures_iter(code) {
                has_any_namespace = true;
                for part in capture[1].split("::").filter(|part| !part.is_empty()) {
                    found_parts.insert(part.to_string());
                }
            }
        }

        if !has_any_namespace {
            return Vec::new();
        }

        let missing: Vec<&str> = expected
            .iter()
            .copied()
            .filter(|part| !found_parts.contains(*part))
            .collect();
        if missing.is_empty() {
            return Vec::new();
        }

        let expected_ns = expected.join("::");
        vec![(
            1,
            format!(
                "Header in fl/{}/ must use namespace {expected_ns}; missing namespace part(s): {}",
                self.subdir,
                missing.join(", ")
            ),
        )]
    }
}

struct UnitTestChecker;

impl FileContentChecker for UnitTestChecker {
    fn name(&self) -> &'static str {
        "UnitTestChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !UNIT_TEST_EXEMPT_FILES.contains(&basename)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.starts_with("//") {
                continue;
            }

            if stripped.contains("#include \"doctest.h\"")
                || stripped.contains("#include <doctest.h>")
            {
                violations.push((
                    index + 1,
                    "Use #include \"test.h\" instead of #include \"doctest.h\"".to_string(),
                ));
            }

            if !UNIT_TEST_FAST_PREFIXES
                .iter()
                .any(|prefix| line.contains(prefix))
            {
                continue;
            }

            for capture in regex_unit_test_macro_call().captures_iter(line) {
                let macro_name = &capture[1];
                if let Some(fl_macro) = unit_test_fl_macro(macro_name) {
                    violations.push((
                        index + 1,
                        format!("Use {fl_macro}() instead of bare {macro_name}()"),
                    ));
                }
            }
        }

        violations
    }
}

struct HeadersExistChecker;

impl FileContentChecker for HeadersExistChecker {
    fn name(&self) -> &'static str {
        "HeadersExistChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".hpp"]) {
            return false;
        }
        let name = normalized.rsplit('/').next().unwrap_or(&normalized);
        if HEADERS_EXIST_EXCLUDED_FILES.contains(&name) {
            return false;
        }
        let Some(rel) = tests_relative_path(&normalized) else {
            return false;
        };
        let parts: Vec<&str> = rel.split('/').collect();
        if parts
            .iter()
            .any(|part| matches!(*part, "core" | "shared" | "test_utils"))
        {
            return false;
        }
        !parts.iter().any(|part| {
            *part == "build"
                || part.starts_with(".build-")
                || *part == "example_compile_direct"
                || *part == "CMakeFiles"
        })
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        let root_prefix = project_root_prefix_for_file(&normalized);
        let Some(test_rel) = tests_relative_path(&normalized) else {
            return Vec::new();
        };
        let first_part = test_rel.split('/').next().unwrap_or("");
        if first_part == "misc" || first_part == "profile" {
            return Vec::new();
        }
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.to_lowercase().contains("// ok standalone"))
        {
            return Vec::new();
        }

        let includes: Vec<String> = file_content
            .lines
            .iter()
            .filter_map(|line| regex_quoted_include_line().captures(line))
            .filter_map(|capture| {
                let header = capture[1].to_string();
                (!header.contains("test.h")
                    && !header.starts_with("testing/")
                    && !header.starts_with("test_utils/"))
                .then_some(header)
            })
            .collect();

        let rel_no_ext = path_without_extension(&test_rel);
        let base_name = normalized
            .rsplit('/')
            .next()
            .and_then(|name| name.rsplit_once('.').map(|(stem, _)| stem))
            .unwrap_or("");
        let parent_rel = rel_no_ext.rsplit_once('/').map_or("", |(parent, _)| parent);
        let primary_rel = if parent_rel.is_empty() {
            format!("src/{base_name}.h")
        } else {
            format!("src/{parent_rel}/{base_name}.h")
        };
        let primary_exists = join_project_path(&root_prefix, &primary_rel).exists();
        let fallback_candidates: Vec<String> = includes
            .iter()
            .map(|include| format!("src/{include}"))
            .filter(|candidate| candidate != &primary_rel)
            .collect();
        let fallback_exists = fallback_candidates
            .iter()
            .any(|candidate| join_project_path(&root_prefix, candidate).exists());

        if !primary_exists && !fallback_exists {
            let test_full_rel = project_relative_path(&normalized).unwrap_or(test_rel.clone());
            let mut parts = vec![
                format!(
                    "Test file {} has no corresponding header in src/",
                    python_path_display(&test_full_rel)
                ),
                format!("  Expected: {}", python_path_display(&primary_rel)),
            ];
            if includes.is_empty() {
                parts.push("  No project headers included!".to_string());
            } else {
                parts.push(format!(
                    "  Includes: {}",
                    includes
                        .iter()
                        .take(3)
                        .cloned()
                        .collect::<Vec<_>>()
                        .join(", ")
                ));
                if includes.len() > 3 {
                    parts.push(format!("  ... and {} more", includes.len() - 3));
                }
            }
            return vec![(0, parts.join("\n"))];
        }

        if !primary_exists && fallback_exists && !includes.is_empty() {
            let test_dir_path = parent_rel;
            let mut any_include_matches = false;
            let mut first_mismatched_include_dir: Option<String> = None;
            for include in &includes {
                let include_dir = include.rsplit_once('/').map_or("", |(dir, _)| dir);
                if !test_dir_path.is_empty()
                    && !include_dir.is_empty()
                    && test_dir_path == include_dir
                {
                    any_include_matches = true;
                    break;
                }
                if first_mismatched_include_dir.is_none()
                    && !test_dir_path.is_empty()
                    && !include_dir.is_empty()
                    && test_dir_path != include_dir
                {
                    first_mismatched_include_dir = Some(include_dir.to_string());
                }
            }
            if !any_include_matches
                && first_mismatched_include_dir.is_some()
                && !source_mirror_dir_has_headers(&root_prefix, test_dir_path)
            {
                let include_dir = first_mismatched_include_dir.unwrap();
                let test_full_rel = project_relative_path(&normalized).unwrap_or(test_rel);
                return vec![(
                    0,
                    format!(
                        "⚠️  Test file {} may be in wrong directory:\n  Test location: tests/{test_dir_path}/\n  Includes headers from: src/{include_dir}/\n  Expected location: tests/{include_dir}/",
                        python_path_display(&test_full_rel)
                    ),
                )];
            }
        }

        Vec::new()
    }
}

struct TestIncludePathsChecker;

impl FileContentChecker for TestIncludePathsChecker {
    fn name(&self) -> &'static str {
        "TestIncludePathsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "tests")
            && ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cc", ".cxx"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let root_prefix = project_root_prefix_for_file(&file_content.path);
        let mut allowed_bare = top_level_headers(&root_prefix, "tests");
        allowed_bare.extend(top_level_headers(&root_prefix, "src"));
        let all_test_filenames = all_test_header_filenames(&root_prefix);
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }
            if line.to_lowercase().contains("ok test include") {
                continue;
            }
            let Some(capture) = regex_quoted_include_line().captures(stripped) else {
                continue;
            };
            let include_path = &capture[1];
            if include_path.starts_with("../") || include_path.starts_with("./") {
                violations.push((
                    index + 1,
                    format!(
                        "Relative include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., tests/fl/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }
            if TEST_INCLUDE_VALID_PREFIXES
                .iter()
                .any(|prefix| include_path.starts_with(prefix))
            {
                continue;
            }
            if !include_path.contains('/') && !include_path.contains('\\') {
                if allowed_bare.contains(include_path) || !all_test_filenames.contains(include_path)
                {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Bare include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., fl/... or tests/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }

            let file_dir = Path::new(&file_content.path)
                .parent()
                .map(Path::to_path_buf)
                .unwrap_or_default();
            if file_dir.join(include_path).exists() {
                violations.push((
                    index + 1,
                    format!(
                        "Sub-path include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., tests/fl/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }
            let first_component = include_path.split('/').next().unwrap_or("");
            if matches!(
                first_component,
                "fl" | "misc" | "platforms" | "shared" | "profile"
            ) && !include_path.starts_with("tests/")
            {
                violations.push((
                    index + 1,
                    format!(
                        "Include missing tests/ prefix: #include \"{include_path}\" — use #include \"tests/{include_path}\" instead. Add '// ok test include' to suppress."
                    ),
                ));
            }
        }

        violations
    }
}

struct TestPathStructureChecker;

impl FileContentChecker for TestPathStructureChecker {
    fn name(&self) -> &'static str {
        "TestPathStructureChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests")
            || !ends_with_any(&normalized, &[".cpp", ".hpp"])
        {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if TEST_PATH_EXCLUDED_FILES.contains(&basename) {
            return false;
        }
        let Some(rel) = tests_relative_path(&normalized) else {
            return false;
        };
        let parts: Vec<&str> = rel.split('/').collect();
        if parts
            .first()
            .is_some_and(|part| matches!(*part, "misc" | "profile" | "shared"))
            || parts.contains(&"test_utils")
        {
            return false;
        }
        !is_under_config_excluded_test_dir(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        let root_prefix = project_root_prefix_for_file(&normalized);
        let Some(rel_from_tests) = tests_relative_path(&normalized) else {
            return Vec::new();
        };
        let test_name_no_ext = path_without_extension(&rel_from_tests);
        let expected_h = format!("src/{test_name_no_ext}.h");
        let expected_hpp = format!("src/{test_name_no_ext}.hpp");
        let expected_cpp_hpp = format!("src/{test_name_no_ext}.cpp.hpp");
        if join_project_path(&root_prefix, &expected_h).exists()
            || join_project_path(&root_prefix, &expected_hpp).exists()
            || join_project_path(&root_prefix, &expected_cpp_hpp).exists()
        {
            return Vec::new();
        }
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.to_lowercase().contains("// ok standalone"))
        {
            return Vec::new();
        }
        let project_rel = project_relative_path(&normalized).unwrap_or_else(|| normalized.clone());
        let test_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        vec![(
            1,
            format!(
                "Test file has no corresponding source file at matching path. Test is at '{}' but no source file found at 'src/{}', 'src/{}', or 'src/{}'. \n\nREQUIRED ACTIONS (in order of preference):\n  1. RENAME the test to match the source file it's testing (best option)\n  2. MERGE this test into an existing test file that tests the same source — each test\n     file costs compile time, so consolidating into fewer files is strongly preferred\n  3. MOVE to 'tests/misc/{test_name}' if this truly doesn't test a specific source file\n\n⚠️  DO NOT add '// ok standalone' unless absolutely necessary. This amnesty is a last\nresort for rare infrastructure files that genuinely cannot be organized. AI agents\nshould NEVER add this comment — instead fix the path or consolidate tests.\n\nAvoid creating tests in 'tests/misc/' - prefer mirroring source directory structure.\nTest organization should mirror source organization for maintainability.\nNote: Source matcher checks .h, .hpp, and .cpp.hpp files.",
                python_path_display(&project_rel),
                python_path_display(&format!("{test_name_no_ext}.h")),
                python_path_display(&format!("{test_name_no_ext}.hpp")),
                python_path_display(&format!("{test_name_no_ext}.cpp.hpp")),
            ),
        )]
    }
}

struct TestAggregationChecker;

impl FileContentChecker for TestAggregationChecker {
    fn name(&self) -> &'static str {
        "TestAggregationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "tests")
            && ends_with_any(&normalized, &[".cpp", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        test_aggregation_check_single_file(&file_content.path)
            .into_iter()
            .map(|message| (0, message))
            .collect()
    }
}

#[derive(Clone, Copy)]
enum BannedHeadersScope {
    Fl,
    Lib8tion,
    FxSensorsPlatformsShared,
    Platforms,
    Examples,
    ThirdParty,
    Tests,
}

struct BannedHeadersChecker {
    banned_headers: &'static [&'static str],
    strict_mode: bool,
    scope: BannedHeadersScope,
}

impl BannedHeadersChecker {
    fn scope_matches(&self, path: &str, project_root: &Path) -> bool {
        match self.scope {
            BannedHeadersScope::Fl => is_under_project_subpath(path, project_root, "src/fl"),
            BannedHeadersScope::Lib8tion => {
                is_under_project_subpath(path, project_root, "src/lib8tion")
            }
            BannedHeadersScope::FxSensorsPlatformsShared => {
                is_under_project_subpath(path, project_root, "src/fx")
                    || is_under_project_subpath(path, project_root, "src/sensors")
                    || is_under_project_subpath(path, project_root, "src/platforms/shared")
            }
            BannedHeadersScope::Platforms => {
                is_under_project_subpath(path, project_root, "src/platforms")
            }
            BannedHeadersScope::Examples => {
                is_under_project_subpath(path, project_root, "examples")
            }
            BannedHeadersScope::ThirdParty => {
                is_under_project_subpath(path, project_root, "src/third_party")
            }
            BannedHeadersScope::Tests => is_under_project_subpath(path, project_root, "tests"),
        }
    }
}

impl FileContentChecker for BannedHeadersChecker {
    fn name(&self) -> &'static str {
        "BannedHeadersChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(&normalized) {
            return false;
        }
        self.scope_matches(&normalized, project_root)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let file_ext = last_dot_extension(&file_content.path);
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.starts_with("//") {
                continue;
            }

            if let Some(capture) = regex_private_libcpp_header().captures(line) {
                let header = capture.get(1).unwrap().as_str();
                let public_header = private_libcpp_header_recommendation(header);
                violations.push((
                    index + 1,
                    format!(
                        "Found private libc++ header \"{header}\" - Use {public_header} instead. Private libc++ headers (starting with __) are internal implementation details and should never be included directly. They are unstable across libc++ versions and break portability."
                    ),
                ));
            }

            let Some(capture) = regex_banned_header_include().captures(line) else {
                continue;
            };
            let header = capture.get(1).unwrap().as_str();
            if !self.banned_headers.contains(&header) {
                continue;
            }
            if banned_header_matches_exception(&file_content.path, header) {
                continue;
            }

            let recommendation = banned_header_recommendation(header);
            let has_bypass_comment =
                line.contains("// ok include") || line.contains("// OK include");
            if matches!(file_ext, "h" | "hpp") {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead (banned in header files). Try moving the #include to the corresponding .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace."
                    ),
                ));
            } else if self.strict_mode && !has_bypass_comment {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead (strict mode, no bypass allowed). Try moving the #include to a .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace."
                    ),
                ));
            } else if !self.strict_mode && !has_bypass_comment {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead. Try finding the equivalent header in fl/stl in the fl:: namespace, or if you need this header in a .cpp file, add '// ok include' comment to suppress this warning."
                    ),
                ));
            }
        }

        violations
    }
}

struct NamespaceIncludesChecker;

impl FileContentChecker for NamespaceIncludesChecker {
    fn name(&self) -> &'static str {
        "NamespaceIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        let lower = normalized.to_ascii_lowercase();
        if [
            ".build",
            ".pio",
            ".venv",
            "libdeps",
            "third_party",
            "vendor",
            "tests",
        ]
        .iter()
        .any(|part| lower.contains(part))
        {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .any(|line| regex_allow_include_after_namespace().is_match(line))
        {
            return Vec::new();
        }

        let mut current_namespace: Option<(usize, String)> = None;
        let mut seen_namespace = false;
        let mut seen_include_after_namespace = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") || stripped.starts_with("/*") {
                continue;
            }
            if current_namespace.is_some() && stripped.starts_with('}') {
                current_namespace = None;
            }
            if stripped.contains("using namespace")
                && regex_namespace_include_using().is_match(stripped)
            {
                current_namespace = Some((index + 1, namespace_include_snippet(line)));
                seen_namespace = true;
            }
            if stripped.contains("namespace")
                && stripped.contains('{')
                && regex_namespace_include_open().is_match(stripped)
            {
                current_namespace = Some((index + 1, namespace_include_snippet(line)));
                seen_namespace = true;
            }
            if stripped.contains("#include")
                && regex_namespace_include_directive().is_match(stripped)
            {
                if line.contains("// nolint") {
                    continue;
                }
                if current_namespace.is_some() {
                    // The Python checker stores this as a provisional violation, then replaces it
                    // with the thorough pass whenever a namespace/include pair was seen.
                }
                if seen_namespace {
                    seen_include_after_namespace = true;
                }
            }
        }

        if seen_namespace && seen_include_after_namespace {
            if !namespace_braces_are_balanced(&file_content.lines) {
                return Vec::new();
            }
            return namespace_include_thorough_violations(&file_content.lines);
        }

        Vec::new()
    }
}

struct NativePlatformDefinesChecker;

impl FileContentChecker for NativePlatformDefinesChecker {
    fn name(&self) -> &'static str {
        "NativePlatformDefinesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        if is_under_project_subpath(&normalized, project_root, "src/third_party") {
            return false;
        }

        let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        let file_name_lower = file_name.to_lowercase();
        if file_name.starts_with("is_")
            || file_name_lower.contains("core_detection")
            || file_name_lower.contains("compile_test")
            || NATIVE_COMPILER_ABSTRACTION_FILES.contains(&file_name)
        {
            return false;
        }

        let src_root_file = project_relative_path(&normalized)
            .is_some_and(|rel| rel.starts_with("src/") && rel.matches('/').count() == 1);
        if src_root_file && NATIVE_ROOT_DISPATCH_HEADERS.contains(&file_name) {
            return false;
        }
        if NATIVE_DISPATCH_CONFIG_FILES.contains(&file_name)
            && normalized.contains("/src/lib8tion/")
        {
            return false;
        }

        if is_under_project_subpath(&normalized, project_root, "src/platforms") {
            let platforms_root_file = project_relative_path(&normalized).is_some_and(|rel| {
                rel.starts_with("src/platforms/")
                    && rel["src/platforms/".len()..].matches('/').count() == 0
            });
            if platforms_root_file {
                return false;
            }
            if file_name.starts_with("fastpin_") || file_name.starts_with("fastspi_") {
                return false;
            }
            if file_name.starts_with("led_sysdefs_") && file_name.matches('_').count() <= 2 {
                return false;
            }
            if file_name_lower.contains("dispatch") || file_name == "fastpin_legacy.h" {
                return false;
            }
        }

        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !regex_preprocessor_conditional().is_match(&file_content.content) {
            return Vec::new();
        }
        if !NATIVE_TO_MODERN_DEFINES
            .iter()
            .any(|(native, _)| file_content.content.contains(native))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment {
                continue;
            }

            let stripped = line.trim();
            if stripped.starts_with("//") || !regex_preprocessor_conditional().is_match(stripped) {
                continue;
            }
            let code_part = split_line_comment(line);
            for (native_define, modern_define) in NATIVE_TO_MODERN_DEFINES {
                if !code_part.contains(native_define)
                    || !contains_ascii_word(code_part, native_define)
                {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Native platform define '{native_define}' found in preprocessor conditional. Use '#ifdef {modern_define}' or '#if defined({modern_define})' instead (FL_IS_* macros are defined/undefined, never use bare '#if {modern_define}')."
                    ),
                ));
            }
        }

        violations
    }
}

struct NoexceptSpecialMembersChecker;

impl FileContentChecker for NoexceptSpecialMembersChecker {
    fn name(&self) -> &'static str {
        "NoexceptSpecialMembersChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"]) {
            return false;
        }
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.ends_with("/noexcept.h") || normalized.ends_with("fl/stl/noexcept.h") {
            return false;
        }
        !normalized.contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let class_names: HashSet<String> = regex_noexcept_class_def()
            .captures_iter(&file_content.content)
            .map(|capture| capture[1].to_string())
            .collect();
        if class_names.is_empty() {
            return Vec::new();
        }

        let comment_mask = compute_block_comment_mask(&file_content.lines);
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") || stripped.starts_with('#') {
                continue;
            }
            if comment_mask.get(index).copied().unwrap_or(false) {
                continue;
            }
            if stripped.starts_with("/*") || stripped.starts_with('*') {
                continue;
            }
            if regex_noexcept_suppress_special().is_match(line) {
                continue;
            }

            let mut info = classify_noexcept_line(line, &class_names);
            if info.is_none() {
                if let Some(joined) = join_multiline_signature(&file_content.lines, index) {
                    if let Some((kind, open_paren)) = classify_noexcept_line(&joined, &class_names)
                    {
                        let original_open = line.find('(').unwrap_or(open_paren);
                        info = Some((kind, original_open));
                    }
                }
            }
            let Some((kind, open_paren)) = info else {
                continue;
            };
            if signature_has_noexcept(&file_content.lines, index, open_paren) {
                continue;
            }
            violations.push((
                index + 1,
                format!("Missing FL_NOEXCEPT on {kind}: {stripped}"),
            ));
        }

        violations
    }
}

struct EnumClassChecker;

impl FileContentChecker for EnumClassChecker {
    fn name(&self) -> &'static str {
        "EnumClassChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
            || normalized.contains("/examples/")
        {
            return false;
        }
        is_under_project_subpath(&normalized, project_root, "src/fl")
            || is_under_project_subpath(&normalized, project_root, "src/platforms")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut brace_depth = 0_i32;
        let mut class_struct_enter_depths: Vec<i32> = Vec::new();
        let mut pending_class_struct = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if regex_class_struct().is_match(code_part) {
                let before_brace = code_part.split('{').next().unwrap_or(code_part);
                if !before_brace.contains(';') {
                    pending_class_struct = true;
                }
            }

            for ch in code_part.chars() {
                if ch == '{' {
                    if pending_class_struct {
                        class_struct_enter_depths.push(brace_depth);
                        pending_class_struct = false;
                    }
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if class_struct_enter_depths
                        .last()
                        .is_some_and(|depth| brace_depth == *depth)
                    {
                        class_struct_enter_depths.pop();
                    }
                }
            }

            if line.contains("// ok plain enum") || line.contains("// okay plain enum") {
                continue;
            }
            if !code_part.contains("enum") || !class_struct_enter_depths.is_empty() {
                continue;
            }
            if regex_named_enum().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "Plain enum detected - use 'enum class' for type safety. Plain enums leak names into enclosing scope and allow implicit integer conversions. Suppress with '// ok plain enum' for legacy enums: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct PlatformsFlNamespaceChecker;

impl FileContentChecker for PlatformsFlNamespaceChecker {
    fn name(&self) -> &'static str {
        "PlatformsFlNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/platforms") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        if ends_with_any(&normalized, &["_build.hpp", "_build.cpp", "_build.cpp.hpp"]) {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".cpp", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content.content.contains("namespace fl")
            || file_content.content.contains("// ok no namespace fl")
        {
            return Vec::new();
        }
        vec![(
            0,
            "Missing 'namespace fl {' or '// ok no namespace fl' comment".to_string(),
        )]
    }
}

struct NamespacePlatformsChecker;

impl FileContentChecker for NamespacePlatformsChecker {
    fn name(&self) -> &'static str {
        "NamespacePlatformsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "src/platforms")
            && ends_with_any(&normalized, &[".cpp", ".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if regex_namespace_platform_singular().is_match(code_part) {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct LoggingInIramChecker;

impl FileContentChecker for LoggingInIramChecker {
    fn name(&self) -> &'static str {
        "LoggingInIramChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if is_excluded_file(&normalized) || !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        normalized.contains("/platforms/") || normalized.contains("/fl/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let full_content = file_content.lines.join("\n");
        if !full_content.contains("FL_IRAM") {
            return Vec::new();
        }

        let cleaned_content = strip_comments_preserving_lines(&file_content.lines);
        let mut violations = Vec::new();

        for function_match in regex_iram_function().captures_iter(&cleaned_content) {
            let Some(whole_match) = function_match.get(0) else {
                continue;
            };
            let func_name = function_match.get(2).map_or("", |value| value.as_str());
            let func_start = whole_match.end();
            let mut brace_depth = 1_i32;
            let mut func_body_end = func_start;
            for (offset, ch) in cleaned_content[func_start..].char_indices() {
                if ch == '{' {
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if brace_depth == 0 {
                        func_body_end = func_start + offset;
                        break;
                    }
                }
            }
            let func_body = &cleaned_content[func_start..func_body_end];

            for macro_match in regex_iram_banned_macro().captures_iter(func_body) {
                let macro_name = macro_match.get(1).unwrap().as_str();
                let match_pos = func_start + macro_match.get(0).unwrap().start();
                let line_number = cleaned_content[..match_pos].matches('\n').count() + 1;
                let original_line = file_content
                    .lines
                    .get(line_number.saturating_sub(1))
                    .map_or("", |line| line.trim());
                violations.push((
                    line_number,
                    format!(
                        "Found '{macro_name}' in FL_IRAM function '{func_name}'\n  Line: {}\n  Logging macros cannot be used in ISR functions marked with FL_IRAM.",
                        original_line.chars().take(100).collect::<String>()
                    ),
                ));
            }

            for log_match in regex_fl_log_macro().find_iter(func_body) {
                let macro_call = log_match.as_str();
                let macro_name = macro_call.split('(').next().unwrap_or(macro_call).trim();
                if macro_name.contains("ASYNC") {
                    continue;
                }
                let match_pos = func_start + log_match.start();
                let line_number = cleaned_content[..match_pos].matches('\n').count() + 1;
                let original_line = file_content
                    .lines
                    .get(line_number.saturating_sub(1))
                    .map_or("", |line| line.trim());
                violations.push((
                    line_number,
                    format!(
                        "Found '{macro_name}' in FL_IRAM function '{func_name}'\n  Line: {}\n  Logging macros cannot be used in ISR functions marked with FL_IRAM.",
                        original_line.chars().take(100).collect::<String>()
                    ),
                ));
            }
        }

        violations
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn file(path: &str, content: &str) -> FileContent {
        FileContent {
            path: normalize_path(path),
            content: content.to_string(),
            lines: content.lines().map(str::to_string).collect(),
        }
    }

    #[test]
    fn help_parse_exits_without_listing_checkers() {
        let config = CliConfig::parse(vec!["--help".to_string()]).unwrap();
        assert!(config.show_help);
        assert!(!config.list_checkers);
    }

    #[test]
    fn explicit_missing_input_is_an_error() {
        let input =
            std::env::temp_dir().join(format!("fastled_lint_missing_input_{}", std::process::id()));
        let error = collect_input_files(Path::new("."), &[path_to_string(&input)])
            .unwrap_err()
            .to_string();
        assert!(error.contains("input path not found"));
    }

    #[test]
    fn unmatched_input_pattern_is_an_error() {
        let temp_dir = normalize_path(&path_to_string(&std::env::temp_dir()));
        let input = format!(
            "{temp_dir}/fastled_lint_missing_pattern_{}/*.h",
            std::process::id()
        );
        let error = collect_input_files(Path::new("."), &[input])
            .unwrap_err()
            .to_string();
        assert!(error.contains("input pattern matched no files"));
    }

    #[test]
    fn banned_macros_ignores_string_literals() {
        let checker = BannedMacrosChecker;
        let result = checker.check_file_content(&file(
            "src/fl/example.h",
            "FL_WARN(\"use static_assert elsewhere\");",
        ));
        assert!(result.is_empty());
    }

    #[test]
    fn bare_allocation_rejects_malloc_but_not_fl_malloc() {
        let checker = BareAllocationChecker;
        assert_eq!(
            checker
                .check_file_content(&file("src/fl/example.h", "void* p = malloc(4);"))
                .len(),
            1
        );
        assert!(checker
            .check_file_content(&file("src/fl/example.h", "void* p = fl::malloc(4);"))
            .is_empty());
    }

    #[test]
    fn static_in_header_allows_template_static() {
        let checker = StaticInHeaderChecker;
        let content =
            "template<typename T>\nT& get() {\n    static T instance;\n    return instance;\n}";
        assert!(checker
            .check_file_content(&file("src/fl/example.h", content))
            .is_empty());
    }
}
