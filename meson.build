project('fastled', 'cpp',
  version: '6.0.0',
  default_options: [
    'cpp_std=gnu++17',
    'warning_level=2',
    'werror=false',
    'buildtype=debug'
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

# Find additional sources in subdirectories
lib8tion_sources = run_command('python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/lib8tion").glob("*.cpp")))',
  check: false, capture: true)

stub_sources = run_command('python', '-c',
  'import pathlib; print("\\n".join(str(p) for p in pathlib.Path("src/platforms/stub").glob("*.cpp") if p.name != "stub_main.cpp"))',
  check: false, capture: true)

# Add found sources if any
if lib8tion_sources.returncode() == 0 and lib8tion_sources.stdout().strip() != ''
  fastled_sources += files(lib8tion_sources.stdout().strip().split('\n'))
endif

if stub_sources.returncode() == 0 and stub_sources.stdout().strip() != ''
  fastled_sources += files(stub_sources.stdout().strip().split('\n'))
endif

# Compile flags for unit tests
unit_test_compile_args = [
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
  '-O0',
  '-g0'
]

# Link arguments
unit_test_link_args = []
if build_machine.system() == 'windows'
  unit_test_link_args = ['-ldbghelp', '-lpsapi']
else
  unit_test_link_args = ['-pthread']
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
