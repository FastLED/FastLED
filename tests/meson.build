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
  override_options: ['unity=off'],  # Don't apply unity builds to this single-file library
  install: false
)

# Automatically discover test files using rglob from tests/ directory
# Find all test_*.cpp files recursively, plus *.cpp files in fl/, fx/ subdirectories
test_cpp_files = run_command('python', '-c', '''
import sys
from pathlib import Path
tests_dir = Path(sys.argv[1]).resolve()
# Exclude special files that have their own build rules or aren't standard tests
# Also exclude broken tests with missing dependencies (TODO: fix these)
excluded = {"doctest_main.cpp", "test_parallel_spi_isr.cpp", "test_audio_reactive_simple.cpp", "test_beat_detector.cpp"}
# Find all test_*.cpp files recursively
for f in sorted(tests_dir.rglob("test_*.cpp")):
    if f.name not in excluded:
        rel_path = f.resolve().relative_to(tests_dir)
        print(rel_path.as_posix())
# Also find *.cpp files directly in fl/, fx/ subdirectories
for subdir in ["fl", "fx"]:
    subdir_path = tests_dir / subdir
    if subdir_path.exists():
        for f in sorted(subdir_path.glob("*.cpp")):
            rel_path = f.resolve().relative_to(tests_dir)
            print(rel_path.as_posix())
''', meson.current_source_dir(), check: true).stdout().strip().split('\n')

# Build test_names list and file path mapping for subdirectories
test_names = []
test_file_paths = {}

foreach test_file : test_cpp_files
  if test_file != ''
    # Extract test name from file path
    # For noise/test_noise_X.cpp -> test_noise_X (not noise_test_noise_X)
    # For fl/algorithm.cpp -> fl_algorithm (prefix with subdirectory name)
    # For fx/engine.cpp -> fx_engine (prefix with subdirectory name)
    if test_file.startswith('fl/')
      # Files in fl/ subdirectory: add fl_ prefix to avoid conflicts
      base_name = test_file.split('/')[-1].replace('.cpp', '')
      test_name = 'fl_' + base_name
    elif test_file.startswith('fx/')
      # Files in fx/ subdirectory: add fx_ prefix to avoid conflicts
      base_name = test_file.split('/')[-1].replace('.cpp', '')
      test_name = 'fx_' + base_name
    else
      test_name = test_file.split('/')[-1].replace('.cpp', '')
    endif
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
  # UNITY MODE: Combine tests into categorized test executables

  # Categorize tests by functionality
  fl_tests = []         # fl/ directory tests (stdlib-like utilities)
  fx_tests = []         # fx/ directory tests (effects framework)
  noise_tests = []      # noise/ directory tests
  platform_tests = []   # Platform-specific tests (SPI, ISR, ESP32, etc.)
  core_tests = []       # Core FastLED and general tests

  # Platform test patterns
  platform_patterns = [
    'spi', 'isr', 'esp32', 'riscv', 'parallel', 'quad', 'single',
    'clockless', 'dual', 'stub_led'
  ]

  foreach test_name : test_names
    # Get test file path
    test_file = test_file_paths.get(test_name, test_name + '.cpp')

    # Categorize based on prefix or file path
    if test_name.startswith('fl_')
      fl_tests += files(test_file)
    elif test_name.startswith('fx_') or test_name.contains('fx')
      fx_tests += files(test_file)
    elif test_file.startswith('noise/')
      noise_tests += files(test_file)
    else
      # Check if it's a platform test
      is_platform = false
      foreach pattern : platform_patterns
        if test_name.contains(pattern)
          is_platform = true
          break
        endif
      endforeach

      if is_platform
        platform_tests += files(test_file)
      else
        core_tests += files(test_file)
      endif
    endif
  endforeach

  # Add parallel SPI ISR test to platform tests
  platform_tests += files('test_parallel_spi_isr.cpp')

  # Build test executables for each category
  test_categories = {
    'fl_tests': fl_tests,
    'fx_tests': fx_tests,
    'noise_tests': noise_tests,
    'platform_tests': platform_tests,
    'core_tests': core_tests,
  }

  foreach category_name, test_sources : test_categories
    if test_sources.length() > 0
      # Add PCH to sources
      category_sources = [test_pch[0]] + test_sources

      # Build test executable
      # NOTE: Explicitly disable Meson's unity builds for this target to avoid
      # double unity builds (we're already combining all test files manually)
      test_exe = executable(category_name,
        category_sources,
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

      # Register test target
      test(category_name, test_exe, workdir: meson.project_source_root())

      message('Unity test category: ' + category_name + ' (' + test_sources.length().to_string() + ' tests)')
    endif
  endforeach

  message('Unity test build enabled: tests organized into categories')

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

endif  # unity_enabled
