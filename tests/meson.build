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
# Uses the same C++ compiler configured in meson_native.txt to ensure PCH compatibility
# Wrapper script fixes the depfile to reference PCH output instead of temp .obj file
cpp_compiler = meson.get_compiler('cpp')
test_pch = custom_target('test_pch',
  input: 'test_pch.h',
  output: 'test_pch.h.pch',      # Only PCH is an output; .d file is depfile only
  depfile: 'test_pch.h.d',
  command: [
    uv, 'run', 'python',
    meson.project_source_root() / 'ci/compile_pch.py',
    cpp_compiler.cmd_array(),
    '-x', 'c++-header',           # Treat input as C++ header
    '@INPUT@',                     # test_pch.h
    '-o', '@OUTPUT@',              # test_pch.h.pch
    '-MD', '-MF', '@DEPFILE@',     # Generate dependency file (will be fixed by wrapper)
    unit_test_compile_args,        # Same flags as all tests
    '-I' + meson.project_source_root() / 'src',
    '-I' + meson.project_source_root() / 'tests',
    '-I' + meson.project_source_root() / 'src/platforms/stub',
    # STRICT PCH VALIDATION FLAGS (to catch invalidation issues):
    '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
    '-verify-pch',                              # Load and verify PCH is not stale
    '-fpch-validate-input-files-content',       # Validate based on content, not just mtime
    '-fpch-instantiate-templates',              # Instantiate templates during PCH build
  ],
  install: false
)

<<<<<<< Updated upstream
# Doctest main for normal mode - compiled separately without PCH
# This creates a static library that provides the doctest main() function
=======
# Doctest main object (always exports run_tests() for DLL mode)
# TEST_DLL_MODE flag removed - doctest_main.cpp now always exports run_tests()
>>>>>>> Stashed changes
doctest_main = static_library('doctest_main',
  'doctest_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],  # Add src and stub for platform headers
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],  # No TEST_DLL_MODE for normal builds
  override_options: ['unity=off'],  # Don't unity build this single file
  install: false
)

