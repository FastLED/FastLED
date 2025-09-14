## Contributing

The most important part about contributing to FastLED is knowing how to test your changes.

The FastLED library includes a powerful cli that can compile to any device. It will run if you have either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) installed on the system.

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml)

The FastLED compiler cli can be invoked at the project root.

```bash (MacOS/Linux, windows us git-bsh or compile.bat)
git clone https://github.com/fastled/fastled
cd fastled
./compile uno --examples Blink  # linux/macos/git-bash
# compile.bat  # Windows.
```

## Linting and Unit Testing

```bash
./lint
./test  # runs unit tests
# Note that you do NOT need to install the C++ compiler toolchain
# for compiling + running unit tests via ./test. If `gcc` is not
# found in your system `PATH` then the `ziglang` clang compiler
# will be swapped in automatically.
````


### Testing a bunch of platforms at once.

```
./compile teensy41,teensy40 --examples Blink
./compile esp32dev,esp32s3,esp32c3,esp32c6,esp32s2 --examples Blink,Apa102HD
./compiles uno,digix,attiny85 --examples Blink,Apa102HD 
```

## Unit Tests

Shared code is unit-tested on the host machine. They can be found at `tests/` at the root of the repo. Unit testing only requires either `python` or `uv` to be installed. The C++ compiler toolchain will be installed automatically.

The easiest way to run the tests is just use `./test`

Alternatively, tests can be built and run for your development machine with CMake:

```bash
cmake -S tests -B tests/.build
ctest --test-dir tests/.build --output-on-failure
# Not that this will fail if you do not have gcc installed. When in doubt
# use ./test to compile the unit tests, as a compiler is guaranteed to be
# available via this tool.
```

## QEMU Emulation Testing

FastLED supports testing ESP32-S3 examples in QEMU emulation, providing a powerful way to validate code without physical hardware.

### Running ESP32-S3 Examples in QEMU

```bash
# Test default examples (BlinkParallel, RMT5WorkerPool)
./test --qemu esp32s3

# Test specific examples
./test --qemu esp32s3 BlinkParallel
./test --qemu esp32s3 RMT5WorkerPool BlinkParallel

# Quick validation test (setup verification only)
FASTLED_QEMU_QUICK_TEST=true ./test --qemu esp32s3
```

### What QEMU Testing Does

1. **Automatic QEMU Installation**: Downloads and sets up ESP32-S3 QEMU emulator
2. **Cross-Platform Compilation**: Builds examples for ESP32-S3 target architecture
3. **Emulated Execution**: Runs compiled firmware in QEMU virtual environment
4. **Automated Validation**: Monitors execution for success/failure indicators

### QEMU Test Output

The QEMU tests provide detailed feedback:
- **Build Status**: Compilation success/failure for each example
- **Execution Results**: Runtime behavior in emulated environment
- **Summary Statistics**: Pass/fail counts and timing information
- **Error Details**: Specific failure reasons when tests don't pass

### Supported Platforms

Currently supported QEMU platforms:
- **esp32s3**: ESP32-S3 SoC emulation

Future platforms may include additional ESP32 variants as QEMU support expands.

### Advanced QEMU Usage

```bash
# Run with verbose output to see detailed build and execution logs
./test --qemu esp32s3 --verbose

# Test in non-interactive mode (useful for CI/CD)
./test --qemu esp32s3 --no-interactive
```

## VSCode

We also support VSCode and IntelliSense auto-completion when the free [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension is installed. The development sketch to test library changes can be found at [dev/dev.ino](dev/dev.ino).

 * Make sure you have [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
 * Click the compile button.

![image](https://github.com/user-attachments/assets/616cc35b-1736-4bb0-b53c-468580be66f4)
*Changes in non platform specific code can be tested quickly in our webcompiler by invoking the script `./wasm` at the project root*

## VSCode Debugging Guide

FastLED includes comprehensive VSCode debugging support with GDB and Clang-generated debug symbols, providing professional-grade step-through debugging capabilities for the test suite.

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

#### Clang + GDB Benefits
This setup provides the **best of both worlds**:
- **Clang's superior symbol generation**: Better template debugging, modern C++ support
- **GDB's mature debugging features**: Robust breakpoint handling, memory inspection
- **Cross-platform compatibility**: Works on Linux, macOS, Windows
- **FastLED optimization**: Unified compilation testing with `FASTLED_ALL_SRC=1`

#### Debug Build Configuration
The FastLED test system automatically uses optimal debug settings:
- **Compiler**: Clang (when available) or GCC fallback
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


## Going deeper

[ADVANCED_DEVELOPMENT.md](https://github.com/FastLED/FastLED/blob/master/ADVANCED_DEVELOPMENT.md)
