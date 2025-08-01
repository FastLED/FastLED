# FastLED Project Rules for Cursor

## Cursor Configuration

### Post-Change Hooks
Run linting after every code change:
```yaml
post_change_hooks:
  - command: "bash lint"
    description: "Run code formatting and linting"
    working_directory: "."
```

## MCP Server Configuration
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

### FastLED Web Compiler (FOREGROUND AGENTS ONLY)

**🌐 FOR INTERACTIVE DEVELOPMENT:** The MCP server includes a `run_fastled_web_compiler` tool that:

**Note:** For direct command-line WASM compilation, see the **WASM Sketch Compilation** section below.

- **Compiles Arduino sketches to WASM** for browser execution
- **Captures console.log output** with playwright automation  
- **Takes screenshots** of running visualizations
- **Monitors FastLED_onFrame calls** to verify proper initialization
- **Provides detailed analysis** of errors and performance

**PREREQUISITES:**
- `pip install fastled` - FastLED web compiler
- `pip install playwright` - Browser automation (included in pyproject.toml)
- Docker (optional, for faster compilation)

**USAGE EXAMPLES:**
```python
# Via MCP Server - Basic usage
use run_fastled_web_compiler tool with:
- example_path: "examples/Audio"
- capture_duration: 30
- headless: false
- save_screenshot: true

# Via MCP Server - Different examples
use run_fastled_web_compiler tool with:
- example_path: "examples/Blink"
- capture_duration: 15
- headless: true

# Via MCP Server - Quick test
use run_fastled_web_compiler tool with:
- example_path: "examples/wasm"
- capture_duration: 10
```

**KEY FEATURES:**
- **Automatic browser installation:** Installs Chromium via playwright
- **Console.log capture:** Records all browser console output with timestamps
- **Error detection:** Identifies compilation failures and runtime errors
- **FastLED monitoring:** Tracks `FastLED_onFrame` calls to verify functionality
- **Screenshot capture:** Saves visualization images with timestamps
- **Docker detection:** Checks for Docker availability for faster builds
- **Background agent protection:** Automatically disabled for CI/background environments

**🚫 BACKGROUND AGENT RESTRICTION:**
This tool is **COMPLETELY DISABLED** for background agents and CI environments. Background agents attempting to use this tool will receive an error message. This is intentional to prevent:
- Hanging processes in automated environments
- Resource conflicts in CI/CD pipelines  
- Interactive browser windows in headless environments

**CONSOLE.LOG CAPTURE PATTERN:**
The tool follows the pattern established in `ci/wasm_test.py` and `ci/ci/scrapers/`:
```javascript
// Example captured console.log patterns:
[14:25:30] log: FastLED_onFrame called: {"frame":1,"leds":100}
[14:25:30] log: Audio worklet initialized
[14:25:31] error: Missing audio_worklet_processor.js
[14:25:31] warning: WebGL context lost
```

**INTEGRATION WITH EXISTING CI:**
- Complements existing `ci/wasm_test.py` functionality
- Uses same playwright patterns as `ci/ci/scrapers/`
- Leverages existing pyproject.toml dependencies
- Compatible with existing Docker-based compilation workflow

## Project Structure
- `src/` - Main FastLED library source code
- `examples/` - Arduino examples demonstrating FastLED usage
- `tests/` - Test files and infrastructure
- `ci/` - Continuous integration scripts
- `docs/` - Documentation

## Key Commands
- `uv run test.py` - Run all tests
- `uv run test.py --cpp` - Run C++ tests only
- `uv run test.py TestName` - Run specific C++ test
  - For example: running test_xypath.cpp would be uv run test.py xypath
- `./lint` - Run code formatting/linting
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
  - For example (uno): `uv run ci/ci-compile.py uno --examples Blink`
  - For example (esp32dev): `uv run ci/ci-compile.py esp32dev --examples Blink`
  - For example (esp8266): `uv run ci/ci-compile.py esp01 --examples Blink`
  - For example (teensy31): `uv run ci/ci-compile.py teensy31 --examples Blink`
- **WASM Compilation** - Compile Arduino sketches to run in web browsers:
  - `uv run ci/wasm_compile.py examples/Blink -b --open` - Compile Blink to WASM and open browser
  - `uv run ci/wasm_compile.py path/to/your/sketch -b --open` - Compile any sketch to WASM
- **Symbol Analysis** - Analyze binary size and optimization opportunities:
  - `uv run ci/ci/symbol_analysis.py --board uno` - Analyze UNO platform
  - `uv run ci/ci/symbol_analysis.py --board esp32dev` - Analyze ESP32 platform
  - `uv run ci/demo_symbol_analysis.py` - Analyze all available platforms

## 🤖 AI AGENT LINTING GUIDELINES

### FOREGROUND AGENTS (Interactive Development)
**🚨 ALWAYS USE `bash lint` - DO NOT RUN INDIVIDUAL LINTING SCRIPTS**

- **✅ CORRECT:** `bash lint`
- **❌ FORBIDDEN:** `./lint-js`, `./check-js`, `python3 scripts/enhance-js-typing.py`
- **❌ FORBIDDEN:** `uv run ruff check`, `uv run black`, individual tools

**WHY:** `bash lint` provides:
- **Comprehensive coverage** - Python, C++, JavaScript, and enhancement analysis
- **Consistent workflow** - Single command for all linting needs  
- **Proper sequencing** - Runs tools in the correct order with dependencies
- **Clear output** - Organized sections showing what's being checked
- **Agent guidance** - Shows proper usage for AI agents

### BACKGROUND AGENTS (Automated/CI Environments)
**CAN USE FINE-GRAINED LINTING FOR SPECIFIC NEEDS**

Background agents may use individual linting scripts when needed:
- `./lint-js` - JavaScript-only linting
- `./check-js` - JavaScript type checking
- `python3 scripts/enhance-js-typing.py` - JavaScript enhancement analysis
- `uv run ruff check` - Python linting only
- MCP server `lint_code` tool - Programmatic access

**BUT STILL PREFER `bash lint` FOR COMPREHENSIVE CHECKING**

### Linting Script Integration

The `bash lint` command now includes:
1. **📝 Python Linting** - ruff, black, isort, pyright
2. **🔧 C++ Linting** - clang-format (when enabled)
3. **🌐 JavaScript Linting** - Deno lint, format check, type checking
4. **🔍 JavaScript Enhancement** - Analysis and recommendations
5. **💡 AI Agent Guidance** - Clear instructions for proper usage

### When to Use Individual Scripts

**FOREGROUND AGENTS:** Never. Always use `bash lint`.

**BACKGROUND AGENTS:** Only when:
- **Debugging specific issues** with one language/tool
- **Testing incremental changes** to linting configuration
- **Running targeted analysis** for specific files
- **Integrating with automated workflows** via MCP server

## Development Guidelines
- Follow existing code style and patterns
- Run tests before committing changes
- Use the MCP server tools for common tasks
- Check examples when making API changes

## 🚨 CRITICAL: .INO FILE CREATION RULES 🚨

### ⚠️ THINK BEFORE CREATING .INO FILES ⚠️

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
```cpp
// temp_json_api.ino - Testing new JSON fetch functionality
// test_networking.ino - Validating network stack changes
```
- ✅ **FOR:** Testing new APIs during development
- ✅ **FOR:** Quick prototyping and validation
- ✅ **DELETE AFTER USE** - These are temporary by design

#### 2. **Significant New Feature Examples**
**Use Pattern:** `examples/<FeatureName>/<FeatureName>.ino`
```cpp
// examples/JsonFetchApi/JsonFetchApi.ino - Comprehensive JSON API example
// examples/NetworkStack/NetworkStack.ino - Major networking features
```
- ✅ **FOR:** Large, comprehensive new features
- ✅ **FOR:** APIs that warrant dedicated examples
- ✅ **FOR:** Features that users will commonly implement
- ✅ **PERMANENT** - These become part of the example library

