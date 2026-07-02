## Contributing

The most important part about contributing to FastLED is knowing how to test your changes.

The FastLED library includes a powerful cli that can compile to any device. It will run if you have either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) installed on the system.

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml)

The FastLED compiler cli can be invoked at the project root.

```bash (MacOS/Linux, windows us git-bsh or compile.bat)
git clone https://github.com/fastled/fastled
cd fastled
./compile uno  # Compiles Blink by default (linux/macos/git-bash)
# compile.bat  # Windows.
```

## Linting and Unit Testing

```bash
./lint
./test  # runs unit tests
# Note that you do NOT need to install the C++ compiler toolchain
# for compiling + running unit tests via ./test. The clang-tool-chain
# compiler will be automatically installed when you run the tests.
````


### Testing platforms and examples

By default, `./compile <board>` compiles the Blink example. You can specify different examples or use the `all` keyword to compile all examples:

```bash
# Compile Blink (default)
./compile uno

# Compile specific examples
./compile teensy41,teensy40 --examples ColorPalette
./compile esp32dev,esp32s3,esp32c3,esp32c6,esp32s2 --examples Blink,Apa102HD

# Compile ALL examples (use 'all' keyword)
./compile uno all
./compile esp32dev all
```

## Unit Tests

Shared code is unit-tested on the host machine. They can be found at `tests/` at the root of the repo. Unit testing only requires either `python` or `uv` to be installed. The C++ compiler toolchain will be installed automatically.

The easiest way to run the tests is just use `./test`

## QEMU Emulation Testing

FastLED validates ESP32 examples end-to-end in QEMU. The previous Docker-based `./test --qemu esp32s3` runner (which pulled `niteris/fastled-simulator-*` images) was retired along with the rest of the platform-Docker infrastructure — fbuild now drives the Espressif QEMU binary directly, and the exact invocation lives in `.github/workflows/qemu_docker_template.yml`.

### Running ESP32 Examples in QEMU locally

```bash
# 1. Stage the sketch — produces .build/pio/<env>/ with firmware + partitions.
uv run ci/ci-compile.py esp32s3 \
    --examples BlinkParallel \
    --merged-bin \
    --defines FASTLED_ESP32_IS_QEMU \
    --verbose

# 2. Emulate with fbuild (auto-downloads the Espressif QEMU binary on first run).
uv run fbuild test-emu \
    --emulator qemu \
    --environment esp32s3 \
    --timeout 120 \
    --halt-on-success "setup starting" \
    --halt-on-error "Guru Meditation|abort\\(\\)|Backtrace:" \
    .build/pio/esp32s3
