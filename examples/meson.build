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

# Discover all .ino files using Python helper scripts with caching
cache_script = project_root / 'ci' / 'meson' / 'example_metadata_cache.py'
discover_all_script = project_root / 'ci' / 'meson' / 'discover_examples_all.py'
examples_dir = project_root / 'examples'
examples_build_dir = meson.current_build_dir()

# Check if cache is valid
cache_check = run_command(
    'python', cache_script, '--check', examples_dir, examples_build_dir,
    check: false
)

if cache_check.returncode() == 0
    # Cache hit - use cached metadata
    example_metadata_output = cache_check.stdout().strip()
    message('Using cached example metadata (no changes detected)')
else
    # Cache miss - run full discovery
    message('Running example discovery (cache invalid or missing)')

    discovery_result = run_command(
        'python', discover_all_script, examples_dir,
        check: true
    )
    example_metadata_output = discovery_result.stdout().strip()

    # Update cache
    run_command(
        'python', cache_script, '--update',
        examples_dir, examples_build_dir, example_metadata_output,
        check: true
    )
endif

# Parse consolidated metadata in single pass
# Format: EXAMPLE|<name>|<dir1>|..., SOURCES|<name>|<src1>|..., SKIP|<name>|<reason>
# Note: Using | delimiter instead of : to avoid Windows path issues (C:\...)
stub_incompatible_examples = []
example_includes = {}
example_paths = {}  # Map example name to its root directory path
example_sources = {}

foreach line : example_metadata_output.split('\n')
    if line != ''
        parts = line.split('|')
        if parts.length() >= 2
            line_type = parts[0]
            example_name = parts[1]

            if line_type == 'EXAMPLE' and parts.length() >= 3
                # Parse EXAMPLE|<name>|<dir1>|<dir2>|...
                # First include dir is the example's root directory (sketch directory)
                example_root = parts[2]
                example_paths += {example_name: example_root}

                # Collect all include dirs
                include_dirs = []
                foreach i : range(2, parts.length())
                    include_dirs += [parts[i]]
                endforeach
                example_includes += {example_name: include_dirs}

            elif line_type == 'SOURCES'
                # Parse SOURCES|<name>|<source1>|<source2>|...
                # Collect additional .cpp source files (beyond the .ino)
                source_files = []
                foreach i : range(2, parts.length())
                    if parts[i] != ''
                        source_files += [parts[i]]
                    endif
                endforeach
                example_sources += {example_name: source_files}

            elif line_type == 'SKIP' and parts.length() >= 3
                # Parse SKIP|<name>|<reason>
                reason = parts[2]
                stub_incompatible_examples += [example_name]
            endif
        endif
    endif
endforeach

# ============================================================================
# BUILD EXAMPLE RUNNER (DLL LOADER)
# ============================================================================
# Build example_runner.exe (static, generic DLL loader)
# This executable loads an example DLL and calls its run_example() function
# It will be copied to each example directory as example-<name>.exe
#
# IMPORTANT: The runner intentionally does NOT link against fastled_lib.
# Only the example DLLs should link FastLED to avoid duplicate vtables/symbols
# which cause UBSAN "invalid vptr" errors when objects cross library boundaries.
# The runner uses std:: types instead of fl:: types to stay lightweight.
#
# System libraries (pthread, dbghelp, psapi) are linked here (once)
# instead of in each example-X.dll (96 times), significantly reducing build time.
# The example_runner.exe provides these symbols to the loaded DLL at runtime.
# Crash handler is also setup in example_runner.exe before loading any DLL.
#
# NOTE: runner_link_args, runner_cpp_args, and crash_handler_lib are defined
# in root meson.build and shared with tests/meson.build for consistency.
# libunwind is injected via DLL at runtime by clang-tool-chain, no explicit linking needed.
example_runner_exe = executable('example_runner',
  files('../tests/shared/example_runner.cpp'),
  include_directories: [src_dir, stub_dir],
  cpp_args: runner_cpp_args,
  link_args: runner_link_args,
  link_with: [crash_handler_lib],  # Shared crash handler from root meson.build
  install: false
)

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
    example_include_dirs = example_includes.get(example_name, [])

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

    # Use static wrapper template instead of generating wrapper at build time
    # This eliminates custom_target overhead and Python script dependency
    wrapper_cpp = files(project_root / 'tests' / 'shared' / 'example_dll_wrapper_template.cpp')

    # Add FastLED includes (src, stub platform, and tests for shared infrastructure) to the example includes
    all_includes = [src_dir, stub_dir, tests_dir] + include_objs

    # Get additional source files for this example (e.g., src/*.cpp)
    additional_sources = []
    example_source_list = example_sources.get(example_name, [])
    if example_source_list.length() > 0
        foreach src_file : example_source_list
            additional_sources += files(project_root / src_file)
        endforeach
    endif

    # DEBUG FLAGS: Special case for Downscale example - add debug symbols for investigation
    # TODO: Consider generalizing this mechanism for per-example compile flags if needed
    debug_cpp_args = []
    # if example_name == 'Downscale'
    #     debug_cpp_args = ['-g3', '-O0', '-DFASTLED_MULTITHREADING=0']
    # endif

    # Pass .ino path via preprocessor define to the static wrapper template
    # This eliminates the need for dynamic wrapper generation
    # Add -fvisibility=hidden to make all symbols private by default
    # Only explicitly exported symbols (run_example) will be visible
    example_specific_cpp_args = [
        '-DEXAMPLE_INO_PATH="' + ino_file + '"',
        '-fvisibility=hidden'
    ]

    # Collect all source files: wrapper + any additional .cpp files from subdirectories
    all_sources = [wrapper_cpp] + additional_sources

    # Build example as a shared library (DLL)
    # NOTE: System dependencies moved to example_runner.exe
    example_dll = shared_library(
        'example-' + example_name,
        all_sources,  # Wrapper + additional .cpp files
        include_directories: all_includes,
        link_with: fastled_lib,
        cpp_args: example_compile_args + debug_cpp_args + example_specific_cpp_args + ['-DEXAMPLE_DLL_MODE'],
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

# Create alias target to build all examples (only if we have examples)
if example_executables.length() > 0
    alias_target('examples-host', example_executables)
endif

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
