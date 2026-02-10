# FastLED Examples Agent Guidelines

## 📚 Main Documentation

**For comprehensive guidelines, read these files:**

| Topic | Read |
|-------|------|
| C++ coding standards | `docs/agents/cpp-standards.md` |
| Build commands | `docs/agents/build-system.md` |
| Testing commands | `docs/agents/testing-commands.md` |
| Debugging strategies | `docs/agents/debugging.md` |
| LLDB debugging guide | `docs/agents/lldb-debugging.md` |

**This file contains only example-directory-specific guidelines.**

---

## 🚨 CRITICAL: .INO FILE CREATION RULES

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

---

## Code Standards for Examples

### No Emoticons or Emojis
**NO emoticons or emoji characters are allowed in C++ source files.** This ensures professional, maintainable code that works correctly across all platforms and development environments.

**Examples of what NOT to do in .ino files:**
```cpp
// ❌ BAD - Emoticons in comments
// 🎯 This function handles user input

// ❌ BAD - Emoticons in log messages
FL_WARN("✅ Operation successful!");
FL_WARN("❌ Error occurred: " << error_msg);

// ❌ BAD - Emoticons in string literals
const char* status = "🔄 Processing...";
```

**Examples of correct alternatives:**
```cpp
// ✅ GOOD - Clear text in comments
// TUTORIAL: This function handles user input

// ✅ GOOD - Text prefixes in log messages
FL_WARN("SUCCESS: Operation completed successfully!");
FL_WARN("ERROR: Failed to process request: " << error_msg);

// ✅ GOOD - Descriptive text in string literals
const char* status = "PROCESSING: Request in progress...";
```

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

**📚 Reference Example:** See `examples/Json/Json.ino` for comprehensive usage patterns and API comparison.

---

## Exception Handling

**DO NOT use try-catch blocks or C++ exception handling in examples.** FastLED is designed to work on embedded systems like Arduino where exception handling may not be available or desired due to memory and performance constraints.

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

---

## Compiler Warning Suppression

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

---

## WASM Testing Requirements

**🚨 MANDATORY: Always test WASM compilation after platform file changes**

**Platform Testing Commands:**
```bash
# Test WASM platform changes (for platform developers)
uv run ci/wasm_compile.py examples/wasm --just-compile

# Quick compile test for any sketch (compile only, no browser)
uv run ci/wasm_compile.py examples/Blink --just-compile

# Quick compile test for NetTest example
uv run ci/wasm_compile.py examples/NetTest --just-compile

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

---

## Memory Refresh Rule
**🚨 ALL AGENTS: Read docs/agents/cpp-standards.md and relevant docs/agents/*.md files before concluding example work to refresh memory about .ino file creation rules and example coding standards.**