```

Swap `esp32s3` for `esp32dev`, `esp32c3`, or any other QEMU-supported target. CI runs the same two-step sequence in `qemu_docker_template.yml`.

## VSCode

We support VSCode with powerful IntelliSense auto-completion via the [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) extension.

### Setup

Run the installation script to automatically install the clangd extension and generate IntelliSense configuration:

```bash
bash install
```

This will:
- Auto-install the clangd extension for VSCode/Cursor
- Generate `compile_commands.json` for IntelliSense
- Set up debugging capabilities

### Alternative: PlatformIO

You can also use the [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension for compilation:

 * Make sure you have [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
 * Click the compile button.

![image](https://github.com/user-attachments/assets/616cc35b-1736-4bb0-b53c-468580be66f4)
*Changes in non platform specific code can be tested quickly in our webcompiler by invoking the script `./wasm` at the project root*

## VSCode Debugging Guide

FastLED includes comprehensive VSCode debugging support with LLDB and Clang-generated debug symbols, providing professional-grade step-through debugging capabilities for the test suite.

**Note:** FastLED also includes built-in crash handlers that automatically provide excellent stack traces with function names, line numbers, and full call stacks. For many debugging scenarios, the automatic crash output is sufficient.

### Quick Start

1. **Prerequisites**: Ensure you have VSCode with the C/C++ extension installed
2. **Open a test file**: e.g., `tests/test_allocator.cpp`
3. **Set breakpoints**: Click the left margin next to line numbers
4. **Press F5**: Automatically debugs the corresponding test executable
5. **Debug**: Use F10 (step over), F11 (step into), F5 (continue)

### Available Debug Configurations

#### 🎯 **"Debug FastLED Test (Current File)"** ⭐ *Most Common*
- **When to use**: When you have any test file open (e.g., `test_allocator.cpp`)
- **What it does**: Automatically detects and debugs the corresponding test executable
- **How to use**: Open any `test_*.cpp` file and press `F5`

#### 🔧 **Specific Test Configurations**
- **"Debug test_allocator"** - Memory allocation and deallocation debugging
- **"Debug test_math"** - Mathematical functions and color calculations
- **"Debug test_fastled"** - Core FastLED functionality and API
- **"Debug test_hsv16"** - Color space conversions and accuracy
- **"Debug test_corkscrew"** - LED layout and geometric calculations

#### 🎛️ **Advanced Configurations**
- **"Debug with Specific Test Filter"** - Run only specific test cases
  - Example: Filter "allocator_inlined" to debug only those tests
- **"Debug with Custom Args"** - Pass custom command-line arguments
  - Example: `--verbose`, `--list-test-cases`, etc.

### Debugging Features

#### Step Commands
- **F5**: Continue execution
- **F10**: Step over (execute current line)
- **F11**: Step into (enter function calls)
- **Shift+F11**: Step out (exit current function)
- **Ctrl+Shift+F5**: Restart debugging
- **Shift+F5**: Stop debugging

#### Variable Inspection
- **Variables panel**: See all local variables and their values
- **Watch panel**: Add expressions to monitor continuously
- **Hover inspection**: Hover over variables to see values
- **Debug console**: Type expressions to evaluate

### Build Tasks for Debugging

Access via `Ctrl+Shift+P` → "Tasks: Run Task":

- **"Build FastLED Tests"** - Quick incremental build (default: `Ctrl+Shift+B`)
- **"Build FastLED Tests (Full)"** - Complete build including Python tests
- **"Build Single Test"** - Build and run specific test only
- **"Clean Build"** - Remove all build artifacts and rebuild

### Debugging Tips for FastLED

#### Memory Issues
```cpp
// Use test_allocator for memory debugging
// Set breakpoints on allocate/deallocate functions
// Watch pointer values in Variables panel
// Monitor memory patterns for corruption
```

#### Color Conversion Issues
```cpp
// Use test_hsv16 for color space debugging
// Watch RGB/HSV values during conversion
// Step through algorithms with F11
// Compare expected vs actual in Watch panel
```

#### Template Debugging
```cpp
// Clang generates excellent template debug info
// Step into template functions with F11
// Watch template parameters in Variables panel
// Use Call Stack to understand instantiation chain
```

### Technical Setup

#### Clang + LLDB Benefits
This setup provides the **best of both worlds**:
- **Clang's superior symbol generation**: Better template debugging, modern C++ support
- **LLDB's native LLVM integration**: Seamless debugging with Clang-compiled binaries
- **Cross-platform compatibility**: Works on Linux, macOS, Windows
- **Advanced features**: Python scripting, data formatters, modern C++ support

For detailed LLDB usage, see `agents/docs/lldb-debugging.md`.

#### Debug Build Configuration
The FastLED test system automatically uses optimal debug settings:
- **Compiler**: Clang via clang-tool-chain (automatically installed)
- **Debug info**: `-g3` (full debug information including macros)
- **Optimization**: `-O0` (no optimization for accurate debugging)
- **Frame pointers**: `-fno-omit-frame-pointer` (for accurate stack traces)

### Troubleshooting

#### "Program not found" Error
1. **Build first**: Run "Build FastLED Tests" task
2. **Check executable exists**: `ls tests/.build/bin/test_*`
3. **Verify path**: Ensure executable path in launch.json is correct

#### Breakpoints Not Hit
1. **Check file paths**: Ensure source file matches executable
2. **Verify compilation**: Code might be optimized out
3. **Try function breakpoints**: Sometimes more reliable than line breakpoints

#### Variables Show "Optimized Out"
1. **Use debug build**: Already configured with `-O0` (no optimization)
2. **Check variable scope**: Variable might be out of scope
3. **Try different breakpoint**: Move breakpoint to where variable is active

For complete debugging documentation, see [DEBUGGING.md](DEBUGGING.md).


## Once you are done
  * run `./test`
  * run `./lint`
  * Then submit your code via a git pull request.

## Pull Request Guidelines

**All PRs will be squash merged.** When your pull request is merged, all commits will be squashed into a single commit on the main branch. This keeps the project history clean and linear.

What this means for you:
- Feel free to make as many commits as needed during development
- No need to manually squash or rebase your commits before submitting
- The PR title and description should clearly summarize the changes (they become the squash commit message)
- All commit history within the PR will be preserved in the PR description upon merge


## Going deeper

[ADVANCED_DEVELOPMENT.md](https://github.com/FastLED/FastLED/blob/master/ADVANCED_DEVELOPMENT.md)
