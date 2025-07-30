# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FastLED is a high-performance LED driver library for Arduino, ESP32, Teensy, Raspberry Pi, and many other platforms. It supports nearly every LED chipset in existence (WS2812/NeoPixel, APA102/DotStar, etc.) and provides advanced features like parallel rendering, background processing, visual effects, and WASM compilation for web browsers.

The project has evolved from a simple LED driver into a comprehensive cross-platform framework for creating LED visualizations and interactive effects.

## MCP Server Integration

This project includes a custom MCP server (`mcp_server.py`) that provides tools for:
- Running tests with various options
- Compiling examples for different platforms  
- Code fingerprinting and change detection
- Linting and formatting
- Project information and status
- **Build info analysis for platform-specific defines, compiler flags, and toolchain information**
- **Symbol analysis for binary optimization (all platforms)**
- Stack trace setup for enhanced debugging
- **🌐 FastLED Web Compiler with Playwright (FOREGROUND AGENTS ONLY)**
- **🚨 CRITICAL: `validate_completion` tool for background agents**

To use the MCP server, run: `uv run mcp_server.py`

**BACKGROUND AGENTS:** The MCP server includes a mandatory `validate_completion` tool that MUST be used before indicating task completion. This tool runs `bash test` and ensures all tests pass.

## Architecture Overview

### Core Components

- **`src/FastLED.h`** - Main entry point and central include file
- **`src/fl/`** - Modern FastLED namespace with STL-equivalent implementations
- **`src/platforms/`** - Platform-specific implementations (ESP32, AVR, ARM, WASM, etc.)
- **`src/fx/`** - Visual effects system with 1D and 2D effects
- **`examples/`** - Comprehensive example sketches demonstrating features
- **`tests/`** - Unit test suite with CMake build system

### Key Architectural Patterns

**Custom STL Implementation**: FastLED provides its own STL equivalents under the `fl::` namespace to ensure cross-platform compatibility:
- `fl::vector`, `fl::map`, `fl::shared_ptr`, `fl::unique_ptr`
- `fl::string`, `fl::optional`, `fl::variant`
- Custom containers: `fl::hash_map`, `fl::circular_buffer`, `fl::raster`

**Platform Abstraction**: Clean separation between core functionality and platform-specific code:
- Platform detection and configuration in `src/platforms.h`
- Hardware-specific implementations in `src/platforms/{platform}/`
- Unified API across all supported platforms

**Effects System**: Modular visual effects with compositing:
- Base classes in `src/fx/fx.h` and `src/fx/fx_engine.h`
- 1D effects in `src/fx/1d/` (Fire2012, Cylon, Pride2015, etc.)
- 2D effects in `src/fx/2d/` (Animartrix, Wave, NoisePlayground)

## Development Commands

### Building and Testing
```bash
# Run comprehensive test suite (C++ unit tests + Python tests)
bash test                        # PREFERRED - handles UV internally
uv run test.py                   # DIRECT - UV managed execution

# Run specific C++ test
bash test audio_json_parsing     # PREFERRED - handles UV internally
uv run test.py audio_json_parsing # DIRECT - UV managed execution

# Quick C++ tests only (when no Python changes)
bash test --quick --cpp          # PREFERRED - handles UV internally
uv run test.py --cpp             # DIRECT - UV managed execution

# Run comprehensive linting (Python, C++, JavaScript)
bash lint                        # PREFERRED - handles UV internally

# Compile examples for specific platforms
bash compile uno --examples Blink                    # PREFERRED - handles UV internally
uv run ci/ci-compile.py uno --examples Blink        # DIRECT - UV managed execution
bash compile esp32dev --examples Blink,DemoReel100  # PREFERRED - handles UV internally
bash compile teensy31,esp32s3 --examples Blink     # PREFERRED - handles UV internally
```

### 🤖 AI AGENT EXECUTION GUIDELINES

**FOREGROUND AGENTS (Interactive Development):**
- **🚨 ALWAYS USE `bash lint` - DO NOT RUN INDIVIDUAL LINTING SCRIPTS**
- **🚨 ALWAYS USE `uv run` FOR DIRECT PYTHON EXECUTION**
- **✅ CORRECT:** `bash lint`, `uv run test.py`, `uv run ci/symbol_analysis.py`
- **❌ FORBIDDEN:** `python test.py`, `./lint-js`, `./check-js`, individual tools without UV

