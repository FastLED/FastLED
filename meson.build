project('fastled', 'cpp',
  version: '6.0.0',
  meson_version: '>=1.1.0',
  default_options: [
    'cpp_std=c++11',  # Use c++11 to match AVR builds
    'warning_level=2',
    'werror=false',
    'buildtype=debug',
    'unity=off',      # Unity builds disabled by default (use --unity flag to enable)
    'unity_size=40'   # Combine 40 translation units per unity build (default is 4)
  ]
)

# Source directory
src_dir = include_directories('src')
tests_dir = include_directories('tests')

# Platform-specific stub include
stub_dir = include_directories('src/platforms/stub')

# Recursively discover all .cpp source files in src/ directory
# Note: Meson doesn't have native rglob, so we use Python's pathlib
# All files should always be included, no exceptions.
# If there is a build issue then the cpp file does not have
# the right #if-def guard in it to prevent compilation
uv_prog = find_program('uv', required: true)

python_rglob_script = '''
import pathlib
import sys
import os
directory = sys.argv[1]
pattern = sys.argv[2]
exclude_path = sys.argv[3] if len(sys.argv) > 3 else None
matches = pathlib.Path(directory).rglob(pattern)
for p in matches:
    # Normalize path for comparison (use forward slashes)
    path_str = str(p).replace(os.sep, "/")
    if exclude_path is None or exclude_path not in path_str:
        print(str(p))
'''

# Discover platforms/shared sources separately for unity build isolation
shared_sources_result = run_command(
  uv_prog, 'run', 'python', '-c', python_rglob_script, 'src/platforms/shared', '*.cpp',
  check: true,
  capture: true
)
shared_sources = files(shared_sources_result.stdout().strip().split('\n'))

# Discover all other sources (excluding platforms/shared to avoid duplicates)
discovered_sources = run_command(
  uv_prog, 'run', 'python', '-c', python_rglob_script, 'src', '*.cpp', 'platforms/shared',
  check: true,
  capture: true
)
fastled_sources = files(discovered_sources.stdout().strip().split('\n'))

# ============================================================================
# CENTRALIZED COMPILATION AND LINK CONFIGURATION
# ============================================================================
# This section defines all compilation and linking flags used throughout the
# project. These settings ensure ABI compatibility between the FastLED library,
# unit tests, and examples.
#
# Configuration hierarchy:
#   base_compile_args        → Shared by all targets (ABI-critical)
#   test_specific_args       → Additional flags for unit tests only
#   example_specific_args    → Additional flags for examples only
#   unit_test_compile_args   → base + test_specific + platform-specific
#   example_compile_args     → base + example_specific + platform-specific
#
#   base_link_args           → Shared platform-specific linking (stdlib, threading)
#   test_link_args           → base + test-specific additions (debug libs)
#   example_link_args        → base + example-specific additions
# ============================================================================

# Base compilation flags shared between unit tests and examples
# CRITICAL: These flags ensure ABI compatibility between libfastled.a and executables
base_compile_args = [
  '-std=gnu++11',                # GNU C++11 standard (must be first to override Meson's default)
  '-DFASTLED_USE_PROGMEM=0',
  '-DSTUB_PLATFORM',
  '-DARDUINO=10808',
  '-DFASTLED_USE_STUB_ARDUINO',
  '-DSKETCH_HAS_LOTS_OF_MEMORY=1',
  '-DFASTLED_STUB_IMPL',
  '-DFASTLED_TESTING',           # CRITICAL: Controls InlinedMemoryBlock layout (adds __data pointer)
  '-DFASTLED_STUB_MAIN_FAST_EXIT',  # Fast exit from stub_main after 5 loop iterations
  '-DFASTLED_NO_AUTO_NAMESPACE',
  '-DFASTLED_NO_PINMAP',
  '-DHAS_HARDWARE_PIN_SUPPORT',
  '-DFASTLED_DEBUG_LEVEL=1',
  '-DFASTLED_NO_ATEXIT=1',
  '-DENABLE_CRASH_HANDLER',
  '-Wall',
  '-Wextra',
  '-Wno-deprecated-register',
  '-Wno-backslash-newline-escape',
  '-Wno-narrowing',
  '-fno-exceptions',
  '-fno-rtti',
  '-fno-omit-frame-pointer',
  '-fno-strict-aliasing',
  '-Werror=unused-variable',
  '-Werror=unused-function',
  '-Werror=c++14-extensions',  # Enforce C++11 compatibility
  '-Werror=c++17-extensions',  # Enforce C++11 compatibility
  '-Werror=unused-result',     # Treat ignored FL_NODISCARD return values as errors
  '-O0',
  '-g1'  # Minimal debug info for stack traces (function names + line numbers)
]

# Debug mode: Add full debug symbols and sanitizers when debug_mode option is enabled
if get_option('debug_mode')
  unit_test_compile_args += [
    '-g3',                        # Full debug symbols (override -g1)
    '-fsanitize=address',         # Address sanitizer (detect memory errors)
    '-fsanitize=undefined',       # Undefined behavior sanitizer
  ]
endif

# Unit test-specific compilation flags (only used for test executables)
test_specific_args = [
  '-DFASTLED_UNIT_TEST=1',
  '-DFASTLED_USE_JSON_UI=1',
  '-DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS',
]

# Example-specific compilation flags (reserved for future use)
# Currently empty - examples use only base_compile_args
example_specific_args = []

# Platform-specific base link arguments (shared by both tests and examples)
base_link_args = []
if build_machine.system() == 'windows'
  # Windows: Clang targeting x86_64-windows-gnu with custom stdlib
  base_link_args = [
    '-mconsole',              # Console application
    '-nodefaultlibs',         # Don't automatically include default libraries
    '-unwindlib=libunwind',   # Use Clang's libunwind (avoid MinGW unwind)
    '-nostdlib++',            # Don't auto-link standard C++ library
    '-lc++',                  # Manually link libc++ (Clang's C++ standard library)
    '-lkernel32',             # Windows kernel functions
    '-luser32',               # Windows user interface
    '-lgdi32',                # Graphics device interface
    '-ladvapi32',             # Advanced Windows API
    '-Wl,--stack,16777216',   # 16MB stack (for large arrays in examples/tests)
  ]
else
  # Unix/Linux/macOS: Standard pthread linking
  base_link_args = [
    '-pthread',               # Enable POSIX threads
    '-rdynamic',              # Export symbols for backtrace_symbols() in crash handler
  ]
endif

# Test-specific link arguments (adds debug libraries for crash handling)
test_link_args = base_link_args
if build_machine.system() == 'windows'
  test_link_args += [
    '-ldbghelp',              # Debug helper library (for stack traces in crash handler)
    '-lpsapi',                # Process status API (for memory info in crash handler)
  ]
endif

# Build FastLED static library for tests
fastled_lib = static_library('fastled',
  fastled_sources,
  include_directories: [src_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false,
  objects: shared_objects.extract_all_objects()  # Include shared unity .o in libfastled.a
)

# Build tests subdirectory
subdir('tests')

# Build examples subdirectory (provides meson targets for example compilation)
subdir('examples')
