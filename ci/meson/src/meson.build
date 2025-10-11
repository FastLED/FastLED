# FastLED Library Build Configuration

# Get source root (project root is ci/meson, we need to go up 2 levels)
src_root = proj_root / 'src'

# Use run_command to glob source files (fs.glob is not available in older Meson)
# Get absolute path for the Python command, then convert back to relative for Meson
# Include all .cpp files in src/ directory (top-level and subdirectories)
find_prog = find_program('python3', 'python', required: true)
result = run_command(
  find_prog, '-c',
  'import sys; from pathlib import Path; print(";".join(str(f.relative_to(sys.argv[2])) for f in Path(sys.argv[1]).rglob("*.cpp")))',
  src_root,
  proj_root,
  check: true
)
fl_srcs_relative = result.stdout().strip().split(';')

# Prepend ../../../ to make paths relative to meson source dir (ci/meson/src)
fl_srcs = []
foreach src : fl_srcs_relative
  fl_srcs += '../../../' + src
endforeach

# Include directories (relative path from ci/meson/src)
fl_inc = include_directories(
  '../../../src',
  '../../../src/platforms/stub',  # Arduino.h stub for unit tests
  '../../../src/third_party',  # Third-party libraries
  '../../../src/third_party/libnsgif/include',
  '../../../src/third_party/TJpg_Decoder/src',
  '../../../src/third_party/pl_mpeg/src'
)

# Platform-specific defines
platform_defines = [
  '-DSTUB_PLATFORM',
  '-DFASTLED_UNIT_TEST=1',
  '-DFASTLED_FORCE_NAMESPACE=1',
  '-DFASTLED_TESTING=1',
  '-DFASTLED_USE_PROGMEM=0',
  '-DARDUINO=10808',
  '-DFASTLED_USE_STUB_ARDUINO',
  '-DSKETCH_HAS_LOTS_OF_MEMORY=1',
  '-DFASTLED_STUB_IMPL',
  '-DFASTLED_USE_JSON_UI=1',
  '-DFASTLED_NO_AUTO_NAMESPACE',
  '-DFASTLED_NO_PINMAP',
  '-DHAS_HARDWARE_PIN_SUPPORT',
  '-DFASTLED_DEBUG_LEVEL=1',
  '-DFASTLED_NO_ATEXIT=1',
]

# Compiler arguments
fl_cpp_args = [
  '-std=gnu++17',
  '-fpermissive',
  '-Wall',
  '-Wextra',
  '-Wno-deprecated-register',
  '-Wno-backslash-newline-escape',
  '-fno-exceptions',
  '-fno-rtti',
  '-Werror=unused-variable',
  '-Werror=return-type',
  '-fno-omit-frame-pointer',
  '-fno-threadsafe-statics',
  '-g0',  # No debug symbols for fast builds
] + platform_defines

# Get options
use_unity = get_option('fastled_unity').enabled()
unity_size_opt = get_option('fastled_unity_size')

# Build static library libfastled.a - shared between unit tests and example tests
# Note: Unity builds are not yet fully supported in this Meson version, so we'll compile normally
fastled_lib = static_library(
  'fastled',
  fl_srcs,
  include_directories : fl_inc,
  cpp_args : fl_cpp_args,
  install : false  # CI: don't install, just build
)

# Export dependency for tests
fastled_dep = declare_dependency(
  link_with : fastled_lib,
  include_directories : fl_inc,
  compile_args : platform_defines
)