**BACKGROUND AGENTS (Automated/CI Environments):**
- **🚨 MUST USE `uv run` FOR ALL PYTHON SCRIPTS**
- **CAN USE FINE-GRAINED LINTING FOR SPECIFIC NEEDS**
- **BUT STILL PREFER `bash lint` FOR COMPREHENSIVE CHECKING**
- **✅ CORRECT:** `uv run ci/cpp_test_run.py --test specific_test`
- **❌ FORBIDDEN:** `python ci/cpp_test_run.py --test specific_test`

### Platform-Specific Development
```bash
# ESP32 with advanced features
bash compile esp32dev --examples Audio,NetTest,Json

# Multi-platform testing
bash compile uno,esp32dev,teensy31 --examples Blink,ColorPalette

# AVR platforms (memory-constrained)
bash compile attiny85,attiny1616 --examples Blink
```

### WASM Development
```bash
# Compile Arduino sketches to WebAssembly
uv run ci/wasm_compile.py examples/Blink --just-compile
uv run ci/wasm_compile.py examples/DemoReel100 --just-compile
uv run ci/wasm_compile.py examples/FxWave2d --just-compile

# WASM with browser testing
uv run ci/wasm_compile.py examples/NetTest -b --open
```

### Advanced Analysis
```bash
# Symbol analysis for binary optimization (CORRECTED PATHS)
uv run ci/symbol_analysis.py --board uno
uv run ci/symbol_analysis.py --board esp32dev
uv run ci/demo_symbol_analysis.py  # All platforms

# Platform build information analysis (CORRECTED PATHS)
uv run ci/build_info_analyzer.py --board esp32dev --show-defines
uv run ci/build_info_analyzer.py --compare uno esp32dev
```

## 🚨 CRITICAL REQUIREMENTS FOR BACKGROUND AGENTS 🚨

### MANDATORY PRE-COMPLETION VALIDATION
**ALL BACKGROUND AGENTS MUST FOLLOW THESE REQUIREMENTS BEFORE INDICATING COMPLETION:**

1. **🚨 ALWAYS RUN `bash test` BEFORE INDICATING COMPLETION**
   - This is MANDATORY and NON-NEGOTIABLE for all background agents
   - The command `bash test` runs the full test suite including unit tests and compilation checks
   - Background agents MUST NOT indicate they are "done" until ALL tests pass

2. **🚨 USE MCP SERVER VALIDATION TOOL**
   - Use the `validate_completion` tool from the MCP server: `uv run mcp_server.py`
   - This tool runs `bash test` and validates that all tests pass
   - Only indicate completion if this validation tool returns success

3. **🚨 ZERO TOLERANCE FOR TEST FAILURES**
   - If ANY test fails, the background agent MUST fix the issues before completion
   - Do NOT indicate completion with failing tests
   - Do NOT ignore test errors or warnings

### Background Agent Completion Checklist:
- [ ] All code changes have been made
- [ ] `bash test` has been run and ALL tests pass
- [ ] No compilation errors or warnings
- [ ] MCP server `validate_completion` tool shows success
- [ ] Only then indicate task completion

**FAILURE TO FOLLOW THESE REQUIREMENTS WILL RESULT IN BROKEN CODE SUBMISSIONS.**

## 📝 IMPORT PATTERNS AND MODULE STRUCTURE

### SUPPORTED: The `ci.ci.` import pattern - Use appropriate module structure

**✅ The `ci.ci.module_name` import pattern is now supported - Use the correct module structure:**

**✅ CORRECT PATTERNS:**
```python
from ci.ci.clang_compiler import Compiler          # CORRECT - ci.ci modules are supported
from ci.ci.symbol_analysis import analyze_symbols  # CORRECT - for ci.ci modules
from ci.ci.build_info_analyzer import get_info     # CORRECT - for ci.ci modules
import ci.ci.paths                                 # CORRECT - for ci.ci modules
```

**📝 MODULE STRUCTURE:**
```python
# For modules in ci/ci/ directory, use ci.ci prefix
from ci.ci.clang_compiler import Compiler         # For ci/ci/clang_compiler.py
from ci.ci.symbol_analysis import analyze_symbols # For ci/ci/symbol_analysis.py

# For modules directly in ci/ directory, use ci prefix
from ci.ci_compile import main                     # For ci/ci_compile.py (if exists)
import ci.module_name                              # CORRECT - simple import
```

**Why this is critical:**
- **The `ci.ci.` pattern is the #1 cause of import failures in this codebase**
- **It breaks the clean architecture separation between `ci/` (scripts) and `ci/ci/` (internal modules)**  
- **Creates "ModuleNotFoundError" that are difficult to debug**
- **Violates the established import conventions used throughout the project**

