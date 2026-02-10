# ============================================================================
# NATIVE BUILD CONFIGURATION
# ============================================================================
# This section defines all compilation and linking flags for native (non-WASM)
# builds. These settings ensure ABI compatibility between the FastLED library,
# unit tests, and examples.
#
# Configuration hierarchy:
#   base_compile_args        → Shared by all targets (ABI-critical)
#   test_specific_args       → Additional flags for unit tests only
#   unit_test_compile_args   → base + test_specific + platform-specific
#   example_compile_args     → base (same as base_compile_args)
#
#   base_link_args           → Shared platform-specific linking (stdlib, threading)
#   test_link_args           → base + test-specific additions (debug libs)
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
  '-Werror=unused-but-set-variable',  # Treat set-but-unused variables as errors
  '-Werror=unused-function',
  '-Werror=c++14-extensions',  # Enforce C++11 compatibility
  '-Werror=c++17-extensions',  # Enforce C++11 compatibility
  '-Werror=unused-result',     # Treat ignored FL_NODISCARD return values as errors
  '-Werror=switch',            # Treat missing switch cases as errors
]

# Add GCC-specific warnings (maybe-uninitialized is GCC-only, not available in Clang)
if cpp_compiler.get_id() == 'gcc'
  base_warnings += ['-Werror=maybe-uninitialized']
endif

# Group 3: Optimization and code generation flags (mode-dependent)
# Common flags present in all modes (ABI-critical)
base_common = [
  '-std=gnu++11',              # GNU C++11 standard
  '-fno-exceptions',           # Disable C++ exceptions (ABI compatibility)
  '-fno-rtti',                 # Disable runtime type information
  '-fno-strict-aliasing',      # Prevent strict aliasing optimizations
]

# Stack trace preservation flags (used in debug and quick modes only)
stack_trace_flags = [
  '-fno-omit-frame-pointer',      # Keep frame pointers for stack traces
  '-fno-optimize-sibling-calls',  # Preserve tail call frames for complete stack traces
]
# Add GCC-specific stack trace flag (not supported by Clang)
if cpp_compiler.get_id() == 'gcc'
  stack_trace_flags += ['-fno-inline-small-functions']  # Preserve function names (GCC only)
endif

# Mode-specific optimization flags
if build_mode == 'debug'
  # Debug mode: No optimization, full debug symbols, sanitizers
  base_optimization = base_common + stack_trace_flags + [
    '-O0',                      # No optimization (fastest compilation)
    '-g3',                      # Full debug symbols (variables, macros, everything)
    '-fsanitize=address',       # Address sanitizer (detect memory errors)
    '-fsanitize=undefined',     # Undefined behavior sanitizer
  ]
elif build_mode == 'quick'
  # Quick mode: Default mode for fast development iteration
  # Light optimization, minimal debug info, no sanitizers
  # Preserves function names and stack frames for proper call stack translation
  base_optimization = base_common + stack_trace_flags + [
    '-O1',                      # Light optimization (good balance of speed and debuggability)
    '-g1',                      # Minimal debug info (function names + line numbers)
  ]
elif build_mode == 'release'
  # Release mode: Optimized production build (no stack trace preservation)
  base_optimization = base_common + [
    '-O2',                      # Standard optimization (good performance, reasonable compile time)
    '-DNDEBUG',                 # Disable assertions
  ]
elif build_mode == 'profile'
  # Profile mode: Size-optimized with debug symbols for callgrind profiling
  # Matches embedded target optimization (-Os) while preserving function names
  # -gdwarf-4: Force DWARF4 for compatibility with Valgrind 3.19 (Debian Bookworm)
  base_optimization = base_common + [
    '-Os',                      # Size optimization (matches embedded targets)
    '-g',                       # Standard debug symbols (function names + lines)
    '-gdwarf-4',                # DWARF4 format (compatible with Valgrind 3.19)
  ]
else
  error('Invalid build_mode: @0@. Must be one of: debug, quick, release, profile'.format(build_mode))
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

