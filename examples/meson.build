# FastLED Examples - Meson Build Integration
#
# This file provides two types of example compilation targets:
#
# 1. HOST-BASED COMPILATION (Fast, for testing/development):
#    - Compiles examples for host machine using STUB platform
#    - Ultra-fast: ~0.24s for all 96 examples with PCH
#    - Targets: ninja -C builddir examples-host, example-Blink, etc.
#    - See: examples/host/meson.build
#
# 2. HARDWARE BOARD COMPILATION (PlatformIO-based):
#    - Compiles examples for actual Arduino boards
#    - Targets: example-blink-uno, examples-all-uno, etc.
#    - Uses PlatformIO for cross-compilation
#
# Meson targets:
#   ninja -C builddir examples-host           - Compile ALL examples for host (fast!)
#   ninja -C builddir example-Blink           - Compile Blink for host
#   ninja -C builddir example-blink-uno       - Compile Blink for Arduino Uno
#   ninja -C builddir example-blink-esp32     - Compile Blink for ESP32
#   ninja -C builddir examples-all-uno        - Compile all examples for Uno
#   ninja -C builddir examples-default        - Compile all examples for default boards

# Get project root and filesystem module
project_root = meson.project_source_root()
fs = import('fs')

# Include directories are provided by parent meson.build:
#   src_dir  = include_directories('src')
#   stub_dir = include_directories('src/platforms/stub')
# No need to redefine them here

# ============================================================================
# HOST-BASED EXAMPLE COMPILATION (Fast, using STUB platform)
# ============================================================================
# Compiles all Arduino examples for the host platform using STUB backend.
# Discovers all .ino files and uses their parent directory as the sketch directory.

# Discover all .ino files using Python helper scripts
discover_script = project_root / 'ci' / 'meson' / 'discover_examples.py'
discover_includes_script = project_root / 'ci' / 'meson' / 'discover_example_includes.py'
discover_sources_script = project_root / 'ci' / 'meson' / 'discover_example_sources.py'
filter_script = project_root / 'ci' / 'meson' / 'filter_examples_for_stub.py'
examples_dir = project_root / 'examples'

# Get include directories for each example - this also gives us the example paths
result_includes = run_command(
    'python', discover_includes_script, examples_dir,
    check: true,
    capture: true
)

# Get additional source files for each example (.cpp files in subdirectories)
result_sources = run_command(
    'python', discover_sources_script, examples_dir,
    check: true,
    capture: true
)

# Get @filter-based exclusions for stub platform
result_filter = run_command(
    'python', filter_script, examples_dir,
    check: true,
    capture: true
)

# Parse filter results: SKIP:<name>:<reason>
# Build list of examples that should be skipped for stub platform
stub_incompatible_examples = []
filter_reasons = {}  # Map example name to skip reason
foreach line : result_filter.stdout().strip().split('\n')
    if line != ''
        parts = line.split(':')
        if parts.length() >= 3 and parts[0] == 'SKIP'
            example_name = parts[1]
            reason = parts[2]
            stub_incompatible_examples += [example_name]
            filter_reasons += {example_name: reason}
        endif
    endif
endforeach

# Parse include information: EXAMPLE:<name>:<dir1>:<dir2>:...
# The first directory (dir1) is always the example's root directory (sketch directory)
example_includes = {}
example_paths = {}  # Map example name to its root directory path
foreach line : result_includes.stdout().strip().split('\n')
    parts = line.split(':')
    if parts.length() >= 3
        example_name = parts[1]
        # First include dir is the example's root directory (sketch directory)
        example_root = parts[2]
        example_paths += {example_name: example_root}

        # Collect all include dirs
        include_dirs = []
        foreach i : range(2, parts.length())
            include_dirs += [parts[i]]
        endforeach
        example_includes += {example_name: include_dirs}
    endif
endforeach

# Parse source file information: SOURCES:<name>:<source1>:<source2>:...
example_sources = {}
foreach line : result_sources.stdout().strip().split('\n')
    parts = line.split(':')
    if parts.length() >= 2
        example_name = parts[1]
        # Collect additional .cpp source files (beyond the .ino)
        source_files = []
        foreach i : range(2, parts.length())
            if parts[i] != ''
                source_files += [parts[i]]
            endif
        endforeach
        example_sources += {example_name: source_files}
    endif
endforeach

# Get stub_main.cpp source file (provides main() that calls setup/loop)
stub_main_src = project_root / 'src' / 'platforms' / 'stub_main.cpp'

# ============================================================================
# BUILD EXAMPLE RUNNER (DLL LOADER)
# ============================================================================
# Build crash handler library for example_runner.exe
# Reuse the crash handler from tests (same implementation)
crash_handler_deps_example = []
if libunwind_dep.found()
  crash_handler_deps_example += [libunwind_dep]