**🚨 BEFORE writing ANY import statement, verify it follows the single-level pattern: `ci.module_name` NOT `ci.ci.module_name`**

This rule was added based on direct user feedback after repeated import errors caused by the nested pattern.

## 🚨 CRITICAL: UV PROJECT EXECUTION REQUIREMENTS 🚨

### ABSOLUTELY REQUIRED: Use `uv run` for ALL Python script execution

**🚨 This is a UV project - NEVER execute Python scripts directly. Always use `uv run`:**

**❌ FORBIDDEN PATTERNS:**
```bash
python test.py                    # WRONG - missing UV
python3 ci/ci-compile.py         # WRONG - missing UV  
./ci/cpp_test_run.py             # WRONG - missing UV
python -m ci.symbol_analysis     # WRONG - missing UV
```

**✅ CORRECT PATTERNS:**
```bash
uv run test.py                   # CORRECT - UV managed execution
uv run ci/ci-compile.py          # CORRECT - UV managed execution
uv run ci/cpp_test_run.py        # CORRECT - UV managed execution
uv run ci/symbol_analysis.py    # CORRECT - UV managed execution
```

**Why this is critical:**
- **UV manages the Python environment and dependencies automatically**
- **Direct Python execution will fail with `ModuleNotFoundError` due to missing UV virtual environment**
- **UV ensures correct package versions and dependency resolution**
- **The project's `pyproject.toml` defines the required UV-managed dependencies**

**🚨 BEFORE executing ANY Python script, always prefix with `uv run`**

**Exception:** Bash wrapper scripts like `bash test` and `bash lint` are acceptable as they handle UV execution internally.

## Code Architecture Guidelines

### Naming Conventions
- **Simple objects**: lowercase (`fl::vec2f`, `fl::point`, `fl::rect`)
- **Complex objects**: CamelCase (`Raster`, `Controller`, `FxEngine`)
- **Pixel types**: ALL CAPS (`CRGB`, `CHSV`, `RGB24`)
- **Complex class members**: `mVariableName` format
- **Simple struct members**: snake_case

### Memory Management
**🚨 CRITICAL: Always use proper RAII patterns - smart pointers, moveable objects, or wrapper classes instead of raw pointers for resource management.**

**Resource Management Options:**
- ✅ **PREFERRED**: `fl::shared_ptr<T>` for shared ownership (multiple references to same object)
- ✅ **PREFERRED**: `fl::unique_ptr<T>` for exclusive ownership (single owner, automatic cleanup)
- ✅ **PREFERRED**: Moveable wrapper objects (like `fl::promise<T>`) for safe copying and transferring of unique resources
- ✅ **ACCEPTABLE**: `fl::vector<T>` storing objects by value when objects support move/copy semantics
- ❌ **AVOID**: `fl::vector<T*>` storing raw pointers - use `fl::vector<fl::shared_ptr<T>>` or `fl::vector<fl::unique_ptr<T>>`
- ❌ **AVOID**: Manual `new`/`delete` - use `fl::make_shared<T>()` or `fl::make_unique<T>()`

```cpp
// ✅ GOOD - Using smart pointers
fl::vector<fl::shared_ptr<HttpClient>> mActiveClients;
auto client = fl::make_shared<HttpClient>();
mActiveClients.push_back(client);

// ✅ GOOD - Using unique_ptr for exclusive ownership
fl::unique_ptr<HttpClient> client = fl::make_unique<HttpClient>();

// ✅ GOOD - Moveable wrapper objects (fl::promise example)
fl::vector<fl::promise<Response>> mActivePromises;  // Copyable wrapper around unique future
fl::promise<Response> promise = fetch.get(url).execute();
mActivePromises.push_back(promise);  // Safe copy, shared internal state

// ❌ BAD - Raw pointers require manual memory management
fl::vector<Promise*> mActivePromises;  // Memory leaks possible
Promise* promise = new Promise();      // Who calls delete?
```

### Platform Compatibility
- **Use `fl::` namespace equivalents** instead of std:: (e.g., `fl::vector` not `std::vector`)
- **No C++ exceptions** - use `fl::optional<T>`, return codes, or early returns
- **Use FastLED compiler control macros** from `fl/compiler_control.h` for warning suppression
- **Debug with `FL_WARN`** for consistent cross-platform output

### 🚨 CRITICAL: .INO FILE CREATION RULES 🚨