# Profile test-specific compilation flags
# Stack trace preservation for debugger attachment (deadlock detector)
# Note: These flags have ~1-5% performance overhead but are critical for debugging hung tests.
# The overhead affects absolute timings but relative comparisons remain valid.
profile_stacktrace_args = [
  '-fno-omit-frame-pointer',      # Preserve frame pointers for complete backtraces (~1-3% overhead)
  '-fno-optimize-sibling-calls',  # Preserve tail call frames in stack traces (~0.5-1% overhead)
]

# Profile test compilation flags
# Always include debug symbols (-g) for function names in stack traces
# Conditionally include stack trace preservation flags in debug/quick modes only
profile_specific_args = ['-g']  # Always on: function names + line numbers
if build_mode == 'debug' or build_mode == 'quick'
  # Debug/quick modes: Add stack trace preservation (helps deadlock detector)
  profile_specific_args += profile_stacktrace_args
endif
# Release/profile modes: Skip stack trace flags for accurate performance measurement

# Construct final compile args for tests and examples
unit_test_compile_args = base_compile_args + test_specific_args
profile_test_compile_args = base_compile_args + profile_specific_args
example_compile_args = base_compile_args

# ============================================================================
# UNIFIED LINK CONFIGURATION
# ============================================================================
# clang-tool-chain provides uniform GNU-style behavior across all platforms.
# This allows us to use the same link flags everywhere with minimal exceptions.

# Detect platform for the few remaining differences
is_windows = build_machine.system() == 'windows'

# ============================================================================
# Detect if clang-tool-chain is being used
# ============================================================================
# clang-tool-chain provides special flags like --deploy-dependencies that
# stock clang doesn't support. We detect this by checking the compiler command.
cpp_cmd_array = cpp_compiler.cmd_array()
is_clang_tool_chain = false
foreach cmd_part : cpp_cmd_array
  if cmd_part.contains('clang-tool-chain')
    is_clang_tool_chain = true
  endif
endforeach

# ============================================================================
# Core link arguments (shared by all targets)
# ============================================================================
# NOTE: -Wl,--no-undefined is only added in quick/release modes because
# sanitizers (ASan/UBSan) have symbols resolved at runtime, not link time.
# See: https://github.com/google/sanitizers/issues/380
#
# --deploy-dependencies is a clang-tool-chain specific flag that copies
# required runtime libraries to the build directory. Stock clang doesn't
# support this flag.
core_link_args = ['-fuse-ld=lld']
if is_clang_tool_chain
  core_link_args += ['--deploy-dependencies']
endif
if build_mode != 'debug'
  core_link_args += ['-Wl,--no-undefined']
endif

# Sanitizer link flags (used in debug mode)
sanitizer_link_args = []
if build_mode == 'debug'
  sanitizer_link_args = [
    '-fsanitize=address',
    '-fsanitize=undefined',
    '-shared-libasan',  # Use shared ASan runtime (required for DLL architecture)
  ]
endif

# ============================================================================
# DLL/shared library link arguments
# ============================================================================
# Used for test DLLs and example DLLs. Minimal linking - heavy deps in runner.
dll_link_args = core_link_args + sanitizer_link_args
if is_windows
  dll_link_args += [
    '-static-libgcc',
    '-static-libstdc++',
    '-mconsole',
    '-Wl,--stack,16777216',
  ]
else
  dll_link_args += ['-rdynamic']
  # In debug mode with sanitizers, shared libraries need to allow undefined symbols
  # that will be provided by the sanitizer runtime when the runner loads them.
  # LLD by default enforces no undefined symbols even in shared libraries.
  if build_mode == 'debug'
    dll_link_args += ['-Wl,--allow-shlib-undefined']
  endif
endif

