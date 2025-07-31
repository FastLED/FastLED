# Log Output Formatting Enhancement

## Problem Statement
Currently, log output from FastLED's JSON processing includes repetitive file path and line number prefixes on every line, making the output verbose and harder to read. For example:

```
[uv] C:\Users\niteris\dev\fastled\src\fl\json.cpp(153): Processing double value: 3.14
[uv] C:\Users\niteris\dev\fastled\src\fl\json.cpp(160): Value 3.14 CAN be exactly represented as float
```

## Proposed Solution
Modify the logging system to group related messages under a common prefix, with subsequent messages indented. This will make logs more readable while preserving all necessary debugging information.

Example desired output:
```
[uv] C:\Users\niteris\dev\fastled\src\fl\json.cpp(153):
  Processing double value: 3.14
  Value 3.14 CAN be exactly represented as float
  Processing double value: 0.00
  Value 0.00 CAN be exactly represented as float
```

## Implementation
The change should be as simple as possible:
- Implement directly in the test runner code
- No new classes or complex infrastructure
- Use simple static variables in the test runner to track the current file/line
- Modify output formatting inline where the test messages are printed
- Keep all changes localized to the test execution code

This approach ensures:
- Minimal code changes
- No impact on the core library
- Easy to maintain
- No additional dependencies
- Simple to understand and modify

The implementation should be a few lines of code that track the current file/line and handle indentation, nothing more complex than that.

## Detailed Transformation Examples

### Current Raw Log Output (Excerpt)
```
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] test cases: 1 | 1 passed | 0 failed | 0 skipped
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] assertions: 2 | 2 passed | 0 failed |
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] Status: SUCCESS!
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] doctest version is "2.4.11"
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] run with "--help" for options
[uv run python -m ci.compiler.cpp_test_run --clang] ===============================================================================
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] test cases:  17 |  17 passed | 0 failed | 0 skipped
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] assertions: 174 | 174 passed | 0 failed |
[uv run python -m ci.compiler.cpp_test_run --clang] [doctest] Status: SUCCESS!
[uv run python -m ci.compiler.cpp_test_run --clang]   Test json.exe passed in 0.20s
[uv run python -m ci.compiler.cpp_test_run --clang] [83/89] Running test: ui_help.exe
[uv run python -m ci.compiler.cpp_test_run --clang]   Test raster.exe passed in 0.17s
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fx\video\pixel_stream.cpp(120): readFrameAt failed - read: 0, mbytesPerFrame: 300, frame:2, left: 18446744073709551316
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fx\video\pixel_stream.cpp(120): readFrameAt failed - read: 0, mbytesPerFrame: 300, frame:3, left: 18446744073709551016
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(40):
[uv run python -m ci.compiler.cpp_test_run --clang] TEST CASE:  LinePath at_subpixel
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(47): MESSAGE:
[uv run python -m ci.compiler.cpp_test_run --clang] Tile:
[uv run python -m ci.compiler.cpp_test_run --clang]   255 0
[uv run python -m ci.compiler.cpp_test_run --clang]   0 0
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(62):
[uv run python -m ci.compiler.cpp_test_run --clang] TEST CASE:  Point at exactly the middle
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(73): MESSAGE: Origin: 0, 0
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(75): MESSAGE: 64
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(76): MESSAGE: 64
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fl\json.cpp(137): Array conversion: processing 9 items
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fl\json.cpp(146): Non-numeric value found, no optimization possible
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fl\json.cpp(137): Array conversion: processing 3 items
[uv run python -m ci.compiler.cpp_test_run --clang] C:\Users\niteris\dev\fastled\src\fl\json.cpp(146): Non-numeric value found, no optimization possible
```

### Desired Transformed Output
```
=== [uv run python -m ci.compiler.cpp_test_run --clang] ===
  [doctest] test cases: 1 | 1 passed | 0 failed | 0 skipped
  [doctest] assertions: 2 | 2 passed | 0 failed |
  [doctest] Status: SUCCESS!
  [doctest] doctest version is "2.4.11"
  [doctest] run with "--help" for options
  ===============================================================================
  [doctest] test cases:  17 |  17 passed | 0 failed | 0 skipped
  [doctest] assertions: 174 | 174 passed | 0 failed |
  [doctest] Status: SUCCESS!
  Test json.exe passed in 0.20s
  [83/89] Running test: ui_help.exe
  Test raster.exe passed in 0.17s

  C:\Users\niteris\dev\fastled\src\fx\video\pixel_stream.cpp(120):
    readFrameAt failed - read: 0, mbytesPerFrame: 300, frame:2, left: 18446744073709551316
    readFrameAt failed - read: 0, mbytesPerFrame: 300, frame:3, left: 18446744073709551016

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(40):
    TEST CASE:  LinePath at_subpixel

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(47):
    MESSAGE:
    Tile:
      255 0
      0 0

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(62):
    TEST CASE:  Point at exactly the middle

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(73):
    MESSAGE: Origin: 0, 0

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(75):
    MESSAGE: 64

  C:\Users\niteris\dev\fastled\tests\test_xypath.cpp(76):
    MESSAGE: 64

  C:\Users\niteris\dev\fastled\src\fl\json.cpp(137):
    Array conversion: processing 9 items

  C:\Users\niteris\dev\fastled\src\fl\json.cpp(146):
    Non-numeric value found, no optimization possible

  C:\Users\niteris\dev\fastled\src\fl\json.cpp(137):
    Array conversion: processing 3 items

  C:\Users\niteris\dev\fastled\src\fl\json.cpp(146):
    Non-numeric value found, no optimization possible
```

## Enhanced Implementation Requirements

### 1. Command Prefix Section Headers
When a new command prefix is detected (e.g., `[uv run python -m ci.compiler.cpp_test_run --clang]`), it should be displayed as a section header:
```
=== [command] ===
```

All subsequent messages from that command should be indented under this header until a new command prefix is encountered.

### 2. File/Line Grouping Within Commands
Within each command section, messages with file/line prefixes should be grouped:
- **First occurrence** of `C:\path\file.cpp(123):` creates a sub-header
- **Subsequent messages** from the same file/line are indented under that sub-header
- **New file/line** creates a new sub-header

### 3. Multi-Level Indentation
The output should use a consistent indentation scheme:
- **Command section content**: 2 spaces
- **File/line grouped messages**: 4 spaces
- **Already indented content** (like test output): preserve existing indentation + base indent

### 4. Pattern Recognition Rules
- **Command prefix pattern**: `[command with spaces and special chars]` at start of line
- **File/line pattern**: `C:\path\file.ext(number):` or `/path/file.ext(number):`
- **Non-matching lines**: Regular messages that don't fit patterns above
- **Empty lines**: Preserved to maintain readability

### 5. State Management
- Track current command prefix for section headers
- Track current file/line prefix for message grouping
- Reset file/line grouping when command changes
- Handle mixed output from parallel processes
