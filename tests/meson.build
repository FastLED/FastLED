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

# Crash handler library for runner.exe
# This provides crash handler setup that runs BEFORE any DLL is loaded
# Links to dbghelp/psapi on Windows, libunwind on Unix (if available)
crash_handler_deps = []
if libunwind_dep.found()
  crash_handler_deps += [libunwind_dep]
endif

crash_handler_runner = static_library('crash_handler_runner',
  'shared/crash_handler_main.cpp',
  include_directories: [tests_dir, src_dir, stub_dir],
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],
  dependencies: crash_handler_deps,
  install: false
)

# Build runner executable (static, generic DLL loader)
# This executable loads a test DLL and calls its run_tests() function
# It will be copied to each test directory as <test_name>.exe
#
# OPTIMIZATION: System libraries (pthread, dbghelp, psapi) are linked here (once)
# instead of in each test_X.dll (176 times), significantly reducing build time.
# The runner.exe provides these symbols to the loaded DLL at runtime.
# Crash handler is also setup in runner.exe before loading any DLL.
# Runner link arguments: Force LLD linker on all platforms for consistency.
# LLD is used everywhere via -fuse-ld=lld (set in root meson.build common_link_args).
# Static linking flags work with clang-tool-chain's MinGW mode on Windows and GNU mode on Linux.
# macOS doesn't support fully static binaries (system libs unavailable as static).
runner_deps = []
if build_machine.system() == 'windows'
  # Windows: Static linking + system libraries (MinGW mode)
  runner_link_args = [
    '-fuse-ld=lld',             # Force LLD linker
    '-static',                  # Static linking
    '-static-libgcc',           # Statically link libgcc
    '-static-libstdc++',        # Statically link libstdc++
    '-lpthread',                # POSIX threads (winpthreads)
    '-ldbghelp',                # Debug helper library (for stack traces)
    '-lpsapi',                  # Process status API (for memory info)
  ]
elif build_machine.system() == 'darwin'
  # macOS: Dynamic linking (static not supported), but force LLD
  runner_link_args = [
    '-fuse-ld=lld',             # Force LLD linker
    '-pthread',                 # POSIX threads
  ]
else
  # Linux: Static linking + pthread
  runner_link_args = [
    '-fuse-ld=lld',             # Force LLD linker
    '-static',                  # Static linking
    '-static-libgcc',           # Statically link libgcc
    '-static-libstdc++',        # Statically link libstdc++
    '-pthread',                 # POSIX threads
  ]
  if libunwind_dep.found()
    runner_deps += [libunwind_dep]
  endif
endif

runner_exe = executable('runner',
  'shared/runner.cpp',
  include_directories: [src_dir, stub_dir],  # For platforms/run_unit_test.h
  cpp_args: [
    '-std=c++11',
    '-O2',
    '-g',
    '-DFASTLED_STUB_IMPL',  # Required for fl:: types
    # Suppress warnings from Windows SDK headers
    '-Wno-pragma-pack',
  ],
  link_args: runner_link_args,
  link_with: [crash_handler_runner, fastled_lib],  # Link crash handler and fastled library
  dependencies: runner_deps,
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
    include_directories: [src_dir, tests_dir, stub_dir],
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
  test(test_name, runner_exe,
    args: [test_dll.full_path()],
    workdir: meson.project_source_root(),
    depends: [test_dll]
  )
endforeach

# Create alias target that builds all test DLLs and runners
alias_target('all_tests', all_test_dlls + all_test_runners)

message('Individual test DLL build enabled: ' + test_names.length().to_string() + ' test DLLs + runners')