### 📋 CREATION CHECKLIST:

**Before creating ANY .ino file, ask:**

1. **🤔 Is this testing a new API?**
   - YES → Create `temp_<name>.ino`, delete after testing
   - NO → Consider alternatives

2. **🤔 Is this a significant new feature that users will commonly use?**
   - YES → Create `examples/<FeatureName>/<FeatureName>.ino`
   - NO → Use existing examples or test files

3. **🤔 Can I modify an existing example instead?**
   - YES → Extend existing example rather than creating new
   - NO → Proceed with creation

4. **🤔 Is this just for debugging/validation?**
   - YES → Use unit tests or temporary test files
   - NO → Consider if it meets the "significant feature" criteria

### 🔍 REVIEW CRITERIA:

**For Feature Examples (.ino files that stay):**
- ✅ **Demonstrates complete, real-world usage patterns**
- ✅ **Covers multiple aspects of the feature comprehensively**  
- ✅ **Provides educational value for users**
- ✅ **Shows best practices and common use cases**
- ✅ **Is likely to be referenced by multiple users**

**For Temporary Testing (.ino files that get deleted):**
- ✅ **Clearly named as temporary (temp_*, test_*)**
- ✅ **Focused on specific API validation**
- ✅ **Will be deleted after development cycle**
- ✅ **Too complex for unit test framework**

### ❌ EXAMPLES OF WHAT NOT TO CREATE:
- `test_basic_led.ino` - Use existing Blink example
- `debug_colors.ino` - Use existing ColorPalette example  
- `quick_brightness.ino` - Use unit tests or modify existing example
- `validate_pins.ino` - Use PinTest example or unit tests

### ✅ EXAMPLES OF JUSTIFIED CREATIONS:
- `temp_new_wifi_api.ino` - Testing major new WiFi functionality (temporary)
- `examples/MachineLearning/MachineLearning.ino` - New ML integration feature (permanent)
- `temp_performance_test.ino` - Validating optimization changes (temporary)

### 🧹 CLEANUP RESPONSIBILITY:
- **Temporary files:** Creator must delete when testing is complete
- **Feature examples:** Must be maintained and updated as API evolves
- **Abandoned files:** Regular cleanup reviews to remove unused examples

**Remember: The examples directory is user-facing documentation. Every .ino file should provide clear value to FastLED users.**

### Memory Management with Smart Pointers and Moveable Objects
**🚨 CRITICAL: Always use proper RAII patterns - smart pointers, moveable objects, or wrapper classes instead of raw pointers for resource management.**

**Resource Management Options:**
- ✅ **PREFERRED**: `fl::shared_ptr<T>` for shared ownership (multiple references to same object)
- ✅ **PREFERRED**: `fl::unique_ptr<T>` for exclusive ownership (single owner, automatic cleanup)
- ✅ **PREFERRED**: Moveable wrapper objects (like `fl::promise<T>`) for safe copying and transferring of unique resources
- ✅ **ACCEPTABLE**: `fl::vector<T>` storing objects by value when objects support move/copy semantics
- ❌ **AVOID**: `fl::vector<T*>` storing raw pointers - use `fl::vector<fl::shared_ptr<T>>` or `fl::vector<fl::unique_ptr<T>>`
- ❌ **AVOID**: Manual `new`/`delete` - use `fl::make_shared<T>()` or `fl::make_unique<T>()`

**Moveable Wrapper Pattern:**
When you have a unique resource (like a future, file handle, or network connection) that needs to be passed around easily, create a moveable wrapper class that:
- Internally manages the unique resource (often with `fl::unique_ptr<T>` or similar)
- Provides copy semantics through shared implementation details
- Maintains clear ownership semantics
- Allows safe transfer between contexts

**Examples:**
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

// ✅ GOOD - Objects stored by value (if copyable/moveable)
fl::vector<Request> mRequests;  // When Request supports copy/move

// ❌ BAD - Raw pointers require manual memory management
fl::vector<Promise*> mActivePromises;  // Memory leaks possible
Promise* promise = new Promise();      // Who calls delete?
```

**fl::promise<T> as Moveable Wrapper Example:**
```cpp
// fl::promise<T> wraps a unique fl::future<T> but provides copyable semantics
class promise<T> {
    fl::shared_ptr<future<T>> mImpl;  // Shared wrapper around unique resource
public:
    promise(const promise& other) : mImpl(other.mImpl) {}  // Safe copying
    promise(promise&& other) : mImpl(fl::move(other.mImpl)) {}  // Move support
    // ... wrapper delegates to internal future
};

// Usage - can be copied and passed around safely
fl::promise<Response> promise = http_get_promise("https://api.example.com");
someContainer.push_back(promise);  // Copy is safe
processAsync(promise);  // Can pass to multiple places
```

**Why This Pattern:**
- **Automatic cleanup** - No memory leaks from forgotten `delete` calls
- **Exception safety** - Resources automatically freed even if exceptions occur
- **Clear ownership** - Code clearly shows who owns what objects
- **Thread safety** - Smart pointers provide atomic reference counting
- **Easy sharing** - Moveable wrappers allow safe copying of unique resources
- **API flexibility** - Can pass resources between different contexts safely

## 🔧 CMAKE BUILD SYSTEM ARCHITECTURE (DEPRECATED - NO LONGER USED)

**⚠️ IMPORTANT: CMake build system is no longer used. FastLED now uses a Python-based build system.**

### Build System Overview (Historical Reference)
FastLED previously used a sophisticated CMake build system located in `tests/cmake/` with modular configuration:

**Core Build Files:**
- `tests/CMakeLists.txt` - Main CMake entry point
- `tests/cmake/` - Modular CMake configuration directory

**Key CMake Modules:**
- `LinkerCompatibility.cmake` - **🚨 CRITICAL for linker issues** - GNU↔MSVC flag translation, lld-link compatibility, warning suppression
- `CompilerDetection.cmake` - Compiler identification and toolchain setup
- `CompilerFlags.cmake` - Compiler-specific flag configuration
- `DebugSettings.cmake` - Debug symbol and optimization configuration  
- `OptimizationSettings.cmake` - LTO and performance optimization settings
- `ParallelBuild.cmake` - Parallel compilation and linker selection (mold, lld)
- `TestConfiguration.cmake` - Test target setup and configuration
- `BuildOptions.cmake` - Build option definitions and defaults

### Linker Configuration (Most Important for Build Issues)

**🎯 PRIMARY LOCATION for linker problems: `tests/cmake/LinkerCompatibility.cmake`**

**Key Functions:**
- `apply_linker_compatibility()` - **Main entry point** - auto-detects lld-link and applies compatibility
- `translate_gnu_to_msvc_linker_flags()` - Converts GNU-style flags to MSVC-style for lld-link
- `get_dead_code_elimination_flags()` - Platform-specific dead code elimination
- `get_debug_flags()` - Debug information configuration
- `get_optimization_flags()` - Performance optimization flags

**Linker Detection Logic:**
```cmake
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    find_program(LLDLINK_EXECUTABLE lld-link)
    if(LLDLINK_EXECUTABLE)
        # Apply lld-link compatibility automatically
    endif()