endif

crash_handler_example = static_library('crash_handler_example',
  files('../tests/shared/crash_handler_main.cpp'),
  include_directories: [src_dir, stub_dir, tests_dir],
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],
  dependencies: crash_handler_deps_example,
  install: false
)

# Build example_runner.exe (static, generic DLL loader)
# This executable loads an example DLL and calls its run_example() function
# It will be copied to each example directory as example-<name>.exe
#
# OPTIMIZATION: System libraries (pthread, dbghelp, psapi) are linked here (once)
# instead of in each example-X.dll (96 times), significantly reducing build time.
# The example_runner.exe provides these symbols to the loaded DLL at runtime.
# Crash handler is also setup in example_runner.exe before loading any DLL.
# Example runner link arguments: Force LLD linker on all platforms for consistency.
# Static linking flags work with clang-tool-chain's MinGW mode on Windows and GNU mode on Linux.
# macOS doesn't support fully static binaries (system libs unavailable as static).
example_runner_deps = []
if build_machine.system() == 'windows'
  # Windows: Static linking + system libraries (MinGW mode)
  example_runner_link_args = [
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
  example_runner_link_args = [
    '-fuse-ld=lld',             # Force LLD linker
    '-pthread',                 # POSIX threads
  ]
  if libunwind_dep.found()
    example_runner_deps += [libunwind_dep]
  endif
else
  # Linux: Static linking + pthread
  example_runner_link_args = [
    '-fuse-ld=lld',             # Force LLD linker
    '-static',                  # Static linking
    '-static-libgcc',           # Statically link libgcc
    '-static-libstdc++',        # Statically link libstdc++
    '-pthread',                 # POSIX threads
  ]
  if libunwind_dep.found()
    example_runner_deps += [libunwind_dep]
  endif
endif

example_runner_exe = executable('example_runner',
  files('../tests/shared/example_runner.cpp'),
  include_directories: [src_dir, stub_dir],  # For platforms/run_example.h
  cpp_args: [
    '-std=c++11',
    '-O0',
    '-g',
    '-DFASTLED_STUB_IMPL',  # Required for fl:: types
  ],
  link_args: example_runner_link_args,
  link_with: [crash_handler_example, fastled_lib],  # Link crash handler and fastled library
  dependencies: example_runner_deps,
  install: false
)

# Cache python program to avoid repeated lookups in the loop
python_prog = find_program('python')

# List to store all example executables for the alias target
example_executables = []

# Create executable target for each discovered example
foreach example_name : example_paths.keys()
    # Skip blacklisted examples (platform-specific or incompatible with STUB)
    if stub_incompatible_examples.contains(example_name)
        continue
    endif

    # Get the example's root directory path (sketch directory)
    example_root = example_paths[example_name]

    # Get include directories for this example
    example_include_dirs = []
    if example_includes.has_key(example_name)
        example_include_dirs = example_includes[example_name]
    endif

    # Build include_directories objects
    # inc_dir is relative to project root (e.g., "examples/Blink")
    # We need to make it relative to this meson.build (which is in examples/)
    include_objs = []
    foreach inc_dir : example_include_dirs
        # Strip "examples/" prefix if present (Windows uses backslash)
        inc_dir_fixed = inc_dir
        if inc_dir.startswith('examples\\') or inc_dir.startswith('examples/')
            inc_dir_fixed = inc_dir.substring(9)  # Length of "examples/"
        endif
        include_objs += include_directories(inc_dir_fixed)
    endforeach

    # Construct proper path: <example_root>/<example_name>.ino
    ino_file = project_root / example_root / (example_name + '.ino')

    # Generate a wrapper .cpp file that includes the .ino and provides main()
    # This approach is cleaner than using preprocessor macros
    wrapper_cpp = custom_target(
        'wrapper-' + example_name,
        output: 'example-' + example_name + '-wrapper.cpp',
        command: [
            python_prog,
            project_root / 'ci' / 'util' / 'generate_wrapper.py',
            '@OUTPUT@',
            example_name,
            ino_file,
        ],
        build_by_default: false
    )

    # Add FastLED includes (src, stub platform, and tests for shared infrastructure) to the example includes
    all_includes = [src_dir, stub_dir, tests_dir] + include_objs

    # Get additional source files for this example (e.g., src/*.cpp)
    additional_sources = []
    if example_sources.has_key(example_name) and example_sources[example_name].length() > 0
        foreach src_file : example_sources[example_name]
            additional_sources += files(project_root / src_file)
        endforeach
    endif

    # DEBUG FLAGS: Special case for Downscale example - add debug symbols for investigation
    # TODO: Consider generalizing this mechanism for per-example compile flags if needed
    debug_cpp_args = []
    # if example_name == 'Downscale'
    #     debug_cpp_args = ['-g3', '-O0', '-DFASTLED_MULTITHREADING=0']
    # endif

    # Collect all source files: wrapper + any additional .cpp files from subdirectories
    all_sources = [wrapper_cpp] + additional_sources

    # Build example as a shared library (DLL)
    # NOTE: System dependencies moved to example_runner.exe
    example_dll = shared_library(
        'example-' + example_name,
        all_sources,  # Wrapper + additional .cpp files
        include_directories: all_includes,
        link_with: fastled_lib,
        cpp_args: example_compile_args + debug_cpp_args + ['-DEXAMPLE_DLL_MODE'],
        link_args: dll_link_args,  # Minimal linking (defined in root meson.build)
        name_prefix: '',  # Remove 'lib' prefix on Windows (produce 'example-Blink.dll' not 'libexample-Blink.dll')
        build_by_default: false,  # Only build when explicitly requested
        install: false
    )

    # Copy example_runner.exe to example directory as example-<name>.exe
    # This allows running `example-Blink.exe` which auto-loads `example-Blink.dll`
    example_runner_copy = custom_target(
        'example-' + example_name + '_runner',
        input: example_runner_exe,
        output: 'example-' + example_name + '.exe',
        command: ['cp', '@INPUT@', '@OUTPUT@'],
        depends: [example_dll],  # Ensure DLL is built first
        build_by_default: false
    )

    # Register as test target: runner loads the DLL and executes it
    # Add to 'examples' suite for easy filtering
    test('example-' + example_name, example_runner_exe,
        args: [example_dll.full_path()],
        suite: 'examples',
        timeout: 60
    )

    # Track both DLL and runner for alias target
    example_executables += [example_dll, example_runner_copy]
endforeach

# Create alias target to build all examples
alias_target('examples-host', example_executables)

example_count = example_executables.length()
total_discovered = example_paths.keys().length()
skipped_count = stub_incompatible_examples.length()
message('Host-based example compilation: ' + example_count.to_string() + '/' + total_discovered.to_string() + ' examples (' + skipped_count.to_string() + ' platform-specific examples skipped)')

# ============================================================================
# HARDWARE BOARD COMPILATION (PlatformIO-based)
# ============================================================================

# Find uv executable
uv_exe = find_program('uv', required: false)
if not uv_exe.found()
    warning('uv not found - PlatformIO example compilation targets will not be available. Install with: pip install uv')
    subdir_done()
endif

# Path to compile script (relative to project root)
compile_script = meson.project_source_root() / 'ci' / 'ci-compile.py'

# Common boards for quick testing
common_boards = ['uno', 'esp32dev', 'esp8266', 'esp32s3', 'teensy41']

# Common examples
common_examples = ['Blink', 'DemoReel100', 'ColorPalette', 'Cylon', 'Fire2012']

# Create individual example + board targets
# Usage: ninja -C builddir example-blink-uno
foreach board : common_boards
    foreach example : common_examples
        target_name = 'example-@0@-@1@'.format(example.to_lower(), board)
        run_target(target_name,
            command: [uv_exe, 'run', compile_script, board, example, '--local'],
            depends: [fastled_lib]  # Ensure libfastled.a is built first
        )
    endforeach
endforeach

# Create "all examples for board" targets
# Usage: ninja -C builddir examples-all-uno
foreach board : common_boards
    target_name = 'examples-all-@0@'.format(board)
    run_target(target_name,
        command: [uv_exe, 'run', compile_script, board, '--local'],
        depends: [fastled_lib]  # Ensure libfastled.a is built first
    )
endforeach

# Create target to compile default examples on default boards
# Usage: ninja -C builddir examples-default
run_target('examples-default',
    command: [uv_exe, 'run', compile_script, '--local'],
    depends: [fastled_lib]  # Ensure libfastled.a is built first
)

# Create target to compile a single example for all common boards
# Usage: ninja -C builddir examples-blink-all-boards
foreach example : common_examples
    target_name = 'examples-@0@-all-boards'.format(example.to_lower())
    board_list = ','.join(common_boards)
    run_target(target_name,
        command: [uv_exe, 'run', compile_script, board_list, example, '--local'],
        depends: [fastled_lib]  # Ensure libfastled.a is built first
    )
endforeach

message('PlatformIO example compilation targets configured:')
message('  - Individual: example-<name>-<board> (e.g., example-blink-uno)')
message('  - All examples: examples-all-<board> (e.g., examples-all-uno)')
message('  - Single example, all boards: examples-<name>-all-boards (e.g., examples-blink-all-boards)')
message('  - Default: examples-default')
message('  Available boards: ' + ', '.join(common_boards))
message('  Available examples: ' + ', '.join(common_examples))
