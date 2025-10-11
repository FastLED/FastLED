# FastLED Unit Tests Build Configuration

# Get tests root
tests_root = proj_root / 'tests'

# Use run_command to glob test files (excluding test_example_compilation.cpp which has its own main)
find_prog = find_program('python3', 'python', required: true)
result = run_command(
  find_prog, '-c',
  'import sys; from pathlib import Path; p = Path(sys.argv[1]); print(";".join(str(f.relative_to(sys.argv[2])) for f in p.glob("test_*.cpp") if f.name != "test_example_compilation.cpp"))',
  tests_root,
  proj_root,
  check: true
)
test_srcs_relative = result.stdout().strip().split(';')

# Prepend ../../../ to make paths relative to meson source dir (ci/meson/tests)
test_srcs = []
foreach src : test_srcs_relative
  test_srcs += '../../../' + src
endforeach

# Add doctest main manually (relative path from ci/meson/tests)
test_srcs += ['../../../tests/doctest_main.cpp']

# Test include directories (relative path from ci/meson/tests)
test_inc = include_directories('../../../tests')

# Test-specific defines (additional to platform defines from fastled_dep)
test_defines = [
  '-DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS',
  # Crash handler disabled - causes DLL dependency issues on Windows
  # '-DENABLE_CRASH_HANDLER',
]

# Test-specific compiler arguments
# Note: Sanitizers are disabled on Windows due to lack of support in MinGW
test_cpp_args = test_defines
if host_machine.system() != 'windows'
  test_cpp_args += [
    '-fsanitize=address',
    '-fsanitize=undefined',
    '-fno-omit-frame-pointer',
    '-fno-optimize-sibling-calls',
  ]
endif

# Test-specific linker arguments
test_link_args = []
if host_machine.system() != 'windows'
  test_link_args += [
    '-fsanitize=address',
    '-fsanitize=undefined',
  ]
endif

# Platform-specific link arguments
if host_machine.system() == 'linux'
  test_link_args += ['-pthread', '-lm', '-ldl', '-lunwind', '-lunwind-x86_64']
elif host_machine.system() == 'windows'
  # Windows-specific linking (MinGW/MSYS2)
  # Static link C++ runtime to avoid DLL dependency issues
  test_link_args += ['-static-libgcc', '-static-libstdc++']
elif host_machine.system() == 'darwin'
  # macOS-specific linking
  test_link_args += ['-pthread']
endif

# Build test executable
test_exe = executable(
  'fastled_tests',
  test_srcs,
  include_directories : [test_inc],
  dependencies : fastled_dep,
  cpp_args : test_cpp_args,
  link_args : test_link_args,
  install : false
)

# Register test with Meson
# Set FASTLED_PROJECT_ROOT environment variable to help tests find data files
test('fastled_unit_tests', test_exe, env: {'FASTLED_PROJECT_ROOT': proj_root})