# ============================================================================
# Runner executable link arguments (shared by tests and examples)
# ============================================================================
# The runner loads DLLs and provides system libraries (pthread, dbghelp, etc.)
# In debug mode: dynamic linking (required for sanitizers)
# In quick mode: static linking with libunwind for stack traces
# In release mode: static linking without stack trace support
if build_mode == 'debug'
  # Debug mode: dynamic linking required for sanitizers
  runner_link_args = core_link_args + sanitizer_link_args + ['-pthread']
  if is_windows
    runner_link_args += ['-ldbghelp', '-lpsapi']
  else
    # Unix: Link libunwind for crash handler stack traces
    runner_link_args += ['-lunwind']
  endif
elif build_mode == 'quick'
  # Quick mode: static linking on Windows, dynamic on Unix
  # NOTE: Linux with glibc doesn't support static linking + dlopen() properly.
  # When a statically linked executable loads shared libraries via dlopen(),
  # there are conflicts with thread-local storage and other glibc internals
  # that cause SIGSEGV crashes. Use dynamic linking on Linux like macOS.
  if is_windows
    # Windows: static linking works well with LoadLibrary
    runner_link_args = core_link_args + [
      '-static',
      '-static-libgcc',
      '-static-libstdc++',
      '-pthread',
      '-ldbghelp', '-lpsapi',
    ]
  else
    # macOS and Linux: dynamic linking (glibc doesn't support static + dlopen)
    # Link libunwind for crash handler stack traces
    runner_link_args = core_link_args + ['-pthread', '-lunwind']
  endif
else
  # Release mode: static linking on Windows, dynamic on Unix
  # NOTE: Linux with glibc doesn't support static linking + dlopen() properly.
  if is_windows
    # Windows: static linking works well with LoadLibrary
    runner_link_args = core_link_args + [
      '-static',
      '-static-libgcc',
      '-static-libstdc++',
      '-pthread',
      '-ldbghelp', '-lpsapi',
    ]
  else
    # macOS and Linux: dynamic linking (glibc doesn't support static + dlopen)
    # Link libunwind for crash handler stack traces
    runner_link_args = core_link_args + ['-pthread', '-lunwind']
  endif
endif

# ============================================================================
# Runner executable compile arguments
# ============================================================================
# Shared by both test runner and example runner. Uses same optimization as rest of app.
runner_cpp_args = [
  '-std=c++11',
  '-DFASTLED_STUB_IMPL',
  '-Wno-pragma-pack',
]
if build_mode == 'debug'
  runner_cpp_args += stack_trace_flags + [
    '-O0',
    '-g3',
    '-fsanitize=address',
    '-fsanitize=undefined',
  ]
elif build_mode == 'quick'
  runner_cpp_args += stack_trace_flags + [
    '-O1',
    '-g1',
  ]
elif build_mode == 'release'
  runner_cpp_args += [
    '-O2',
    '-DNDEBUG',
  ]
elif build_mode == 'profile'
  runner_cpp_args += [
    '-Os',
    '-g',
    '-gdwarf-4',
  ]
endif

# ============================================================================
# Legacy aliases for backward compatibility
# ============================================================================
unit_test_link_args = dll_link_args

# ============================================================================
# libunwind for enhanced stack traces (Unix only)
# ============================================================================
# libunwind provides detailed stack unwinding for crash handlers on Unix systems
# Always enabled on Unix - explicitly linked via -lunwind in runner_link_args
message('libunwind enabled for Unix platforms - enhanced stack traces enabled')
base_defines += ['-DUSE_LIBUNWIND']

# ============================================================================
# SHARED CRASH HANDLER (used by both test runner and example runner)
# ============================================================================
# Build crash handler library once - provides crash handler setup that runs
# BEFORE any DLL is loaded. Links to dbghelp/psapi on Windows.
# On Unix, libunwind is explicitly linked via -lunwind in runner_link_args.
crash_handler_lib = static_library('crash_handler',
  files(meson.project_source_root() / 'tests' / 'shared' / 'crash_handler_main.cpp'),
  include_directories: [tests_dir, src_dir, stub_dir],
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL', '-DUSE_LIBUNWIND'],
  install: false
)

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
