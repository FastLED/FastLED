project('fastled', 'cpp',
  version: '6.0.0',
  default_options: [
    'cpp_std=c++11',  # Use c++11 to match AVR builds
    'warning_level=2',
    'werror=false',
    'buildtype=debug',
    'unity=on'       # Enable unity builds for faster compilation (default: 20 files per chunk)
  ]
)

# Source directory
src_dir = include_directories('src')
tests_dir = include_directories('tests')

# Platform-specific stub include
stub_dir = include_directories('src/platforms/stub')

# FastLED library sources
fastled_sources = files(
  'src/FastLED.cpp',
  'src/bitswap.cpp',
  'src/cled_controller.cpp',
  'src/colorpalettes.cpp',
  'src/crgb.cpp',
  'src/hsv2rgb.cpp',
  'src/lib8tion.cpp',
  'src/noise.cpp',
  'src/platforms.cpp',
  'src/power_mgt.cpp',
  'src/simplex.cpp',
  'src/transpose8x1_noinline.cpp',
  'src/wiring.cpp',
  'src/fastled_delay.cpp'
)

# Collect all .cpp files recursively from src directory
# This mimics what the Python build system does
fs = import('fs')

# Find uv for running Python commands
uv_prog = find_program('uv', required: true)

# Find additional sources in subdirectories
lib8tion_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/lib8tion").glob("*.cpp")))',
  check: false, capture: true)

stub_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/platforms/stub").glob("*.cpp")))',
  check: false, capture: true)

platforms_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/platforms").glob("*.cpp")))',
  check: false, capture: true)

fl_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/fl").rglob("*.cpp")))',
  check: false, capture: true)

shared_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/platforms/shared").rglob("*.cpp")))',
  check: false, capture: true)

esp_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/platforms/esp").rglob("*.cpp")))',
  check: false, capture: true)

third_party_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/third_party").rglob("*.cpp")))',
  check: false, capture: true)

codec_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/fl/codec").glob("*.cpp")))',
  check: false, capture: true)

fx_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/fx").rglob("*.cpp")))',
  check: false, capture: true)

sensors_sources = run_command(uv_prog, 'run', 'python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/sensors").glob("*.cpp")))',
  check: false, capture: true)

# Add found sources if any
if lib8tion_sources.returncode() == 0 and lib8tion_sources.stdout().strip() != ''
  fastled_sources += files(lib8tion_sources.stdout().strip().split('\n'))
endif

if stub_sources.returncode() == 0 and stub_sources.stdout().strip() != ''
  fastled_sources += files(stub_sources.stdout().strip().split('\n'))
endif

if platforms_sources.returncode() == 0 and platforms_sources.stdout().strip() != ''
  fastled_sources += files(platforms_sources.stdout().strip().split('\n'))
endif

if fl_sources.returncode() == 0 and fl_sources.stdout().strip() != ''
  fastled_sources += files(fl_sources.stdout().strip().split('\n'))
endif

if shared_sources.returncode() == 0 and shared_sources.stdout().strip() != ''
  fastled_sources += files(shared_sources.stdout().strip().split('\n'))
endif

if esp_sources.returncode() == 0 and esp_sources.stdout().strip() != ''
  fastled_sources += files(esp_sources.stdout().strip().split('\n'))
endif

if third_party_sources.returncode() == 0 and third_party_sources.stdout().strip() != ''
  fastled_sources += files(third_party_sources.stdout().strip().split('\n'))
endif

if codec_sources.returncode() == 0 and codec_sources.stdout().strip() != ''
  fastled_sources += files(codec_sources.stdout().strip().split('\n'))
endif

if fx_sources.returncode() == 0 and fx_sources.stdout().strip() != ''
  fastled_sources += files(fx_sources.stdout().strip().split('\n'))
endif

if sensors_sources.returncode() == 0 and sensors_sources.stdout().strip() != ''
  fastled_sources += files(sensors_sources.stdout().strip().split('\n'))
endif

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
  '-O0',
  '-g'
]

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

# Build FastLED static library for tests
fastled_lib = static_library('fastled',
  fastled_sources,
  include_directories: [src_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false
)

# Build tests subdirectory
subdir('tests')

# Build examples subdirectory (provides meson targets for example compilation)
subdir('examples')
