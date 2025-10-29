# ============================================================================
# SHARED PRECOMPILED HEADER (Manual PCH Management)
# ============================================================================
# Create a single PCH file shared by all tests to avoid per-target duplication.
# Meson creates one PCH per executable by default, which would create 100+ PCH
# files for identical compilation settings. This manual approach builds test.h
# once and shares it across all test executables.

# Find uv for running Zig compiler
uv = find_program('uv')

# Build precompiled header from test_pch.h (contains FastLED.h, doctest + common headers)
# Uses Zig's bundled Clang to match the compile flags (Meson detected GCC but we need Clang)
test_pch = custom_target('test_pch',
  input: 'test_pch.h',
  output: ['test_pch.h.pch', 'test_pch.h.d'],
  depfile: 'test_pch.h.d',
  command: [
    uv, 'run', 'python', '-m', 'ziglang', 'c++',
    '-x', 'c++-header',           # Treat input as C++ header
    '@INPUT@',                     # test_pch.h
    '-o', '@OUTPUT0@',             # test_pch.h.pch
    '-MD', '-MF', '@OUTPUT1@',     # Generate dependency file
    unit_test_compile_args,        # Same flags as all tests
    '-I' + meson.project_source_root() / 'src',
    '-I' + meson.project_source_root() / 'tests',
    '-I' + meson.project_source_root() / 'src/platforms/stub',
  ],
  install: false
)

# Doctest main object
doctest_main = static_library('doctest_main',
  'doctest_main.cpp',
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false
)

# Automatically discover test files using rglob from tests/ directory
# Find all test_*.cpp files recursively, excluding special files
test_cpp_files = run_command('python', '-c', '''
import sys
from pathlib import Path
tests_dir = Path(sys.argv[1])
# Exclude special files that have their own build rules or aren't standard tests
excluded = {"doctest_main.cpp", }
for f in sorted(tests_dir.rglob("test_*.cpp")):
    if f.name not in excluded:
        print(f.relative_to(tests_dir).as_posix())
''', meson.current_source_dir(), check: true).stdout().strip().split('\n')

# Build test_names list and file path mapping for subdirectories
test_names = []
test_file_paths = {}

foreach test_file : test_cpp_files
  if test_file != ''
    # Extract test name from file path
    # For noise/test_noise_X.cpp -> test_noise_X (not noise_test_noise_X)
    test_name = test_file.split('/')[-1].replace('.cpp', '')
    test_names += test_name

    # Store mapping for files in subdirectories
    if test_file.contains('/')
      test_file_paths += {test_name: test_file}
    endif
  endif
endforeach

# Check if unity builds are enabled
unity_enabled = get_option('unity') == 'on'

if unity_enabled
  # UNITY MODE: Combine all tests into single all_tests executable

  # Collect all test source files
  all_test_sources = [test_pch[0]]  # Start with PCH

  foreach test_name : test_names
    # Get test file path from dynamic map (for subdirectories) or default to test_name.cpp
    test_file = test_file_paths.get(test_name, test_name + '.cpp')
    all_test_sources += files(test_file)
  endforeach

  # Add parallel SPI ISR test
  all_test_sources += files('test_parallel_spi_isr.cpp')

  # Build single unified test executable
  # NOTE: Explicitly disable Meson's unity builds for this target to avoid
  # double unity builds (we're already combining all test files manually)
  all_tests_exe = executable('all_tests',
    all_test_sources,
    include_directories: [src_dir, tests_dir, stub_dir],
    cpp_args: unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Winvalid-pch',
    ],
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_main],
    override_options: ['unity=off'],
    install: false
  )

  # Register single test target
  test('all_tests', all_tests_exe, workdir: meson.project_source_root())

  message('Unity test build enabled: all tests combined into all_tests executable')

else
  # NORMAL MODE: Build individual test executables (current behavior)

  # Build each test executable with shared PCH
  foreach test_name : test_names
    # Get test file path from dynamic map (for subdirectories) or default to test_name.cpp
    test_file = test_file_paths.get(test_name, test_name + '.cpp')

    # Some tests need additional source files for ESP32 parallel SPI host simulation
    test_sources = [test_file, test_pch[0]]
    test_cpp_args = unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Winvalid-pch',
    ]

    # host_sim.cpp is now compiled as part of libfastled.a when STUB_PLATFORM is defined
    # No need to add it explicitly to individual tests anymore

    test_exe = executable(test_name,
      test_sources,
      include_directories: [src_dir, tests_dir, stub_dir],
      cpp_args: test_cpp_args,
      link_args: unit_test_link_args,
      link_with: [fastled_lib, doctest_main],
      install: false
    )

    # Register test with project root as working directory
    test(test_name, test_exe, workdir: meson.project_source_root())
  endforeach

  # ============================================================================
  # Parallel SPI ISR Tests
  # ============================================================================
  # Parallel SPI ISR test using thread-based automatic ISR execution.
  # Uses the same libfastled.a as all other tests. STUB_PLATFORM always compiles
  # real ISR implementations via conditional compilation guards in the source files.


  test_parallel_spi_isr_exe = executable('test_parallel_spi_isr',
    ['test_parallel_spi_isr.cpp', test_pch[0]],
    include_directories: [src_dir, tests_dir, stub_dir],
    cpp_args: unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Winvalid-pch',
    ],
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_main],
    install: false
  )

  test('test_parallel_spi_isr', test_parallel_spi_isr_exe, workdir: meson.project_source_root())

endif  # unity_enabled
