# Bug Report: `fl::string::append(u8)` Incorrectly Converts to Number

**Date:** 2025-07-26

**Status:** Identified

## Description

A bug has been identified in the `fl::string::append(const u8&)` method where `uint8_t` values are incorrectly converted to their numeric string representation instead of being treated as ASCII characters. This causes issues in serialization processes, particularly with JSON, where string keys are mangled into a series of numbers corresponding to the ASCII values of their characters.

## Root Cause Analysis

The issue is located in the `string::append(const u8&)` method within `src/fl/str.h`:

```cpp
string &append(const u8 &c) {
    write(u16(c));
    return *this;
}
```

This implementation explicitly casts the `u8` value to a `u16`. The `write(const u16&)` method then correctly treats this as a number and appends its string representation to the `fl::string` instance. For example, the character 'k', with an ASCII value of 107, is appended as the string "107".

This behavior was confirmed by the `test_json_roundtrip` test, where the JSON key `"key"` was serialized to `"107101121"`.

## Proposed Solution

To fix this bug, the `string::append(const u8&)` method should be modified to treat the `u8` value as a character. The `u8` should be cast to a `char` before being passed to the `write` method. The corrected implementation should be:

```cpp
string &append(const u8 &c) {
    write(static_cast<char>(c));
    return *this;
}
```

This change will ensure that `u8` values are appended to the string as their corresponding ASCII characters, resolving the JSON serialization issue and other potential problems related to this incorrect conversion.