**⚠️ THINK BEFORE CREATING .INO FILES ⚠️**

**.ino files should be created SPARINGLY and ONLY when truly justified.**

### 🚫 WHEN NOT TO CREATE .INO FILES:
- **Testing minor code changes** - Use existing test files or unit tests
- **Quick API validation** - Use unit tests or modify existing examples
- **Debugging specific functions** - Use test files, not new sketches
- **One-off experiments** - Create temporary test files instead
- **Small feature tests** - Extend existing relevant examples

### ✅ WHEN TO CREATE .INO FILES:

#### 1. **Temporary Testing (.ino)**
**Use Pattern:** `temp_<feature>.ino` or `test_<api>.ino`
- ✅ **FOR:** Testing new APIs during development
- ✅ **FOR:** Quick prototyping and validation
- ✅ **DELETE AFTER USE** - These are temporary by design

#### 2. **Significant New Feature Examples**
**Use Pattern:** `examples/<FeatureName>/<FeatureName>.ino`
- ✅ **FOR:** Large, comprehensive new features
- ✅ **FOR:** APIs that warrant dedicated examples
- ✅ **FOR:** Features that users will commonly implement
- ✅ **PERMANENT** - These become part of the example library

**Remember: The examples directory is user-facing documentation. Every .ino file should provide clear value to FastLED users.**

### JSON Usage - Ideal API Patterns
**🎯 PREFERRED: Use the modern `fl::Json` class for all JSON operations.** FastLED provides an ideal JSON API that prioritizes type safety, ergonomics, and crash-proof operation.

**✅ IDIOMATIC JSON USAGE:**
```cpp
// NEW: Clean, safe, idiomatic API
fl::Json json = fl::Json::parse(jsonStr);
int brightness = json["config"]["brightness"] | 128;  // Gets value or 128 default
string name = json["device"]["name"] | string("default");  // Type-safe with default
bool enabled = json["features"]["networking"] | false;  // Never crashes

// Array operations
if (json["effects"].contains("rainbow")) {
    // Safe array checking
}
```

**❌ DISCOURAGED: Verbose legacy API:**
```cpp
// OLD: Verbose, error-prone API (still works, but not recommended)
fl::JsonDocument doc;
fl::string error;
fl::parseJson(jsonStr, &doc, &error);
int brightness = doc["config"]["brightness"].as<int>();  // Can crash if missing
```

**Key Benefits of Ideal API:**
- **🛡️ Type Safety** - No crashes on missing fields or type mismatches
- **🎯 Default Values** - Clean `json["key"] | default` syntax
- **📖 Readable Code** - 50% less boilerplate for common operations
- **🚀 Testing** - Easy test data construction with `JsonBuilder`

**📚 Reference Example:** See `examples/Json/Json.ino` for comprehensive usage patterns and API comparison.

## Testing Infrastructure

### Test System
- **Location**: `tests/` directory with CMake build system
- **Framework**: Custom test framework with doctest-style macros
- **Platforms**: Cross-platform testing (Linux, macOS, Windows)
- **Coverage**: Unit tests, integration tests, compilation tests

### Test Execution Patterns
```bash
# Always use bash test wrapper (not direct executables)
bash test                    # All tests
bash test specific_test      # Run test_specific_test.cpp
bash test --quick           # Quick mode for iterations
```

### Test Assertion Macros
**🚨 CRITICAL: Always use the proper assertion macros for better error messages and debugging:**

**Equality Assertions:**
- ✅ **CORRECT**: `CHECK_EQ(A, B)` - for equality comparisons
- ❌ **INCORRECT**: `CHECK(A == B)` - provides poor error messages

**Inequality Assertions:**
- ✅ **CORRECT**: `CHECK_LT(A, B)` - for less than comparisons
- ✅ **CORRECT**: `CHECK_LE(A, B)` - for less than or equal comparisons
- ✅ **CORRECT**: `CHECK_GT(A, B)` - for greater than comparisons
- ✅ **CORRECT**: `CHECK_GE(A, B)` - for greater than or equal comparisons

**Boolean Assertions:**
- ✅ **CORRECT**: `CHECK_TRUE(condition)` - for true conditions
- ✅ **CORRECT**: `CHECK_FALSE(condition)` - for false conditions

**String Assertions:**
- ✅ **CORRECT**: `CHECK_STREQ(str1, str2)` - for string equality
- ✅ **CORRECT**: `CHECK_STRNE(str1, str2)` - for string inequality