# Doctest main for DLL mode - compiled separately without PCH to avoid namespace conflicts
# This creates a static library that provides the run_tests() export function
doctest_dll_main = static_library('doctest_dll_main',
  'doctest_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],  # Add src and stub for platform headers
  cpp_args: ['-DTEST_DLL_MODE', '-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],  # Add FASTLED_STUB_IMPL
  override_options: ['unity=off'],  # Don't unity build this single file
  install: false
)

# Build runner executable (static, generic DLL loader)
# This executable loads a test DLL and calls its run_tests() function
# It will be copied to each test directory as <test_name>.exe
runner_exe = executable('runner',
  'shared/runner.cpp',
  cpp_args: [
    '-std=c++11',
    '-O2',
    '-g',
    # Suppress warnings from Windows SDK headers
    '-Wno-pragma-pack',
  ],
  link_args: ['-static', '-static-libgcc', '-static-libstdc++'],
  override_options: ['unity=off'],  # Don't apply unity builds to runner
  install: false
)

# Discover and organize tests using external Python script
# Output format: TEST:<name>:<file_path>:<category>
test_metadata_output = run_command(
  'python', meson.current_source_dir() / 'organize_tests.py',
  meson.current_source_dir(),
  check: true
).stdout().strip()

# Parse test metadata
test_names = []
test_file_paths = {}
test_categories = {}

foreach line : test_metadata_output.split('\n')
  if line != ''
    # Parse line format: TEST:<name>:<file_path>:<category>
    parts = line.split(':')
    test_name = parts[1]
    test_file = parts[2]
    test_category = parts[3]

    test_names += test_name
    test_file_paths += {test_name: test_file}
    test_categories += {test_name: test_category}
  endif
endforeach

# Unity builds can be controlled via --unity flag in test.py
# Unity mode builds tests as categorized DLLs loaded by test_runner for faster compilation
# Normal mode builds each test as a separate executable
# FORCE unity builds OFF for tests to avoid DLL singleton issues
unity_enabled = false  # FORCED OFF - do not use get_option('unity')

if unity_enabled
  # UNITY MODE: Combine tests into categorized test executables
  # Categories are pre-computed by organize_tests.py

  # Dynamically organize test files by category
  # Build dictionary on-the-fly based on discovered categories
  category_files = {}

  foreach test_name : test_names
    test_file = test_file_paths.get(test_name, test_name + '.cpp')
    test_category = test_categories.get(test_name)

    # Initialize category list if not exists, then append
    if not category_files.has_key(test_category)
      category_files += {test_category: files(test_file)}
    else
      # Get existing list, append, and update
      existing_files = category_files.get(test_category)
      updated_files = existing_files + files(test_file)
      category_files += {test_category: updated_files}
    endif
  endforeach

  # Track all test executables for alias
  all_test_executables = []

<<<<<<< Updated upstream
  # Build test category DLLs
  test_dlls = []
=======
  # Build test DLLs for each category
>>>>>>> Stashed changes
  foreach category_name, test_sources : category_files
    if test_sources.length() > 0
      # Add PCH, test sources, and doctest_main.cpp
      # doctest_main.cpp is compiled directly into each test DLL (not as a separate static lib)
      category_sources = [test_pch] + test_sources + files('doctest_main.cpp')

<<<<<<< Updated upstream
      # Build test category as a shared library (DLL)
      # Each DLL links against libfastled.a ONLY, exports ONLY run_tests()
=======
      # Build test DLL (shared library)
      # Unity builds enabled: Meson will combine multiple .cpp files into unity
      # compilation units, significantly speeding up compilation
>>>>>>> Stashed changes
      test_dll = shared_library(category_name,
        category_sources,
        include_directories: [src_dir, tests_dir, stub_dir],
        cpp_args: unit_test_compile_args + [
          '-DTEST_DLL_MODE',                          # Mark tests as running in DLL mode
          '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
          '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
          '-fpch-validate-input-files-content',       # Validate based on content, not mtime
        ],
        link_args: unit_test_link_args,
        link_with: [fastled_lib],                     # Link ONLY libfastled.a
        gnu_symbol_visibility: 'hidden',              # Hide all symbols except explicit exports
        override_options: ['unity=off'],              # Disable unity builds to avoid symbol collisions
        install: false
      )

<<<<<<< Updated upstream
      # Track DLLs for dependencies
      test_dlls += test_dll

      message('Unity test category DLL: ' + category_name + ' (' + test_sources.length().to_string() + ' tests)')
=======
      # Deploy test (copy runner.exe + test.dll to tests/bin/<category_name>/)
      deploy_test = custom_target(
        'deploy_' + category_name,
        output: category_name + '_deployed',
        input: [runner_exe, test_dll],
        command: [
          'python', meson.project_source_root() / 'ci/deploy_test.py',
          '@INPUT0@',  # runner.exe
          '@INPUT1@',  # test.dll
          category_name,
          meson.project_source_root() / 'tests/bin',
        ],
        build_by_default: true
      )

      # Register test target - now runs the deployed executable
      test_exe_path = meson.project_source_root() / 'tests/bin' / category_name / (category_name + '.exe')
      test(category_name, deploy_test, workdir: meson.project_source_root())

      # Track for all_tests alias
      all_test_executables += deploy_test

      message('Unity test category: ' + category_name + ' (' + test_sources.length().to_string() + ' tests)')
>>>>>>> Stashed changes
    endif
  endforeach

  # Build the test runner executable (lightweight, no libfastled.a link)
  # CRITICAL: test_runner must NOT link against libfastled.a to avoid symbol duplication
  # The runner only needs standard system libraries for DLL loading and console I/O
  test_runner = executable('test_runner',
    'test_runner.cpp',
    include_directories: [tests_dir],
    cpp_args: ['-std=c++11'],  # Minimal flags for runner
    link_args: build_machine.system() == 'windows' ? ['-mconsole'] : [],  # Minimal linking
    dependencies: [],           # No FastLED dependency
    install: false
  )

  # Register single test that runs all categories via the runner
  test('all_unity_tests', test_runner,
    workdir: meson.current_build_dir(),  # Run from build dir where DLLs are
    depends: test_dlls                    # Ensure DLLs are built first
  )

  # Create alias target that builds the test runner and all DLLs
  alias_target('all_tests', test_runner, test_dlls)

  message('Unity test build enabled: tests organized into categories')

else
  # NORMAL MODE: Build individual test executables (current behavior)

  # Track all test executables for alias
  all_test_executables = []

  # Build each test executable with shared PCH
  foreach test_name : test_names
    # Get test file path from dynamic map (for subdirectories) or default to test_name.cpp
    test_file = test_file_paths.get(test_name, test_name + '.cpp')

    # Some tests need additional source files for ESP32 parallel SPI host simulation
    test_sources = [test_file, test_pch]
    test_cpp_args = unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
      '-fpch-validate-input-files-content',       # Validate based on content, not mtime
    ]

    # host_sim.cpp is now compiled as part of libfastled.a when STUB_PLATFORM is defined
    # No need to add it explicitly to individual tests anymore

    test_exe = executable(test_name,
      test_sources,
      include_directories: [src_dir, tests_dir, stub_dir],
      cpp_args: test_cpp_args,
      link_args: unit_test_link_args,
      link_with: [fastled_lib, doctest_main],
      override_options: ['unity=off'],  # Disable unity builds for tests
      install: false
    )

    # Track test executable for alias
    all_test_executables += test_exe

    # Register test with project root as working directory
    test(test_name, test_exe, workdir: meson.project_source_root())
  endforeach

  # Create alias target that builds all test executables (matches unity mode behavior)
  alias_target('all_tests', all_test_executables)

  message('Individual test build enabled: ' + test_names.length().to_string() + ' test executables')

endif  # unity_enabled
