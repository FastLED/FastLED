# Test for Includes After Namespace Declarations

This document describes a Python script to detect C++ files that have `#include` directives appearing after namespace declarations, which is considered bad practice.

## The Issue

Having `#include` directives after namespace declarations is problematic because:
1. It violates common C++ coding conventions
2. It can lead to confusing compilation errors
3. It makes code harder to read and maintain
4. It may cause unexpected behavior with header dependencies

## Python Script

```python
#!/usr/bin/env python3
"""
Script to check for includes after namespace declarations in C++ files.
This is used in CI/CD to prevent bad code patterns.
"""

import sys
import os
import re

def find_includes_after_namespace(file_path):
    """
    Check if a C++ file has #include directives after namespace declarations.
    
    Args:
        file_path (str): Path to the C++ file to check
        
    Returns:
        list: List of line numbers where includes appear after namespaces
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        # Skip files that can't be decoded as UTF-8
        return []
    
    namespace_started = False
    violations = []
    
    namespace_pattern = re.compile(r'^\s*(namespace\s+\w+|namespace\s*\{)')
    include_pattern = re.compile(r'^\s*#\s*include\s*[<"].*[>"]')
    
    for i, line in enumerate(lines, 1):
        # Check if we're entering a namespace
        if namespace_pattern.match(line):
            namespace_started = True
            continue
            
        # Check for includes after namespace started
        if namespace_started and include_pattern.match(line):
            violations.append(i)
            
    return violations

def scan_cpp_files(directory='.'):
    """
    Scan all C++ files in a directory for includes after namespace declarations.
    
    Args:
        directory (str): Directory to scan for C++ files
        
    Returns:
        dict: Dictionary mapping file paths to line numbers of violations
    """
    violations = {}
    
    # Find all C++ files
    cpp_extensions = ['.cpp', '.h', '.hpp', '.cc']
    skip_patterns = [
        '.venv', 
        'node_modules', 
        'build', 
        '.build',
        'third_party',
        'ziglang',
        'greenlet'
    ]
    
    for root, dirs, files in os.walk(directory):
        # Skip directories with third-party code
        if any(pattern in root for pattern in skip_patterns):
            continue
            
        for file in files:
            if any(file.endswith(ext) for ext in cpp_extensions):
                file_path = os.path.join(root, file)
                try:
                    line_numbers = find_includes_after_namespace(file_path)
                    if line_numbers:
                        violations[file_path] = line_numbers
                except Exception as e:
                    print(f"Error processing {file_path}: {e}")
    
    return violations

def main():
    """Main function to run the script."""
    print("Scanning for #include directives after namespace declarations...")
    
    violations = scan_cpp_files('.')
    
    if violations:
        print("\nFound violations:")
        print("=================")
        for file_path, line_numbers in violations.items():
            print(f"{file_path}:")
            for line_num in line_numbers:
                print(f"  Line {line_num}")
        print("\nPlease fix these issues by moving includes to the top of the file.")
        return 1  # Return error code for CI/CD
    else:
        print("\nNo violations found! All includes are properly placed before namespace declarations.")
        return 0  # Return success code

if __name__ == "__main__":
    sys.exit(main())
```

## Findings

When running the script on the FastLED codebase, several violations were found:

1. **src/fl/fetch.cpp** - Line 21: Includes WASM-specific implementation after namespace declaration
2. **src/fl/ostream.h** - Lines 10-13: Several includes appear after a namespace declaration
3. **src/fl/istream.h** - Lines 17-21: Multiple includes after namespace declaration
4. **src/fl/array.h** - Lines 185, 190: Includes after namespace declaration
5. **tests/doctest.h** - Multiple violations in this third-party testing framework

## Example Violation

In `src/fl/fetch.cpp`, we find this problematic pattern:

```cpp
namespace fl {
#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using JavaScript fetch ==========
// Include WASM-specific implementation
#include "platforms/wasm/js_fetch.h"  // BAD: Include after namespace declaration
```

Instead, it should be:

```cpp
#include "fl/fetch.h"
#include "fl/warn.h"
#include "fl/str.h"
// ... other includes at the top
#ifdef __EMSCRIPTEN__
#include "platforms/wasm/js_fetch.h"  // GOOD: Include before namespace declaration
#endif

namespace fl {
#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using JavaScript fetch ==========
```

## How to Fix Violations

### For src/fl/fetch.cpp
Move the include to the top of the file, but keep it within the `#ifdef __EMSCRIPTEN__` block:

```cpp
// ... existing includes at the top ...
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#include "platforms/wasm/js_fetch.h"  // Moved here
#endif

namespace fl {
// ... rest of the code ...
```

### For src/fl/ostream.h
Move the includes to the top of the file, before the namespace declaration:

```cpp
#pragma once

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
#ifndef FL_IO_H_INCLUDED
namespace fl {
    void print(const char* str);
}
#endif

// Move these includes to the top (before any namespace declarations)
#include "fl/str.h"
#include "fl/int.h"
#include "fl/type_traits.h"
#include "crgb.h"

namespace fl {
// ... rest of the code ...
```

### For src/fl/istream.h
Similar approach - move all includes to the top of the file:

```cpp
#pragma once

#include "fl/string.h"
#include "fl/type_traits.h"
#include "crgb.h"
// ... move any other includes here ...

namespace fl {
// ... rest of the code ...
```

## Recommendations

1. **Add to CI/CD pipeline**: Include this check in the automated testing pipeline to prevent future violations
2. **Code review guidelines**: Add this as a review checklist item
3. **Documentation**: Include this in the project's coding standards documentation
4. **Refactor existing code**: Systematically fix all current violations

## Why This Matters

Following proper include ordering:
1. Improves code readability and maintainability
2. Prevents potential compilation errors
3. Ensures consistent code style across the project
4. Makes dependency relationships clearer
5. Helps with build system optimization

By following these guidelines, the FastLED codebase will be more maintainable and less prone to subtle compilation issues.