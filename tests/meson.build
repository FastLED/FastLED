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

# Doctest main for normal mode - compiled separately without PCH
# This creates a static library that provides the doctest main() function
doctest_main = static_library('doctest_main',
  'doctest_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],  # Add src and stub for platform headers
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],  # No TEST_DLL_MODE for normal builds
  install: false
)

# Doctest main for DLL mode - compiled separately without PCH to avoid namespace conflicts
# This creates a static library that provides the run_tests() export function
doctest_dll_main = static_library('doctest_dll_main',
  'doctest_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],  # Add src and stub for platform headers
  cpp_args: ['-DTEST_DLL_MODE', '-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],  # Add FASTLED_STUB_IMPL
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

# Build individual test executables

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

  # Build list of dependencies (libunwind if available)
  test_dependencies = []
  if libunwind_dep.found()
    test_dependencies += [libunwind_dep]
  endif

  test_exe = executable(test_name,
    test_sources,
    include_directories: [src_dir, tests_dir, stub_dir],
    cpp_args: test_cpp_args,
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_main],
    dependencies: test_dependencies,  # Add libunwind here
    install: false
  )

  # Track test executable for alias
  all_test_executables += test_exe

  # Register test with project root as working directory
  test(test_name, test_exe, workdir: meson.project_source_root())
endforeach

# Create alias target that builds all test executables
alias_target('all_tests', all_test_executables)

message('Individual test build enabled: ' + test_names.length().to_string() + ' test executables')