endif()
```

**Common Linker Issues & Solutions:**
- **lld-link warnings**: Suppressed via `/ignore:4099` in `apply_linker_compatibility()`
- **GNU→MSVC flag translation**: Automatic in `translate_gnu_to_msvc_linker_flags()`
- **Dead code elimination**: Platform-specific via `get_dead_code_elimination_flags()`
- **Debug symbol conflicts**: Handled in `get_debug_flags()`

### Compiler Configuration

**Compiler Detection**: `tests/cmake/CompilerDetection.cmake`
- Auto-detects Clang, GCC, MSVC
- Sets up toolchain-specific configurations
- Handles cross-compilation scenarios

**Compiler Flags**: `tests/cmake/CompilerFlags.cmake`
- Warning configurations per compiler
- Optimization level management
- Platform-specific adjustments

### Build Options & Configuration

**Available Build Options** (defined in `BuildOptions.cmake`):
- `FASTLED_DEBUG_LEVEL` - Debug information level (NONE, MINIMAL, STANDARD, FULL)
- `FASTLED_OPTIMIZATION_LEVEL` - Optimization (O0, O1, O2, O3, Os, Ofast)
- `FASTLED_ENABLE_LTO` - Link-time optimization
- `FASTLED_ENABLE_PARALLEL_BUILD` - Parallel compilation
- `FASTLED_STATIC_RUNTIME` - Static runtime linking

### Testing Infrastructure

**Test Configuration**: `tests/cmake/TestConfiguration.cmake`
- Defines test targets and dependencies
- Configures test execution parameters
- Sets up coverage and profiling

**Test Execution**:
- `bash test` - Comprehensive test runner (preferred)
- `uv run test.py` - Python test interface
- Individual test executables in `.build/bin/`

### Build Troubleshooting Guide

**For Linker Issues:**
1. **Check `tests/cmake/LinkerCompatibility.cmake` first**
2. Look for lld-link detection and compatibility functions
3. Check GNU→MSVC flag translation logic
4. Verify warning suppression settings

**For Compiler Issues:**
1. Check `tests/cmake/CompilerDetection.cmake` for detection logic
2. Review `tests/cmake/CompilerFlags.cmake` for flag conflicts
3. Verify optimization settings in `OptimizationSettings.cmake`

**For Build Performance:**
1. Check `tests/cmake/ParallelBuild.cmake` for parallel settings
2. Review LTO configuration in `OptimizationSettings.cmake`
3. Verify linker selection (mold, lld, default)




## 🚨 CRITICAL REQUIREMENTS FOR ALL AGENTS (FOREGROUND & BACKGROUND) 🚨

## 🚨 MANDATORY COMMAND EXECUTION RULES 🚨

### FOREGROUND AGENTS (Interactive Development)

**FOREGROUND AGENTS MUST FOLLOW THESE COMMAND EXECUTION PATTERNS:**

#### Python Code Execution:
- ❌ **NEVER run Python code directly**
- ✅ **ALWAYS create/modify tmp.py** with your code
- ✅ **ALWAYS run: `uv run tmp.py`**

#### Shell Command Execution:
- ❌ **NEVER run shell commands directly**  
- ✅ **ALWAYS create/modify tmp.sh** with your commands
- ✅ **ALWAYS run: `bash tmp.sh`**

#### Command Execution Examples:

**Python Code Pattern:**
```python
# tmp.py
import subprocess
result = subprocess.run(['git', 'status'], capture_output=True, text=True)
print(result.stdout)
```
Then run: `uv run tmp.py`

**Shell Commands Pattern:**
```bash
# tmp.sh
#!/bin/bash
git status
ls -la
uv run ci/ci-compile.py uno --examples Blink
```
Then run: `bash tmp.sh`

### BACKGROUND AGENTS (Automated/CI Environments)

**BACKGROUND AGENTS MUST FOLLOW THESE RESTRICTED COMMAND EXECUTION PATTERNS:**

#### Python Code Execution:
- ❌ **NEVER run Python code directly**
- ❌ **NEVER create/use tmp.py files** (forbidden for background agents)
- ✅ **ALWAYS use `uv run` with existing scripts** (e.g., `uv run test.py`, `uv run ci/ci-compile.py`)
- ✅ **ALWAYS use MCP server tools** for programmatic operations when available

#### Shell Command Execution:
- ❌ **NEVER run shell commands directly**
- ❌ **NEVER create/use tmp.sh files** (forbidden for background agents)
- ✅ **ALWAYS use existing bash scripts** (e.g., `bash test`, `bash lint`)
- ✅ **ALWAYS use `uv run` for Python scripts** with proper arguments
- ✅ **ALWAYS use MCP server tools** for complex operations when available

#### Background Agent Command Examples:

**✅ ALLOWED - Using existing scripts:**
```bash
bash test
bash lint
uv run test.py audio_json_parsing
uv run ci/ci-compile.py uno --examples Blink
```

**❌ FORBIDDEN - Creating temporary files:**
```bash
# DON'T DO THIS - tmp.sh is forbidden for background agents
echo "git status" > tmp.sh
bash tmp.sh
```

**✅ PREFERRED - Using MCP server tools:**
```bash
uv run mcp_server.py
# Then use appropriate MCP tools like: validate_completion, symbol_analysis, etc.
```

### DELETE Operations - DANGER ZONE (ALL AGENTS):
- 🚨 **STOP and ask for permission** before ANY delete operations
- ✅ **EXCEPTION:** Single files that you just created are OK to delete
- ❌ **NEVER delete multiple files** without explicit permission
- ❌ **NEVER delete directories** without explicit permission
- ❌ **NEVER delete system files or project files** without permission

### Git-Bash Terminal Truncation Issue
**🚨 IMPORTANT: Git-Bash terminal may truncate the first character of commands**

**Problem:** The git-bash terminal on Windows sometimes truncates the first character of commands, causing them to fail or execute incorrectly.

**Solution:** Pre-pend commands with an additional space when using git-bash:
- ✅ **CORRECT**: ` bash test` (note the leading space)
- ❌ **INCORRECT**: `bash test` (may get truncated to `ash test`)

**Examples:**
```bash
# Good - with leading space for git-bash compatibility
 bash test
 uv run test.py
 bash lint

# Bad - may get truncated in git-bash
bash test
uv run test.py
bash lint
```

**Why:** This ensures commands work reliably across all terminal environments, especially git-bash on Windows systems.

### Why These Rules Exist:
- **Ensures all operations are reviewable and traceable**
- **Prevents accidental destructive operations in automated environments**
- **Allows for better debugging and error handling**
- **Maintains consistency across different agent types**
- **Provides audit trail for all system modifications**
- **Prevents background agents from creating unnecessary temporary files**

**These command execution rules apply to ALL operations including but not limited to:**
- File system operations (creating, modifying, deleting files)
- Git operations (commits, pushes, branch changes)
- Package installations and updates
- Build and compilation commands
- Test execution and validation
- System configuration changes


### MANDATORY MEMORY REFRESH BEFORE COMPLETION
**🚨 ALL AGENTS MUST REFRESH THEIR MEMORY BEFORE CONCLUDING WORK:**

- **The AI shall read the cursor rules again to find out what to do before task is considered complete**
  - This is MANDATORY for both foreground and background agents
  - Reading the cursor rules refreshes your memory about the latest project rules, coding standards, and requirements
  - This ensures you have current information about testing procedures, validation tools, and completion requirements
  - Do NOT indicate your work is "done" until you have refreshed your memory by reading the cursor rules

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

### C++ Design Patterns
**Shared Implementation Pattern:** When writing a lot of code that applies the same operation on a bunch of C++ objects, try and determine if those objects share a common base class or internal object. If so consider whether it's appropriate to move the implementation into a shared space.

## Code Standards

### Avoid std:: Prefixed Functions
**DO NOT use `std::` prefixed functions or headers in the codebase.** This project provides its own STL-equivalent implementations under the `fl::` namespace.

**Examples of what to avoid and use instead:**

**Headers:**

**Core Language Support:**
- ❌ `#include <type_traits>` → ✅ `#include "fl/type_traits.h"`
- ❌ `#include <algorithm>` → ✅ `#include "fl/algorithm.h"`
- ❌ `#include <functional>` → ✅ `#include "fl/functional.h"`
- ❌ `#include <initializer_list>` → ✅ `#include "fl/initializer_list.h"`