**Examples:**
```cpp
// Good assertion usage
CHECK_EQ(expected_value, actual_value);
CHECK_LT(current_index, max_index);
CHECK_GT(temperature, 0.0);
CHECK_TRUE(is_initialized);
CHECK_FALSE(has_error);
CHECK_STREQ("expected", actual_string);

// Bad assertion usage
CHECK(expected_value == actual_value);  // Poor error messages
CHECK(current_index < max_index);       // Poor error messages
CHECK(is_initialized);                  // Unclear intent
```

**Why:** Using the proper assertion macros provides:
- **Better error messages** with actual vs expected values
- **Clearer intent** about what is being tested
- **Consistent debugging** across all test failures
- **Type safety** for different comparison types

## Platform-Specific Notes

### ESP32 Platform
- **Parallel Output**: Supports up to 24 parallel WS2812 strips via I2S
- **Networking**: Built-in WiFi with fetch API and JSON console
- **Memory**: ~300KB RAM available for complex effects
- **Special Features**: Audio reactive effects, file system support

### Arduino AVR (UNO, etc.)
- **Memory Constraints**: ~2KB RAM, optimize for size
- **Limited Features**: Basic effects only, no networking
- **Compilation**: Use size-optimized builds automatically

### Teensy Platforms
- **High Performance**: Up to 50 parallel outputs on Teensy 4.1
- **Memory**: Abundant RAM for complex visualizations
- **Special Libraries**: OctoWS2811 support for massive displays

### WASM Platform
- **Browser Execution**: Arduino sketches run in web browsers
- **Features**: Full FastLED API, JSON fetch, UI integration
- **Development**: Docker-based compilation, local HTTP server testing
- **Integration**: Works with JavaScript frameworks and web audio APIs

## ⚠️ CRITICAL WARNING: C++ ↔ JavaScript Bindings

**🚨 EXTREMELY IMPORTANT: DO NOT MODIFY FUNCTION SIGNATURES IN WEBASSEMBLY BINDINGS WITHOUT EXTREME CAUTION! 🚨**

The FastLED project includes WebAssembly (WASM) bindings that bridge C++ and JavaScript code. **Changing function signatures in these bindings is a major source of runtime errors and build failures.**

### Key Binding Files (⚠️ HIGH RISK ZONE ⚠️):
- `src/platforms/wasm/js_bindings.cpp` - Main JavaScript interface via EM_ASM
- `src/platforms/wasm/ui.cpp` - UI update bindings with extern "C" wrappers  
- `src/platforms/wasm/active_strip_data.cpp` - Strip data bindings via EMSCRIPTEN_BINDINGS
- `src/platforms/wasm/fs_wasm.cpp` - File system bindings via EMSCRIPTEN_BINDINGS

### Before Making ANY Changes to These Files:

1. **🛑 STOP and consider if the change is absolutely necessary**
2. **📖 Read the warning comments at the top of each binding file**  
3. **🧪 Test extensively on WASM target after any changes**
4. **🔗 Verify both C++ and JavaScript sides remain synchronized**
5. **📝 Update corresponding JavaScript code if function signatures change**

**Remember: The bindings are a CONTRACT between C++ and JavaScript. Breaking this contract causes silent failures and mysterious bugs that are extremely difficult to debug.**

## Common Patterns

### LED Setup and Control
```cpp
#include "FastLED.h"
#define NUM_LEDS 100
#define DATA_PIN 6

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);
}

void loop() {
    // Fill with effects or direct pixel manipulation
    FastLED.show();
    delay(30);
}
```

### Using Effects System
```cpp
#include "fx/fx.h"
#include "fx/1d/fire2012.h"

Fire2012 fire(NUM_LEDS);
FxEngine fxEngine(NUM_LEDS);

void setup() {
    fxEngine.addFx(fire);
}

void loop() {
    fxEngine.draw(millis(), leds);
    FastLED.show();
}
```

### JSON Integration (ESP32/WASM)
```cpp
#include "fl/json.h"

void handleJsonConfig(const char* jsonStr) {
    fl::Json config = fl::Json::parse(jsonStr);
    int brightness = config["brightness"] | 128;
    bool enabled = config["effects"]["enabled"] | true;
    
    FastLED.setBrightness(brightness);
}
```

## Build System Details

### CMake Configuration
- **Location**: `tests/CMakeLists.txt` with modular `tests/cmake/` directory
- **Compiler Support**: Clang, GCC, MSVC with automatic detection
- **Linker Compatibility**: Special handling for lld-link and cross-platform linking
- **Optimization**: LTO support, parallel builds, debug symbols

