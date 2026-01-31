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
    '-fPIC',                       # Required: tests build as shared_library which needs PIC
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
# NOTE: ENABLE_CRASH_HANDLER is removed - crash handler setup moved to runner.exe
doctest_dll_main = static_library('doctest_dll_main',
  'doctest_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],  # Add src and stub for platform headers
  cpp_args: ['-DTEST_DLL_MODE', '-DFASTLED_STUB_IMPL'],  # Removed -DENABLE_CRASH_HANDLER
  install: false
)

# Build runner executable (static, generic DLL loader)
# This executable loads a test DLL and calls its run_tests() function
# It will be copied to each test directory as <test_name>.exe
#
# IMPORTANT: The runner intentionally does NOT link against fastled_lib.
# Only the test DLLs should link FastLED to avoid duplicate vtables/symbols
# which cause UBSAN "invalid vptr" errors when objects cross library boundaries.
# The runner uses std:: types instead of fl:: types to stay lightweight.
#
# System libraries (pthread, dbghelp, psapi) are linked here (once)
# instead of in each test_X.dll (176 times), significantly reducing build time.
# The runner.exe provides these symbols to the loaded DLL at runtime.
# Crash handler is also setup in runner.exe before loading any DLL.
#
# NOTE: runner_link_args, runner_cpp_args, and crash_handler_lib are defined
# in root meson.build and shared with examples/meson.build for consistency.
# libunwind is injected via DLL at runtime by clang-tool-chain, no explicit linking needed.
runner_exe = executable('runner',
  'shared/runner.cpp',
  include_directories: [src_dir, stub_dir],
  cpp_args: runner_cpp_args,
  link_args: runner_link_args,
  link_with: [crash_handler_lib],  # Shared crash handler from root meson.build
  install: false
)

# Discover and organize tests using external Python script
# Output format: TEST:<name>:<file_path>:<category>
# Uses hash-based caching to avoid re-running organize_tests.py when test files unchanged
cache_script = meson.current_source_dir() / 'test_metadata_cache.py'
tests_source_dir = meson.current_source_dir()
tests_build_dir = meson.current_build_dir()

# Check if cache is valid
cache_check = run_command(
  'python', cache_script, '--check', tests_source_dir, tests_build_dir,
  check: false
)

if cache_check.returncode() == 0
  # Cache hit - use cached metadata
  test_metadata_output = cache_check.stdout().strip()
  message('Using cached test metadata (no changes detected)')
else
  # Cache miss - run full discovery
  message('Running test discovery (cache invalid or missing)')

  discovery_result = run_command(
    'python', meson.current_source_dir() / 'organize_tests.py',
    tests_source_dir,
    check: true
  )
  test_metadata_output = discovery_result.stdout().strip()

  # Update cache
  run_command(
    'python', cache_script, '--update',
    tests_source_dir, tests_build_dir, test_metadata_output,
    check: true
  )
endif

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

# Build individual test DLLs and runner executables

# Track all test DLLs and runners for alias
all_test_dlls = []
all_test_runners = []

# Build each test as a DLL with shared PCH
foreach test_name : test_names
  # Get test file path from dynamic map (for subdirectories) or default to test_name.cpp
  test_file = test_file_paths.get(test_name, test_name + '.cpp')

  # Some tests need additional source files for ESP32 parallel SPI host simulation
  test_sources = [test_file, test_pch]
  test_cpp_args = unit_test_compile_args + [
    '-DTEST_DLL_MODE',                          # Define TEST_DLL_MODE for DLL builds
    '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
    '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
    '-fpch-validate-input-files-content',       # Validate based on content, not mtime
  ]

  # host_sim.cpp is now compiled as part of libfastled.a when STUB_PLATFORM is defined
  # No need to add it explicitly to individual tests anymore

  # Build test as a shared library (DLL)
  # NOTE: No dependencies needed - libunwind moved to runner.exe
  test_dll = shared_library(test_name,
    test_sources,
    include_directories: [src_dir, stub_dir, tests_dir],
    cpp_args: test_cpp_args,
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_dll_main],  # Use DLL-mode doctest main
    name_prefix: '',  # Remove 'lib' prefix on Windows (produce 'test.dll' not 'libtest.dll')
    install: false
  )

  # Track test DLL for alias
  all_test_dlls += test_dll

  # Copy runner.exe to test directory as <test_name>.exe
  test_runner = custom_target(test_name + '_runner',
    input: runner_exe,
    output: test_name + '.exe',
    command: ['cp', '@INPUT@', '@OUTPUT@'],
    depends: [test_dll],  # Ensure DLL is built first
    build_by_default: true
  )

  # Track test runner for alias
  all_test_runners += test_runner

  # Register test: runner.exe loads the DLL and executes it
  # Some tests have race conditions when run in parallel (use wall-clock time via micros())
  is_serial = test_name in ['fl_file_system', 'test_uart_channel_engine']
  test(test_name, runner_exe,
    args: [test_dll.full_path()],
    workdir: meson.project_source_root(),
    depends: [test_dll],
    is_parallel: not is_serial
  )
endforeach

# Create alias target that builds all test DLLs and runners
alias_target('all_tests', all_test_dlls + all_test_runners)

message('Individual test DLL build enabled: ' + test_names.length().to_string() + ' test DLLs + runners')