**Containers:**
- ❌ `#include <vector>` → ✅ `#include "fl/vector.h"`
- ❌ `#include <map>` → ✅ `#include "fl/map.h"`
- ❌ `#include <unordered_map>` → ✅ `#include "fl/hash_map.h"`
- ❌ `#include <unordered_set>` → ✅ `#include "fl/hash_set.h"`
- ❌ `#include <set>` → ✅ `#include "fl/set.h"`
- ❌ `#include <span>` → ✅ `#include "fl/slice.h"`

**Utilities & Smart Types:**
- ❌ `#include <optional>` → ✅ `#include "fl/optional.h"`
- ❌ `#include <variant>` → ✅ `#include "fl/variant.h"`
- ❌ `#include <utility>` → ✅ `#include "fl/pair.h"` (for std::pair)
- ❌ `#include <string>` → ✅ `#include "fl/string.h"`
- ❌ `#include <memory>` → ✅ `#include "fl/scoped_ptr.h"` or `#include "fl/ptr.h"`

**Stream/IO:**
- ❌ `#include <sstream>` → ✅ `#include "fl/sstream.h"`

**Threading:**
- ❌ `#include <thread>` → ✅ `#include "fl/thread.h"`

**Math & System:**
- ❌ `#include <cmath>` → ✅ `#include "fl/math.h"`
- ❌ `#include <cstdint>` → ✅ `#include "fl/stdint.h"`

**Functions and classes:**
- ❌ `std::move()` → ✅ `fl::move()`
- ❌ `std::forward()` → ✅ `fl::forward()`
- ❌ `std::vector` → ✅ `fl::vector`
- ❌ `std::enable_if` → ✅ `fl::enable_if`

**Why:** The project maintains its own implementations to ensure compatibility across all supported platforms and to avoid bloating the library with unnecessary STL dependencies.

**Before using any standard library functionality, check if there's a `fl::` equivalent in the `src/fl/` directory first.**

### Compiler Warning Suppression
**ALWAYS use the FastLED compiler control macros from `fl/compiler_control.h` for warning suppression.** This ensures consistent cross-compiler support and proper handling of platform differences.

**Correct Warning Suppression Pattern:**
```cpp
#include "fl/compiler_control.h"

// Suppress specific warning around problematic code
FL_DISABLE_WARNING_PUSH
FL_DISABLE_FORMAT_TRUNCATION  // Use specific warning macros
// ... code that triggers warnings ...
FL_DISABLE_WARNING_POP
```

**Available Warning Suppression Macros:**
- ✅ `FL_DISABLE_WARNING_PUSH` / `FL_DISABLE_WARNING_POP` - Standard push/pop pattern
- ✅ `FL_DISABLE_WARNING(warning_name)` - Generic warning suppression (use sparingly)
- ✅ `FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS` - Clang global constructor warnings
- ✅ `FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED` - Clang self-assignment warnings  
- ✅ `FL_DISABLE_FORMAT_TRUNCATION` - GCC format truncation warnings

**What NOT to do:**
- ❌ **NEVER use raw `#pragma` directives** - they don't handle compiler differences
- ❌ **NEVER write manual `#ifdef __clang__` / `#ifdef __GNUC__` blocks** - use the macros
- ❌ **NEVER ignore warnings without suppression** - fix the issue or suppress appropriately

**Examples:**
```cpp
// ✅ CORRECT - Using FastLED macros
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_FORMAT_TRUNCATION
// Code that triggers format truncation warnings
sprintf(small_buffer, "Very long format string %d", value);
FL_DISABLE_WARNING_POP

// ❌ INCORRECT - Raw pragma directives
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
sprintf(small_buffer, "Very long format string %d", value);
#pragma GCC diagnostic pop
```

