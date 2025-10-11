# FastLED Examples - Meson Build Integration
#
# This provides meson targets to compile Arduino examples using PlatformIO.
# For direct PlatformIO compilation, use: uv run ci/ci-compile.py <board> <example>
#
# Meson targets:
#   ninja -C builddir example-blink-uno       - Compile Blink for Arduino Uno
#   ninja -C builddir example-blink-esp32     - Compile Blink for ESP32
#   ninja -C builddir examples-all-uno        - Compile all examples for Uno
#   ninja -C builddir examples-default        - Compile all examples for default boards

# Get project root
project_root = meson.project_source_root()

# Find uv executable
uv_exe = find_program('uv', required: false)
if not uv_exe.found()
    warning('uv not found - example compilation targets will not be available. Install with: pip install uv')
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
            depends: []
        )
    endforeach
endforeach

# Create "all examples for board" targets
# Usage: ninja -C builddir examples-all-uno
foreach board : common_boards
    target_name = 'examples-all-@0@'.format(board)
    run_target(target_name,
        command: [uv_exe, 'run', compile_script, board, '--local'],
        depends: []
    )
endforeach

# Create target to compile default examples on default boards
# Usage: ninja -C builddir examples-default
run_target('examples-default',
    command: [uv_exe, 'run', compile_script, '--local'],
    depends: []
)

# Create target to compile a single example for all common boards
# Usage: ninja -C builddir examples-blink-all-boards
foreach example : common_examples
    target_name = 'examples-@0@-all-boards'.format(example.to_lower())
    board_list = ','.join(common_boards)
    run_target(target_name,
        command: [uv_exe, 'run', compile_script, board_list, example, '--local'],
        depends: []
    )
endforeach

message('Example compilation targets configured:')
message('  - Individual: example-<name>-<board> (e.g., example-blink-uno)')
message('  - All examples: examples-all-<board> (e.g., examples-all-uno)')
message('  - Single example, all boards: examples-<name>-all-boards (e.g., examples-blink-all-boards)')
message('  - Default: examples-default')
message('  Available boards: ' + ', '.join(common_boards))
message('  Available examples: ' + ', '.join(common_examples))