**🎯 PRIMARY LOCATION for linker problems: `tests/cmake/LinkerCompatibility.cmake`**

**Key CMake Modules:**
- `LinkerCompatibility.cmake` - **🚨 CRITICAL for linker issues** - GNU↔MSVC flag translation, lld-link compatibility, warning suppression
- `CompilerDetection.cmake` - Compiler identification and toolchain setup
- `CompilerFlags.cmake` - Compiler-specific flag configuration
- `DebugSettings.cmake` - Debug symbol and optimization configuration  
- `OptimizationSettings.cmake` - LTO and performance optimization settings
- `ParallelBuild.cmake` - Parallel compilation and linker selection (mold, lld)
- `TestConfiguration.cmake` - Test target setup and configuration
- `BuildOptions.cmake` - Build option definitions and defaults

### PlatformIO Integration
- **Configuration**: `platformio.ini` with multiple environment definitions
- **Board Support**: 50+ boards with automatic toolchain setup
- **Library Management**: Automatic dependency resolution
- **Development**: VSCode integration with IntelliSense

## Troubleshooting

### Common Build Issues
1. **Missing toolchain**: Install PlatformIO platforms automatically
2. **Memory errors on AVR**: Use size-optimized examples like Blink
3. **ESP32 compilation**: Check Arduino core version (avoid 3.10, use older)
4. **WASM builds**: Ensure Docker is running and internet available

### Debug and Analysis
1. **Use symbol analysis** to identify optimization targets
2. **Check platform defines** with build info analyzer
3. **Run comprehensive tests** before submitting changes
4. **Use MCP server tools** for automated analysis and validation

## Contributing Workflow

1. **Setup**: Clone repository, run `bash test` to verify environment
2. **Development**: Make changes, test frequently with `bash test`
3. **Platform Testing**: Test on relevant platforms with `bash compile`
4. **Quality**: Run `bash lint` for code quality checks
5. **Validation**: Ensure all tests pass before submitting PR

## Important Code Standards

### Avoid std:: Prefixed Functions
**DO NOT use `std::` prefixed functions or headers in the codebase.** This project provides its own STL-equivalent implementations under the `fl::` namespace.

**Examples of what to avoid and use instead:**
- ❌ `#include <vector>` → ✅ `#include "fl/vector.h"`
- ❌ `#include <memory>` → ✅ `#include "fl/scoped_ptr.h"` or `#include "fl/ptr.h"`
- ❌ `#include <optional>` → ✅ `#include "fl/optional.h"`
- ❌ `std::move()` → ✅ `fl::move()`
- ❌ `std::vector` → ✅ `fl::vector`

**Why:** The project maintains its own implementations to ensure compatibility across all supported platforms and to avoid bloating the library with unnecessary STL dependencies.

### Compiler Warning Suppression
**ALWAYS use the FastLED compiler control macros from `fl/compiler_control.h` for warning suppression.** This ensures consistent cross-compiler support and proper handling of platform differences.

```cpp
#include "fl/compiler_control.h"

// Suppress specific warning around problematic code
FL_DISABLE_WARNING_PUSH
FL_DISABLE_FORMAT_TRUNCATION  // Use specific warning macros
// ... code that triggers warnings ...
FL_DISABLE_WARNING_POP
```

### Debug Printing
**Use `FL_WARN` for debug printing throughout the codebase.** This ensures consistent debug output that works in both unit tests and live application testing.

**Usage:**
- ✅ `FL_WARN("Debug message: " << message);`
- ❌ `FL_WARN("Value: %d", value);`

### Exception Handling
**DO NOT use try-catch blocks or C++ exception handling in the codebase.** FastLED is designed to work on embedded systems like Arduino where exception handling may not be available or desired due to memory and performance constraints.

**Use Error Handling Alternatives:**
- ✅ **Return error codes:** `bool function() { return false; }` or custom error enums
- ✅ **Optional types:** `fl::optional<T>` for functions that may not return a value
- ✅ **Assertions:** `FL_ASSERT(condition)` for debug-time validation
- ✅ **Early returns:** `if (!valid) return false;` for error conditions

## Resources

- **Documentation**: http://fastled.io/docs
- **Examples**: Comprehensive examples in `examples/` directory
- **Community**: https://reddit.com/r/FastLED
- **Issues**: http://fastled.io/issues
- **WASM Demos**: https://zackees.github.io/fastled-wasm/
