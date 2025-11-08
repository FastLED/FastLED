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

# Compile flags for unit tests (base flags)
unit_test_compile_args = [
  '-std=gnu++11',                # GNU C++11 standard (must be first to override Meson's default)
  '-DFASTLED_UNIT_TEST=1',
  '-DFASTLED_USE_PROGMEM=0',
  '-DSTUB_PLATFORM',
  '-DARDUINO=10808',
  '-DFASTLED_USE_STUB_ARDUINO',
  '-DSKETCH_HAS_LOTS_OF_MEMORY=1',
  '-DFASTLED_STUB_IMPL',
  '-DFASTLED_USE_JSON_UI=1',
  '-DFASTLED_TESTING',
  '-DFASTLED_NO_AUTO_NAMESPACE',
  '-DFASTLED_NO_PINMAP',
  '-DHAS_HARDWARE_PIN_SUPPORT',
  '-DFASTLED_DEBUG_LEVEL=1',
  '-DFASTLED_NO_ATEXIT=1',
  '-DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS',
  '-DENABLE_CRASH_HANDLER',
  '-DRELEASE=1',
  '-DFASTLED_FORCE_NAMESPACE=1',
  '-fpermissive',
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
  '-Werror=unused-result',  # Treat ignored FL_NODISCARD return values as errors
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

# Platform-specific compiler flags
if build_machine.system() == 'windows'
  # Windows-specific (Clang targeting x86_64-windows-gnu)
  unit_test_compile_args += [
    '-DNOMINMAX',                   # Prevent Windows min/max macros
    '-DWIN32_LEAN_AND_MEAN',        # Reduce Windows header conflicts
    '--target=x86_64-windows-gnu',  # Explicit GNU target for MSYS2/MinGW compatibility
    '-fuse-ld=lld-link',            # Use lld-link (MSVC-compatible linker)
  ]
endif
# Note: -fuse-ld=lld removed for Unix/Linux - zig uses its own bundled lld and ignores this flag

# Link arguments (Clang only, targeting x86_64-windows-gnu)
unit_test_link_args = []
if build_machine.system() == 'windows'
  unit_test_link_args = [
    '-mconsole',              # Console application
    '-nodefaultlibs',         # Don't automatically include default libraries
    '-unwindlib=libunwind',   # Use Clang's libunwind (avoid MinGW unwind)
    '-nostdlib++',            # Don't auto-link standard C++ library
    '-lc++',                  # Manually link libc++ (Clang's C++ standard library)
    '-lkernel32',             # Windows kernel functions
    '-luser32',               # Windows user interface
    '-lgdi32',                # Graphics device interface
    '-ladvapi32',             # Advanced Windows API
    '-ldbghelp',              # Debug helper (for stack traces)
    '-lpsapi',                # Process status API
  ]
else
  # Unix/Linux/macOS: Standard pthread linking
  unit_test_link_args = [
    '-pthread',
    '-rdynamic',  # Export symbols for backtrace_symbols() in crash handler
  ]
  # Note: -fuse-ld=lld removed - zig uses its own bundled lld and ignores this flag
endif

# Debug mode: Add sanitizer flags to linker when debug_mode option is enabled
if get_option('debug_mode')
  unit_test_link_args += [
    '-fsanitize=address',         # Address sanitizer runtime
    '-fsanitize=undefined',       # Undefined behavior sanitizer runtime
  ]
endif

# Build platforms/shared sources with unity disabled (each file gets its own .o)
# This prevents weak defaults from being combined with stub overrides in unity builds
shared_objects = static_library('fastled_shared',
  shared_sources,
  include_directories: [src_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false,
  override_options: ['unity=off']  # Each shared file compiles to its own .o
)

# Build main FastLED static library for tests
# Includes objects from both unity builds (fastled_sources) and non-unity (shared_objects)
fastled_lib = static_library('fastled',
  fastled_sources,
  include_directories: [src_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false,
  objects: shared_objects.extract_all_objects()  # Include shared .o files in libfastled.a
)

# Build tests subdirectory
subdir('tests')

# Build examples subdirectory (provides meson targets for example compilation)
subdir('examples')
