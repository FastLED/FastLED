project('fastled', 'cpp',
  version: '6.0.0',
  meson_version: '>=1.4.0',
  default_options: [
    'cpp_std=c++11',  # Use c++11 to match AVR builds
    'warning_level=2',
    'werror=false'
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

# Discover ALL sources in one call
discovered_sources = run_command(
  uv_prog, 'run', 'python', 'ci/meson/rglob.py', 'src', '*.cpp',
  check: true,
  capture: true
)
all_sources = discovered_sources.stdout().strip().split('\n')

# Separate platforms/shared sources (to avoid weak/strong symbol conflicts)
platforms_shared_sources = []
fastled_sources_list = []

foreach src : all_sources
  # Check for both forward and backslash paths (Windows uses backslash)
  if src.contains('platforms/shared/') or src.contains('platforms\\shared\\')
    platforms_shared_sources += src
  else
    fastled_sources_list += src
  endif
endforeach

fastled_sources = files(fastled_sources_list)
platforms_shared_files = files(platforms_shared_sources)

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

# ============================================================================
# Base compilation flags - organized into three groups for clarity
# ============================================================================
# CRITICAL: These flags ensure ABI compatibility between libfastled.a and executables

# Group 1: Preprocessor defines (platform and feature configuration)
base_defines = [
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
]

# Group 2: Warning and error flags (code quality enforcement)
base_warnings = [
  '-Wall',
  '-Wextra',
  '-Wno-deprecated-register',
  '-Wno-backslash-newline-escape',
  '-Wno-narrowing',
  '-Wno-macro-redefined',      # Suppress redefinition warnings (libc++ hardening mode conflict)
  '-Werror=unused-variable',
  '-Werror=unused-function',
  '-Werror=c++14-extensions',  # Enforce C++11 compatibility
  '-Werror=c++17-extensions',  # Enforce C++11 compatibility
  '-Werror=unused-result',     # Treat ignored FL_NODISCARD return values as errors
]

# Add GCC-specific warnings (maybe-uninitialized is GCC-only, not available in Clang)
cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'gcc'
  base_warnings += ['-Werror=maybe-uninitialized']
endif

# Group 3: Optimization and code generation flags (mode-dependent)
# Common flags present in all modes (ABI-critical)
base_common = [
  '-std=gnu++11',              # GNU C++11 standard
  '-fno-exceptions',           # Disable C++ exceptions (ABI compatibility)
  '-fno-rtti',                 # Disable runtime type information
  '-fno-omit-frame-pointer',   # Keep frame pointers for stack traces
  '-fno-strict-aliasing',      # Prevent strict aliasing optimizations
]

# Mode-specific optimization flags
build_mode = get_option('build_mode')
if build_mode == 'debug'
  # Debug mode: No optimization, full debug symbols, sanitizers
  base_optimization = base_common + [
    '-O0',                      # No optimization (fastest compilation)
    '-g3',                      # Full debug symbols (variables, macros, everything)
    '-fsanitize=address',       # Address sanitizer (detect memory errors)
    '-fsanitize=undefined',     # Undefined behavior sanitizer
  ]
elif build_mode == 'quick'
  # Quick mode: Default mode for fast development iteration
  # Minimal optimization, minimal debug info, no sanitizers
  base_optimization = base_common + [
    '-O0',                      # No optimization (fast compilation)
    '-g1',                      # Minimal debug info (function names + line numbers)
  ]
elif build_mode == 'release'
  # Release mode: Maximum optimization, no debug info
  base_optimization = base_common + [
    '-O3',                      # Maximum optimization
    '-DNDEBUG',                 # Disable assertions
  ]
else
  error('Invalid build_mode: @0@. Must be one of: debug, quick, release'.format(build_mode))
endif

# Windows-specific workaround for Clang 19 AVX-512 intrinsics bugs
# Prevent immintrin.h from including buggy AVX-512 headers that use unavailable builtins
windows_avx_workaround = []
if build_machine.system() == 'windows'
  windows_avx_workaround = [
    '-D__AVX512BITALG__',
    '-D__AVX512VPOPCNTDQ__',
  ]
endif

# Assemble base_compile_args from organized groups
base_compile_args = base_optimization + base_defines + base_warnings + windows_avx_workaround

# Unit test-specific compilation flags (only used for test executables)
test_specific_args = [
  '-DFASTLED_UNIT_TEST=1',
  '-DFASTLED_USE_JSON_UI=1',
  '-DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS',
  '-DFASTLED_DEBUG_STACK_TRACE',
]

# Example-specific compilation flags (reserved for future use)
# Currently empty - examples use only base_compile_args
example_specific_args = []

# Construct final compile args for tests and examples
unit_test_compile_args = base_compile_args + test_specific_args
example_compile_args = base_compile_args + example_specific_args

# ============================================================================
# Platform-specific link arguments
# ============================================================================
# Force LLD linker on all platforms for consistent behavior.
# LLD supports the same flags across Windows, Linux, and macOS.

# Detect platform
is_windows = build_machine.system() == 'windows'

# Common link arguments for all platforms (LLD is forced via -fuse-ld=lld)
# This ensures consistent linker behavior across Windows, Linux, and macOS.
common_link_args = [
  '-fuse-ld=lld',             # CRITICAL: Force LLD linker on all platforms
  '-Wl,--no-undefined',       # Catch undefined symbols at link time
]

# Base link arguments (shared foundation for all link types)
if is_windows
  # Windows-specific base flags
  base_link_args = common_link_args + [
    '-mconsole',                # Console application subsystem
    '-lpthread',                # POSIX threads (winpthreads) - required for thread_local.h
    '-Wl,--stack,16777216',     # 16MB stack (for large arrays in examples/tests)
  ]
else
  # Unix (Linux/macOS) base flags - same for both with LLD
  base_link_args = common_link_args + [
    '-pthread',                 # Enable POSIX threads
    '-rdynamic',                # Export symbols for backtrace_symbols() in crash handler
  ]
endif

# Test-specific link arguments (includes debug libraries on Windows)
if is_windows
  test_link_args = base_link_args + [
    '-ldbghelp',                # Debug helper library (for stack traces in crash handler)
    '-lpsapi',                  # Process status API (for memory info in crash handler)
  ]
else
  # Unix uses same flags for all link types
  test_link_args = base_link_args
endif

# DLL-specific link arguments: Minimize linking by moving libraries to runner.exe
# This optimization moves library linking from N test DLLs (176×) to runner.exe (1×)
# Moved to runner.exe:
#   - pthread (POSIX threads) - required by FastLED
#   - dbghelp (Windows debug helper) - for crash handler stack traces
#   - psapi (Windows process API) - for crash handler memory info
#   - libunwind (Unix) - for enhanced stack traces
# Result: ~10-15% build time improvement
if is_windows
  # Windows: Minimal DLL linking (crash handler moved to runner.exe)
  dll_link_args = common_link_args + [
    '-static-libgcc',           # Statically link libgcc
    '-static-libstdc++',        # Statically link libstdc++
    '-mconsole',                # Console application subsystem
    '-Wl,--stack,16777216',     # 16MB stack (for large arrays in examples/tests)
  ]
else
  # Unix (Linux/macOS): Minimal DLL linking with LLD
  dll_link_args = common_link_args + [
    '-rdynamic',                # Export symbols for backtrace_symbols() in crash handler
  ]
endif

# Legacy alias for backward compatibility with tests/meson.build
# NOTE: This now points to dll_link_args (optimized), not test_link_args (full)
unit_test_link_args = dll_link_args

# Example-specific link arguments (currently same as base on all platforms)
example_link_args = base_link_args

# Add sanitizer flags to link arguments when in debug mode
if build_mode == 'debug'
  test_link_args += [
    '-fsanitize=address',       # Link AddressSanitizer runtime
    '-fsanitize=undefined',     # Link UndefinedBehaviorSanitizer runtime
  ]
  dll_link_args += [
    '-fsanitize=address',       # Link AddressSanitizer runtime
    '-fsanitize=undefined',     # Link UndefinedBehaviorSanitizer runtime
  ]
  unit_test_link_args += [
    '-fsanitize=address',       # Link AddressSanitizer runtime
    '-fsanitize=undefined',     # Link UndefinedBehaviorSanitizer runtime
  ]
  example_link_args += [
    '-fsanitize=address',       # Link AddressSanitizer runtime
    '-fsanitize=undefined',     # Link UndefinedBehaviorSanitizer runtime
  ]
endif

# ============================================================================
# Detect libunwind library for enhanced stack traces (Unix only)
# ============================================================================
# libunwind provides detailed stack unwinding for crash handlers on Unix systems
# When found, enables USE_LIBUNWIND define for crash_handler_libunwind.h
libunwind_dep = dependency('libunwind', required: false)
if libunwind_dep.found()
  message('libunwind found - enhanced stack traces enabled')
  base_defines += ['-DUSE_LIBUNWIND']
else
  message('libunwind not found - using fallback crash handler')
endif

# Build main FastLED static library (includes ALL sources EXCEPT platforms/shared)
# platforms/shared files are intentionally excluded to avoid weak/strong symbol conflicts
# CRITICAL ABI REQUIREMENT: Must use base_compile_args (NOT unit_test_compile_args)
# to ensure binary compatibility with both unit tests and examples.
# Unit tests add their specific flags when compiling test sources.

# Compile platforms/shared files separately
platforms_shared_lib = static_library('platforms_shared',
  platforms_shared_files,
  include_directories: [src_dir, stub_dir],
  cpp_args: base_compile_args,
  install: false
)

fastled_lib = static_library('fastled',
  fastled_sources,
  link_with: platforms_shared_lib, # Link against the shared platforms library
  include_directories: [src_dir, stub_dir],
  cpp_args: base_compile_args,
  install: false
)




# Build tests subdirectory
subdir('tests')

# Build examples subdirectory (provides meson targets for example compilation)
subdir('examples')