**Why:** The FastLED compiler control system automatically handles:
- **Compiler detection** (GCC vs Clang vs MSVC)  
- **Version compatibility** (warnings that don't exist on older compilers)
- **Platform specifics** (AVR, ESP32, ARM, etc.)
- **Consistent naming** across different compiler warning systems

### Debug Printing
**Use `FL_WARN` for debug printing throughout the codebase.** This ensures consistent debug output that works in both unit tests and live application testing.

**Usage:**
- ✅ `FL_WARN("Debug message: " << message);`
- ❌ `FL_WARN("Value: %d", value);`

**Why:** `FL_WARN` provides a unified logging interface that works across all platforms and testing environments, including unit tests and Arduino sketches.

### Emoticon and Emoji Usage Policy
**NO emoticons or emoji characters are allowed in C++ source files.** This ensures professional, maintainable code that works correctly across all platforms and development environments.

**Prohibited in C++ Files:**
- ❌ **C++ source files** (*.cpp, *.c)
- ❌ **C++ header files** (*.h, *.hpp)
- ❌ **Comments in C++ files** - use clear text instead
- ❌ **String literals in C++ code** - use descriptive text
- ❌ **Log messages in C++ code** - use text prefixes like "SUCCESS:", "ERROR:", "WARNING:"

**Examples of what NOT to do in C++ files:**
```cpp
// ❌ BAD - Emoticons in comments
// 🎯 This function handles user input

// ❌ BAD - Emoticons in log messages  
FL_WARN("✅ Operation successful!");
FL_WARN("❌ Error occurred: " << error_msg);

// ❌ BAD - Emoticons in string literals
const char* status = "🔄 Processing...";
```

**Examples of correct alternatives in C++ files:**
```cpp
// ✅ GOOD - Clear text in comments
// TUTORIAL: This function handles user input

// ✅ GOOD - Text prefixes in log messages
FL_WARN("SUCCESS: Operation completed successfully!");
FL_WARN("ERROR: Failed to process request: " << error_msg);

// ✅ GOOD - Descriptive text in string literals  
const char* status = "PROCESSING: Request in progress...";
```

**Allowed in Python Files:**
- ✅ **Python source files** (*.py) - emoticons are acceptable for build scripts, tools, and utilities
- ✅ **Python comments and docstrings** - can use emoticons for clarity in development tools
- ✅ **Python log messages** - emoticons OK in build/test output for visual distinction

**Why:** 
- **Cross-platform compatibility** - Some compilers/platforms have issues with Unicode characters
- **Professional codebase** - Maintains serious, enterprise-grade appearance
- **Accessibility** - Screen readers and text-based tools work better with plain text
- **Consistency** - Ensures uniform code style across all C++ files
- **Debugging** - Text-based prefixes are easier to search and filter in logs

**Before adding any visual indicators to C++ code, use text-based alternatives like "TODO:", "NOTE:", "WARNING:", "SUCCESS:", "ERROR:" prefixes.**

### Naming Conventions
**Follow these naming conventions for consistency across the codebase:**

**Simple Objects:**
- ✅ Use lowercase class names for simple objects (e.g., `fl::vec2f`, `fl::point`, `fl::rect`)
- ❌ Avoid: `fl::Vec2f`, `fl::Point`, `fl::Rect`

**Complex Objects:**
- ✅ Use CamelCase with uppercase first character for complex objects (e.g., `Raster`, `Controller`, `Canvas`)
- ❌ Avoid: `raster`, `controller`, `canvas`

**Pixel Types:**
- ✅ Use ALL CAPS for pixel types (e.g., `CRGB`, `CHSV`, `HSV16`, `RGB24`)
- ❌ Avoid: `crgb`, `Crgb`, `chsv`, `Chsv`

**Member Variables and Functions:**

**Complex Classes/Objects:**
- ✅ **Member variables:** Use `mVariableName` format (e.g., `mPixelCount`, `mBufferSize`, `mCurrentIndex`)
- ✅ **Member functions:** Use camelCase (e.g., `getValue()`, `setPixelColor()`, `updateBuffer()`)
- ❌ Avoid: `m_variable_name`, `variableName`, `GetValue()`, `set_pixel_color()`

**Simple Classes/Structs:**
- ✅ **Member variables:** Use lowercase snake_case (e.g., `x`, `y`, `width`, `height`, `pixel_count`)
- ✅ **Member functions:** Use camelCase (e.g., `getValue()`, `setPosition()`, `normalize()`)
- ❌ Avoid: `mX`, `mY`, `get_value()`, `set_position()`

**Examples:**

```cpp
// Complex class - use mVariableName for members
class Controller {
private:
    int mPixelCount;           // ✅ Complex class member variable
    uint8_t* mBuffer;          // ✅ Complex class member variable
    bool mIsInitialized;       // ✅ Complex class member variable
    
public:
    void setPixelCount(int count);  // ✅ Complex class member function
    int getPixelCount() const;      // ✅ Complex class member function
    void updateBuffer();            // ✅ Complex class member function
};

// Simple struct - use snake_case for members
struct vec2 {
    int x;                     // ✅ Simple struct member variable
    int y;                     // ✅ Simple struct member variable
    
    float magnitude() const;   // ✅ Simple struct member function
    void normalize();          // ✅ Simple struct member function
};

struct point {
    float x;                   // ✅ Simple struct member variable
    float y;                   // ✅ Simple struct member variable
    float z;                   // ✅ Simple struct member variable
    
    void setPosition(float x, float y, float z);  // ✅ Simple struct member function
    float distanceTo(const point& other) const;   // ✅ Simple struct member function
};
```

**Why:** These conventions help distinguish between different categories of objects and maintain consistency with existing FastLED patterns. Complex classes use Hungarian notation for member variables to clearly distinguish them from local variables, while simple structs use concise snake_case for lightweight data containers.

### Container Parameter Types
**Prefer `fl::span<T>` over `fl::vector<T>` or arrays for function parameters.** `fl::span<T>` provides a non-owning view that automatically converts from various container types, making APIs more flexible and efficient.

**Examples:**
- ✅ `void processData(fl::span<const uint8_t> data)` - accepts arrays, vectors, and other containers
- ❌ `void processData(fl::vector<uint8_t>& data)` - only accepts fl::Vector
- ❌ `void processData(uint8_t* data, size_t length)` - requires manual length tracking

**Benefits:**
- **Automatic conversion:** `fl::span<T>` can automatically convert from `fl::vector<T>`, C-style arrays, and other container types
- **Type safety:** Maintains compile-time type checking while being more flexible than raw pointers
- **Performance:** Zero-cost abstraction that avoids unnecessary copying or allocation
- **Consistency:** Provides a uniform interface for working with contiguous data

**When to use `fl::vector<T>` instead:**
- When you need ownership and dynamic resizing capabilities
- When storing data as a class member that needs to persist

**Why:** Using `fl::span<T>` for parameters makes functions more reusable and avoids forcing callers to convert their data to specific container types.

### Exception Handling
**DO NOT use try-catch blocks or C++ exception handling in the codebase.** FastLED is designed to work on embedded systems like Arduino where exception handling may not be available or desired due to memory and performance constraints.

**Examples of what to avoid and use instead:**

**Avoid Exception Handling:**
- ❌ `try { ... } catch (const std::exception& e) { ... }` - Exception handling not available on many embedded platforms
- ❌ `throw std::runtime_error("error message")` - Throwing exceptions not supported
- ❌ `#include <exception>` or `#include <stdexcept>` - Exception headers not needed

**Use Error Handling Alternatives:**
- ✅ **Return error codes:** `bool function() { return false; }` or custom error enums
- ✅ **Optional types:** `fl::optional<T>` for functions that may not return a value
- ✅ **Assertions:** `FL_ASSERT(condition)` for debug-time validation
- ✅ **Early returns:** `if (!valid) return false;` for error conditions
- ✅ **Status objects:** Custom result types that combine success/failure with data

**Examples of proper error handling:**
```cpp
// Good: Using return codes
bool initializeHardware() {
    if (!setupPins()) {
        FL_WARN("Failed to setup pins");
        return false;
    }
    return true;
}

// Good: Using fl::optional
fl::optional<float> calculateValue(int input) {
    if (input < 0) {
        return fl::nullopt;  // No value, indicates error
    }
    return fl::make_optional(sqrt(input));
}

// Good: Using early returns
void processData(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        FL_WARN("Invalid input data");
        return;  // Early return on error
    }
    // Process data...
}
```

**Why:** Many embedded platforms (especially Arduino-compatible boards) don't support C++ exceptions or have them disabled to save memory and improve performance. FastLED must work reliably across all supported platforms.

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

**Testing with JsonBuilder:**
```cpp
// Easy test data construction
auto json = JsonBuilder()
    .set("brightness", 128)
    .set("enabled", true)
    .set("name", "test_device")
    .build();

// Type-safe testing
CHECK_EQ(json["brightness"] | 0, 128);
CHECK(json["enabled"] | false);
```

**🎯 GUIDELINE:** Always prefer the ideal `fl::Json` API for new code. The legacy `fl::parseJson` API remains available for backward compatibility but should be avoided in new implementations.

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

### Common Binding Errors:
- **Parameter type mismatches** (e.g., `const char*` vs `std::string`)
- **Return type changes** that break JavaScript expectations
- **Function name changes** without updating JavaScript calls
- **Missing `extern "C"` wrappers** for EMSCRIPTEN_KEEPALIVE functions
- **EMSCRIPTEN_BINDINGS macro changes** without updating JS Module calls

### If You Must Modify Bindings:
1. **Update BOTH sides simultaneously** (C++ and JavaScript)
2. **Maintain backward compatibility** when possible
3. **Add detailed comments** explaining the interface contract
4. **Test thoroughly** with real WASM builds, not just compilation
5. **Update documentation** and interface specs

**Remember: The bindings are a CONTRACT between C++ and JavaScript. Breaking this contract causes silent failures and mysterious bugs that are extremely difficult to debug.**

## 🚨 WASM PLATFORM SPECIFIC RULES 🚨

### WASM Unified Build Awareness

**🚨 CRITICAL: WASM builds use unified compilation when `FASTLED_ALL_SRC=1` is enabled (automatic for Clang builds)**

**Root Cause**: Multiple .cpp files are compiled together in a single compilation unit, causing:
- Duplicate function definitions
- Type signature conflicts  
- Symbol redefinition errors

**MANDATORY RULES:**
- **ALWAYS check for unified builds** when modifying WASM platform files
- **NEVER create duplicate function definitions** across WASM .cpp files
- **USE `EMSCRIPTEN_KEEPALIVE` functions as canonical implementations**
- **MATCH Emscripten header signatures exactly** for external C functions
- **REMOVE conflicting implementations** and add explanatory comments

**Fix Pattern Example:**
```cpp
// In timer.cpp (CANONICAL)
extern "C" {
EMSCRIPTEN_KEEPALIVE uint32_t millis() {
    // Implementation
}
}

// In js.cpp (FIXED)
extern "C" {
// NOTE: millis() and micros() functions are defined in timer.cpp with EMSCRIPTEN_KEEPALIVE  
// to avoid duplicate definitions in unified builds
}
```

### WASM Async Platform Pump Pattern

**🚨 CRITICAL: Avoid long blocking sleeps that prevent responsive async processing**

**MANDATORY RULES:**
- **AVOID long blocking sleeps** (e.g., `emscripten_sleep(100)`) in main loops
- **USE short sleep intervals** (1ms) for responsive yielding in main loops
- **ALLOW JavaScript to control timing** via extern functions rather than blocking C++ loops

**Correct Implementation:**
```cpp
// ✅ GOOD - responsive yielding without blocking
while (true) {
    // Keep pthread alive for extern function calls
    // Use short sleep for responsiveness
    emscripten_sleep(1);    // 1ms yield for responsiveness
}
```

**Why This Matters:**
- Long blocking sleeps prevent responsive browser interaction
- JavaScript should control FastLED timing via requestAnimationFrame
- Short sleep intervals maintain responsiveness while allowing other threads to work

### WASM Function Signature Matching

**🚨 CRITICAL: External C function declarations must match Emscripten headers exactly**

**Common Error Pattern:**
```cpp
// ❌ WRONG - causes compilation error
extern "C" void emscripten_sleep(int ms);

// ✅ CORRECT - matches official Emscripten header  
extern "C" void emscripten_sleep(unsigned int ms);
```

**MANDATORY RULES:**
- **ALWAYS verify against official Emscripten header signatures**
- **NEVER assume parameter types** - check the actual headers
- **UPDATE signatures immediately** when compilation errors occur

### WASM Sketch Compilation

**🎯 HOW TO COMPILE ANY ARDUINO SKETCH TO WASM:**

**Basic Compilation Commands:**
```bash
# Compile any sketch directory to WASM
uv run ci/wasm_compile.py path/to/your/sketch

# Compile with build and open browser automatically
uv run ci/wasm_compile.py path/to/your/sketch -b --open

# Compile examples/Blink to WASM
uv run ci/wasm_compile.py examples/Blink -b --open

# Compile examples/DemoReel100 to WASM  
uv run ci/wasm_compile.py examples/DemoReel100 -b --open
```

**Output:** Creates `fastled_js/` folder with:
- `fastled.js` - JavaScript loader
- `fastled.wasm` - WebAssembly binary
- `index.html` - HTML page to run the sketch

**Run in Browser:**
```bash
# Simple HTTP server to test
cd fastled_js
python -m http.server 8000
# Open http://localhost:8000
```

**🚨 REQUIREMENTS:**
- **Docker must be installed and running**
- **Internet connection** (for Docker image download on first run)
- **~1GB RAM** for Docker container during compilation

### WASM Testing Requirements

**🚨 MANDATORY: Always test WASM compilation after platform file changes**

**Platform Testing Commands:**
```bash
# Test WASM platform changes (for platform developers)
uv run ci/wasm_compile.py -b examples/wasm

# Quick test without full build
uv run ci/wasm_compile.py examples/wasm --quick
```

**Watch For These Error Patterns:**
- `error: conflicting types for 'function_name'`
- `error: redefinition of 'function_name'`  
- `warning: attribute declaration must precede definition`
- `RuntimeError: unreachable` (often async-related)

**MANDATORY RULES:**
- **ALWAYS test WASM compilation** after modifying any WASM platform files
- **USE `uv run ci/wasm_compile.py` for validation**
- **WATCH for unified build conflicts** in compilation output
- **VERIFY async operations work properly** in browser environment

### WASM Platform File Organization

**Best Practices for WASM platform files:**
- ✅ **Use `timer.cpp` for canonical timing functions** with `EMSCRIPTEN_KEEPALIVE`
- ✅ **Use `entry_point.cpp` for main() and setup/loop coordination** with async pumping
- ✅ **Use `js.cpp` for JavaScript utility functions** without duplicating timer functions
- ✅ **Include proper async infrastructure** (`fl/async.h`) in entry points
- ✅ **Comment when removing duplicate implementations** to explain unified build conflicts

## Testing
The project uses a comprehensive test suite including:
- C++ unit tests
- Platform compilation tests  
- Code quality checks (ruff, clang-format)
- Example compilation verification

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
- ❌ **INCORRECT**: `CHECK(A < B)`, `CHECK(A <= B)`, `CHECK(A > B)`, `CHECK(A >= B)`

**Boolean Assertions:**
- ✅ **CORRECT**: `CHECK_TRUE(condition)` - for true conditions
- ✅ **CORRECT**: `CHECK_FALSE(condition)` - for false conditions
- ❌ **INCORRECT**: `CHECK(condition)` - for boolean checks

**String Assertions:**
- ✅ **CORRECT**: `CHECK_STREQ(str1, str2)` - for string equality
- ✅ **CORRECT**: `CHECK_STRNE(str1, str2)` - for string inequality
- ❌ **INCORRECT**: `CHECK(str1 == str2)` - for string comparisons

**Floating Point Assertions:**
- ✅ **CORRECT**: `CHECK_DOUBLE_EQ(a, b)` - for floating point equality
- ✅ **CORRECT**: `CHECK_DOUBLE_NE(a, b)` - for floating point inequality
- ❌ **INCORRECT**: `CHECK(a == b)` - for floating point comparisons

**Examples:**
```cpp
// Good assertion usage
CHECK_EQ(expected_value, actual_value);
CHECK_LT(current_index, max_index);
CHECK_GT(temperature, 0.0);
CHECK_TRUE(is_initialized);
CHECK_FALSE(has_error);
CHECK_STREQ("expected", actual_string);
CHECK_DOUBLE_EQ(3.14159, pi_value, 0.001);

// Bad assertion usage
CHECK(expected_value == actual_value);  // Poor error messages
CHECK(current_index < max_index);       // Poor error messages
CHECK(is_initialized);                  // Unclear intent
CHECK("expected" == actual_string);     // Wrong comparison type
```

**Why:** Using the proper assertion macros provides:
- **Better error messages** with actual vs expected values
- **Clearer intent** about what is being tested
- **Consistent debugging** across all test failures
- **Type safety** for different comparison types

### Test File Creation Guidelines
**🚨 CRITICAL: Minimize test file proliferation - Consolidate tests whenever possible**

**PREFERRED APPROACH:**
- ✅ **CONSOLIDATE:** Add new test cases to existing related test files
- ✅ **EXTEND:** Expand existing `TEST_CASE()` blocks with additional scenarios
- ✅ **REUSE:** Leverage existing test infrastructure and helper functions
- ✅ **COMPREHENSIVE:** Create single comprehensive test files that cover entire feature areas

**CREATE NEW TEST FILES ONLY WHEN:**
- ✅ **Testing completely new subsystems** with no existing related tests
- ✅ **Isolated feature areas** that don't fit logically into existing test structure
- ✅ **Complex integration tests** that require dedicated setup/teardown

**AVOID:**
- ❌ **Creating new test files for minor bug fixes** - add to existing tests
- ❌ **One test case per file** - consolidate related functionality
- ❌ **Duplicate test patterns** across multiple files
- ❌ **Scattered feature testing** - keep related tests together

**EXAMPLES:**

**✅ GOOD - Consolidation:**
```cpp
// Add to existing tests/test_json_comprehensive.cpp
TEST_CASE("JSON - New Feature Addition") {
    // Add new functionality tests to existing comprehensive file
}
```

**❌ BAD - Proliferation:**
```cpp
// Don't create tests/test_json_new_feature.cpp for minor additions
// Instead add to existing comprehensive test file
```

**DEVELOPMENT WORKFLOW:**
1. **During Development/Bug Fixing:** Temporary test files are acceptable for rapid iteration
2. **Near Completion:** **MANDATORY** - Consolidate temporary tests into existing files
3. **Final Review:** Remove temporary test files and ensure comprehensive coverage in main test files

**CONSOLIDATION CHECKLIST:**
- [ ] Can this test be added to an existing `TEST_CASE` in the same file?
- [ ] Does an existing test file cover the same functional area?
- [ ] Would this test fit better as a sub-section of a comprehensive test?
- [ ] Are there duplicate test patterns that can be eliminated?

**Why:** Maintaining a clean, consolidated test suite:
- **Easier maintenance** - fewer files to manage and update
- **Better organization** - related functionality tested together
- **Faster builds** - fewer compilation units
- **Cleaner repository** - less file clutter
- **Improved discoverability** - easier to find existing test coverage

### Test Execution Format
**🚨 CRITICAL: Always use the correct test execution format:**
- ✅ **CORRECT**: `bash test <test_name>` (e.g., `bash test audio_json_parsing`)
- ❌ **INCORRECT**: `./.build/bin/test_<test_name>.exe`
- ❌ **INCORRECT**: Running executables directly

**Examples:**
- `bash test` - Run all tests (includes debug symbols)
- `bash test audio_json_parsing` - Run specific test
- `bash test xypath` - Run test_xypath.cpp
- `bash compile uno --examples Blink` - Compile examples

**Quick Build Options:**
- `bash test --quick --cpp` - Quick C++ tests only (when no *.py changes)
- `bash test --quick` - Quick tests including Python (when *.py changes)

**Why:** The `bash test` wrapper handles platform differences, environment setup, and proper test execution across all supported systems.

Use `bash test` as specified in user rules for running unit tests. For compilation tests, use `bash compile <platform> --examples <example>` (e.g., `bash compile uno --examples Blink`).

**🚨 FOR BACKGROUND AGENTS:** Running `bash test` is MANDATORY before indicating completion. Use the MCP server `validate_completion` tool to ensure all tests pass before completing any task.

## Debugging and Stack Traces

### Stack Trace Setup
The FastLED project supports enhanced debugging through stack trace functionality for crash analysis and debugging.

**For Background Agents**: Use the MCP server tool `setup_stack_traces` to automatically install and configure stack trace support:

```bash
# Via MCP server (recommended for background agents)
uv run mcp_server.py
# Then use setup_stack_traces tool with method: "auto"
```

**Manual Installation**:

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install -y libunwind-dev build-essential
```

**CentOS/RHEL/Fedora**:
```bash
sudo yum install -y libunwind-devel gcc-c++  # CentOS/RHEL  
sudo dnf install -y libunwind-devel gcc-c++  # Fedora
```

**macOS**:
```bash
brew install libunwind
```

### Available Stack Trace Methods
1. **LibUnwind** (Recommended) - Enhanced stack traces with symbol resolution
2. **Execinfo** (Fallback) - Basic stack traces using standard glibc
3. **Windows** (On Windows) - Windows-specific debugging APIs
4. **No-op** (Last resort) - Minimal crash handling

The build system automatically detects and configures the best available option.

### Testing Stack Traces
```bash
# Note: Stack trace testing now uses Python build system
# CMake commands are deprecated
cd tests
cmake . && make crash_test_standalone crash_test_execinfo  # DEPRECATED

# Test libunwind version
./.build/bin/crash_test_standalone manual   # Manual stack trace
./.build/bin/crash_test_standalone nullptr  # Crash test

# Test execinfo version  
./.build/bin/crash_test_execinfo manual     # Manual stack trace
./.build/bin/crash_test_execinfo nullptr    # Crash test
```

### Using in Code
```cpp
#include "tests/crash_handler.h"

int main() {
    setup_crash_handler();  // Enable crash handling
    // Your code here...
    return 0;
}
```

**For Background Agents**: Always run the `setup_stack_traces` MCP tool when setting up a new environment to ensure proper debugging capabilities are available.

## Platform Build Information Analysis

The FastLED project includes comprehensive tools for analyzing platform-specific build information, including preprocessor defines, compiler flags, and toolchain paths from `build_info.json` files generated during compilation.

### Overview

Platform build information is stored in `.build/{platform}/build_info.json` files that are automatically generated when compiling for any platform. These files contain:

- **Platform Defines** - Preprocessor definitions (`#define` values) specific to each platform
- **Compiler Information** - Paths, flags, and types for C/C++ compilers
- **Toolchain Aliases** - Tool paths for gcc, g++, ar, objcopy, nm, etc.
- **Build Configuration** - Framework, build type, and other settings

### Quick Start

#### 1. Generate Build Information

Before analyzing, ensure you have compiled the platform:

```bash
# Compile a platform to generate build_info.json
uv run ci/ci-compile.py uno --examples Blink
uv run ci/ci-compile.py esp32dev --examples Blink
```

This creates `.build/{platform}/build_info.json` with all platform information.

#### 2. Analyze Platform Defines

**Get platform-specific preprocessor defines:**
```bash
# Command line tool
python3 ci/ci/build_info_analyzer.py --board uno --show-defines

# Via MCP server
uv run mcp_server.py
# Use build_info_analysis tool with: board="uno", show_defines=true
```

**Example output for UNO:**
```
📋 Platform Defines for UNO:
  PLATFORMIO=60118
  ARDUINO_AVR_UNO
  F_CPU=16000000L
  ARDUINO_ARCH_AVR
  ARDUINO=10808
  __AVR_ATmega328P__
```

**Example output for ESP32:**
```
📋 Platform Defines for ESP32DEV:
  ESP32=ESP32
  ESP_PLATFORM
  F_CPU=240000000L
  ARDUINO_ARCH_ESP32
  IDF_VER="v5.3.2-174-g083aad99cf-dirty"
  ARDUINO_ESP32_DEV
  # ... and 23 more defines
```

#### 3. Compare Platforms

**Compare defines between platforms:**
```bash
# Command line
python3 ci/ci/build_info_analyzer.py --compare uno esp32dev

# Via MCP 
# Use build_info_analysis tool with: board="uno", compare_with="esp32dev"
```

Shows differences and commonalities between platform defines.

### Available Tools

#### Command Line Tool

**Basic Usage:**
```bash
# List available platforms
python3 ci/ci/build_info_analyzer.py --list-boards

# Show platform defines
python3 ci/ci/build_info_analyzer.py --board uno --show-defines

# Show compiler information
python3 ci/ci/build_info_analyzer.py --board esp32dev --show-compiler

# Show toolchain aliases
python3 ci/ci/build_info_analyzer.py --board teensy31 --show-toolchain

# Show everything
python3 ci/ci/build_info_analyzer.py --board digix --show-all

# Compare platforms
python3 ci/ci/build_info_analyzer.py --compare uno esp32dev

# JSON output for automation
python3 ci/ci/build_info_analyzer.py --board uno --show-defines --json
```

#### MCP Server Tool

**For Background Agents**, use the MCP server `build_info_analysis` tool:

```bash
# Start MCP server
uv run mcp_server.py

# Use build_info_analysis tool with parameters:
# - board: "uno", "esp32dev", "teensy31", etc. or "list" to see available
# - show_defines: true/false (default: true)
# - show_compiler: true/false  
# - show_toolchain: true/false
# - show_all: true/false
# - compare_with: "other_board_name" for comparison
# - output_json: true/false for programmatic use
```

### Supported Platforms

The build info analysis works with **ANY platform** that generates a `build_info.json` file:

**Embedded Platforms:**
- ✅ **UNO (AVR)** - 8-bit microcontroller (6 defines)
- ✅ **ESP32DEV** - WiFi-enabled 32-bit platform (29 defines)
- ✅ **ESP32S3, ESP32C3, ESP32C6** - All ESP32 variants
- ✅ **TEENSY31, TEENSYLC** - ARM Cortex-M platforms
- ✅ **DIGIX, BLUEPILL** - ARM Cortex-M3 platforms
- ✅ **STM32, NRF52** - Various ARM platforms
- ✅ **RPIPICO, RPIPICO2** - Raspberry Pi Pico platforms
- ✅ **ATTINY85, ATTINY1616** - Small AVR microcontrollers

### Use Cases

#### For Code Development

**Understanding Platform Differences:**
```bash
# See what defines are available for conditional compilation
python3 ci/ci/build_info_analyzer.py --board esp32dev --show-defines

# Compare two platforms to understand differences
python3 ci/ci/build_info_analyzer.py --compare uno esp32dev
```

**Compiler and Toolchain Information:**
```bash
# Get compiler paths and flags for debugging builds
python3 ci/ci/build_info_analyzer.py --board teensy31 --show-compiler

# Get toolchain paths for symbol analysis
python3 ci/ci/build_info_analyzer.py --board digix --show-toolchain
```

#### For Automation

**JSON output for scripts:**
```bash
# Get defines as JSON for processing
python3 ci/ci/build_info_analyzer.py --board uno --show-defines --json

# Get all build info as JSON
python3 ci/ci/build_info_analyzer.py --board esp32dev --show-all --json
```

### Integration with Other Tools

Build info analysis integrates with other FastLED tools:

1. **Symbol Analysis** - Uses build_info.json to find toolchain paths
2. **Compilation** - Generated automatically during example compilation  
3. **Testing** - Provides platform context for test results

### Troubleshooting

**Common Issues:**

1. **"No boards with build_info.json found"**
   - **Solution:** Compile a platform first: `uv run ci/ci-compile.py {board} --examples Blink`

2. **"Board key not found in build_info.json"**
   - **Solution:** Check available boards: `python3 ci/ci/build_info_analyzer.py --list-boards`

3. **"Build info not found for platform"**
   - **Solution:** Ensure the platform compiled successfully and check `.build/{board}/build_info.json` exists

### Best Practices

1. **Always compile first** before analyzing build information
2. **Use comparison feature** to understand platform differences
3. **Check defines** when adding platform-specific code
4. **Use JSON output** for automated processing and CI/CD
5. **Combine with symbol analysis** for complete platform understanding

**For Background Agents:** Use the MCP server `build_info_analysis` tool for consistent access to platform build information and proper error handling.

## Symbol Analysis for Binary Optimization

The FastLED project includes comprehensive symbol analysis tools to identify optimization opportunities and understand binary size allocation across all supported platforms.

### Overview

Symbol analysis examines compiled ELF files to:
- **Identify large symbols** that may be optimization targets
- **Understand memory allocation** across different code sections
- **Find unused features** that can be eliminated to reduce binary size
- **Compare symbol sizes** between different builds or platforms
- **Provide optimization recommendations** based on actual usage patterns

### Supported Platforms

The symbol analysis tools work with **ANY platform** that generates a `build_info.json` file, including:

**Embedded Platforms:**
- ✅ **UNO (AVR)** - Small 8-bit microcontroller (typically ~3-4KB symbols)
- ✅ **ESP32DEV (Xtensa)** - WiFi-enabled 32-bit platform (typically ~200-300KB symbols)
- ✅ **ESP32S3, ESP32C3, ESP32C6, etc.** - All ESP32 variants supported

**ARM Platforms:**
- ✅ **TEENSY31 (ARM Cortex-M4)** - High-performance 32-bit (typically ~10-15KB symbols)
- ✅ **TEENSYLC (ARM Cortex-M0+)** - Low-power ARM platform (typically ~8-10KB symbols)
- ✅ **DIGIX (ARM Cortex-M3)** - Arduino Due compatible (typically ~15-20KB symbols)
- ✅ **STM32 (ARM Cortex-M3)** - STMicroelectronics platform (typically ~12-18KB symbols)

**And many more!** Any platform with toolchain support and `build_info.json` generation.

### Quick Start

#### 1. Prerequisites

Before running symbol analysis, ensure you have compiled the platform:

```bash
# Compile platform first (required step)
uv run ci/ci-compile.py uno --examples Blink
uv run ci/ci-compile.py esp32dev --examples Blink
uv run ci/ci-compile.py teensy31 --examples Blink
```

This generates the required `.build/{platform}/build_info.json` file and ELF binary.

#### 2. Run Symbol Analysis

**Analyze specific platform:**
```bash
uv run ci/ci/symbol_analysis.py --board uno
uv run ci/ci/symbol_analysis.py --board esp32dev
uv run ci/ci/symbol_analysis.py --board teensy31
```

**Analyze all available platforms at once:**
```bash
uv run ci/demo_symbol_analysis.py
```

**Using MCP Server (Recommended for Background Agents):**
```bash
# Use MCP server tools
uv run mcp_server.py
# Then use symbol_analysis tool with board: "uno" or "auto"
# Or use symbol_analysis tool with run_all_platforms: true
```

### Analysis Output

Symbol analysis provides detailed reports including:

**Summary Information:**
- **Total symbols count** and **total size**
- **Symbol type breakdown** (text, data, bss, etc.)
- **Platform-specific details** (toolchain, ELF file location)

**Largest Symbols List:**
- **Top symbols by size** for optimization targeting
- **Demangled C++ names** for easy identification
- **Size in bytes** and **percentage of total**

**Optimization Recommendations:**
- **Feature analysis** - unused features that can be disabled
- **Size impact estimates** - potential savings from removing features
- **Platform-specific suggestions** based on symbol patterns

### Example Analysis Results

**UNO Platform (Small embedded):**
```
================================================================================
UNO SYMBOL ANALYSIS REPORT
================================================================================

SUMMARY:
  Total symbols: 51
  Total symbol size: 3767 bytes (3.7 KB)

LARGEST SYMBOLS (sorted by size):
   1.  1204 bytes - ClocklessController<...>::showPixels(...)
   2.   572 bytes - CFastLED::show(unsigned char)
   3.   460 bytes - main
   4.   204 bytes - CPixelLEDController<...>::show(...)
```

**ESP32 Platform (Feature-rich):**
```
================================================================================
ESP32DEV SYMBOL ANALYSIS REPORT  
================================================================================

SUMMARY:
  Total symbols: 2503
  Total symbol size: 237092 bytes (231.5 KB)

LARGEST SYMBOLS (sorted by size):
   1. 12009 bytes - _vfprintf_r
   2. 11813 bytes - _svfprintf_r
   3.  8010 bytes - _vfiprintf_r
   4.  4192 bytes - port_IntStack
```

### Advanced Features

#### JSON Output
Save detailed analysis results for programmatic processing:
```bash
uv run symbol_analysis.py --board esp32dev --output-json
# Results saved to: .build/esp32dev_symbol_analysis.json
```

#### Batch Analysis
Analyze multiple platforms in sequence:
```bash
for board in uno esp32dev teensy31; do
    uv run symbol_analysis.py --board $board
done
```

#### Integration with Build Systems
The symbol analysis can be integrated into automated build processes:
```python
# In your Python build script
import subprocess
result = subprocess.run(['uv', 'run', 'symbol_analysis.py', '--board', 'uno'], 
                       capture_output=True, text=True)
```

### MCP Server Tools

**For Background Agents**, use the MCP server tools for symbol analysis:

1. **Generic Symbol Analysis** (`symbol_analysis` tool):
   - Works with **any platform** (UNO, ESP32, Teensy, STM32, etc.)
   - Auto-detects available platforms from `.build/` directory
   - Can analyze single platform or all platforms simultaneously
   - Provides comprehensive usage instructions

2. **ESP32-Specific Analysis** (`esp32_symbol_analysis` tool):
   - **ESP32 platforms only** with FastLED-focused filtering
   - Includes feature analysis and optimization recommendations
   - FastLED-specific symbol identification and grouping

**Usage via MCP:**
```bash
# Start MCP server
uv run mcp_server.py

# Use symbol_analysis tool with parameters:
# - board: "uno", "esp32dev", or "auto"
# - run_all_platforms: true/false
# - output_json: true/false
```

### Troubleshooting

**Common Issues:**

1. **"build_info.json not found"**
   - **Solution:** Compile the platform first: `uv run ci/ci-compile.py {board} --examples Blink`

2. **"ELF file not found"**
   - **Solution:** Ensure compilation completed successfully and check `.build/{board}/` directory

3. **"Tool not found"** (nm, c++filt, etc.)
   - **Solution:** The platform toolchain isn't installed or configured properly
   - **Check:** Verify PlatformIO platform installation: `uv run pio platform list`

4. **"No symbols found"**
   - **Solution:** The ELF file may be stripped or compilation failed
   - **Check:** Verify ELF file exists and has debug symbols

**Debug Mode:**
```bash
# Run with verbose Python output for debugging
uv run python -v ci/ci/symbol_analysis.py --board uno
```

### Best Practices

1. **Always compile first** before running symbol analysis
2. **Use consistent examples** (like Blink) for size comparisons
3. **Run analysis on clean builds** to avoid cached/incremental build artifacts
4. **Compare results across platforms** to understand feature scaling
5. **Focus on the largest symbols first** for maximum optimization impact
6. **Use JSON output for automated processing** and trend analysis

**For Background Agents:** Always use the MCP server `symbol_analysis` tool for consistent results and proper error handling.
