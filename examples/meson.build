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

# Define include directories for FastLED (relative to this meson.build file)
fastled_src_dir = include_directories('../src')
fastled_stub_dir = include_directories('../src/platforms/stub')

# ============================================================================
# HOST-BASED EXAMPLE COMPILATION (Fast, using STUB platform)
# ============================================================================
# Compiles all Arduino examples for the host platform using STUB backend.
# Discovers all .ino files and uses their parent directory as the sketch directory.

# Blacklist of examples that won't compile with STUB platform
# These require platform-specific hardware, Serial, or other unavailable features
stub_incompatible_examples = [
    'AdafruitBridge',          # Platform-specific driver
    'Animartrix',              # Excessive memory usage (crashes on host)
    'AnimartrixRing',          # Uses cos/sin (missing math)
    'Audio',                   # Audio hardware
    'AudioInput',              # Audio hardware
    'BeatDetection',           # Uses Serial
    'Bulk',                    # Platform-specific
    'BulkRmtSimple',          # Platform-specific (ESP32 RMT)
    'Chromancer',              # Excessive memory usage (crashes on host)
    'Codec',                   # Platform-specific
    'ColorBoost',              # Crashes on host (uninitialized bool)
    'Corkscrew',               # Crashes on host (excessive memory)
    'Downscale',               # Crashes on host (excessive memory)
    'EaseInOut',               # Crashes on host (excessive memory)
    'Esp32P4Parlio',          # ESP32-specific
    'Esp8266Uart',            # ESP8266-specific
    'FestivalStick',          # Uses Serial
    'FireCylinder',            # Crashes on host (null pointer)
    'FireMatrix',              # Excessive memory usage (100x100 LED array = 300KB)
    'FxEngine',                # Video/buffer issues on host
    'FxNoisePlusPalette',      # Crashes on host (excessive memory)
    'FxNoiseRing',             # Crashes on host (excessive memory)
    'FxSdCard',               # SD card hardware
    'FxWave2d',                # Crashes on host (excessive memory)
    'HSVTest',                 # Crashes on host (excessive memory)
    'Json',                    # Crashes on host (initialization issues)
    'LCD_I80',                # ESP-specific LCD
    'Luminova',               # Uses roundf (missing math)
    'LuminescentGrand',       # Uses Serial
    'NetTest',                # Network hardware
    'NoisePlayground',         # Crashes on host (excessive memory)
    'OctoWS2811',             # Teensy OctoWS2811 library
    'OctoWS2811Demo',         # Teensy OctoWS2811 library
    'OTA',                    # Network/OTA specific
    'Overclock',              # Platform-specific
    'Parallel_IO',            # RP2040-specific
    'ParallelSPI',            # Platform-specific
    'Particles1d',             # Crashes on host (excessive memory)
    'PhasorField',            # Uses cos/sin
    'Pintest',                # Uses Serial
    'PJRCSpectrumAnalyzer',   # Teensy OctoWS2811 library
    'QuadSPI_Basic',          # ESP-specific QSPI
    'QuadSPI_MultiChipset',   # ESP-specific QSPI
    'SAMD_SingleSPI',         # SAMD-specific
    'Sound2Midi',             # Audio hardware
    'Spi',                    # Platform-specific SPI
    'Test',                   # Uses Serial
    'UITest',                 # Uses Serial
    'wasm',                    # Crashes on host (excessive memory)
    'WasmScreenCoords',       # WASM-specific
    'Wave',                    # Crashes on host (excessive memory)
    'Wave2d',                  # Crashes on host (excessive memory)
]

# Discover all .ino files using Python helper script
discover_script = project_root / 'ci' / 'discover_examples.py'
discover_includes_script = project_root / 'ci' / 'discover_example_includes.py'
examples_dir = project_root / 'examples'

# Get include directories for each example - this also gives us the example paths
result_includes = run_command(
    'python', discover_includes_script, examples_dir,
    check: true,
    capture: true
)

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

# Get stub_main.cpp source file (provides main() that calls setup/loop)
stub_main_src = project_root / 'src' / 'fl' / 'stub_main.cpp'

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

    # Compile example as executable using stub_main.cpp
    # The stub_main.cpp will include the .ino file via FASTLED_STUB_MAIN_INCLUDE_INO macro
    # Construct proper path: <example_root>/<example_name>.ino
    ino_file = project_root / example_root / (example_name + '.ino')

    # Add FastLED includes (src and stub platform) to the example includes
    all_includes = [fastled_src_dir, fastled_stub_dir] + include_objs

    # Some examples may fail to compile due to platform-specific code
    # Use should_fail: true to mark them as expected failures
    # Set a large stack size to accommodate examples with large arrays (e.g., FireMatrix, Chromancer)
    example_link_args = []
    if build_machine.system() == 'windows'
        # Windows: Set stack size to 16MB (default is usually 1MB)
        example_link_args = ['-Wl,--stack,16777216']
    endif

    exe = executable(
        'example-' + example_name,
        stub_main_src,
        include_directories: all_includes,
        link_with: fastled_lib,
        cpp_args: [
            '-DARDUINO=1',  # Define ARDUINO to enable Arduino.h inclusion
            '-DFASTLED_STUB_MAIN',
            '-DFASTLED_STUB_MAIN_INCLUDE_INO=' + ino_file,
            '-DFASTLED_STUB_IMPL',
            '-fno-rtti',  # Must match fastled_lib compilation flags
            '-fno-exceptions',  # Must match fastled_lib compilation flags
        ],
        link_args: example_link_args,
        build_by_default: false,  # Only build when explicitly requested
        install: false
    )

    # Register as test target so it can be run with `meson test`
    # Add to 'examples' suite for easy filtering
    # Note: Some tests may fail due to platform-specific code, but we still include them
    test('example-' + example_name, exe, suite: 'examples', timeout: 60)

    example_executables += exe
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
