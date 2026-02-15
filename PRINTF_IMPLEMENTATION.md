# fl::printf Enhancement: Octal, Width, and Flags Support

## ✅ Implementation Complete

Successfully implemented comprehensive printf format specifier support for `fl::printf`, `fl::snprintf`, and `fl::sprintf`.

## What Was Added

### 1. Octal Format (`%o`)
```cpp
fl::snprintf(buf, 10, "%o", 8);     // "10"
fl::snprintf(buf, 10, "%o", 64);    // "100"
fl::snprintf(buf, 10, "%o", 255);   // "377"
```

### 2. Width Specifier
```cpp
fl::snprintf(buf, 10, "%5d", 42);      // "   42"
fl::snprintf(buf, 20, "%10s", "test"); // "      test"
```

### 3. Zero-Padding Flag (`0`) - CRITICAL for Embedded
```cpp
fl::snprintf(buf, 10, "%02x", 0x0F);     // "0f" (was broken, now works!)
fl::snprintf(buf, 10, "%02X", 0x0F);     // "0F"
fl::snprintf(buf, 10, "%04x", 0x12);     // "0012"
fl::snprintf(buf, 20, "%08x", 0xDEADBEEF); // "deadbeef"
fl::snprintf(buf, 10, "%05d", 42);       // "00042"
```

### 4. Left-Align Flag (`-`)
```cpp
fl::snprintf(buf, 10, "%-5d", 42);      // "42   "
fl::snprintf(buf, 20, "%-10s", "test"); // "test      "
```

### 5. Plus Sign Flag (`+`)
```cpp
fl::snprintf(buf, 10, "%+d", 42);   // "+42"
fl::snprintf(buf, 10, "%+d", -42);  // "-42"
fl::snprintf(buf, 10, "%+d", 0);    // "+0"
```

### 6. Space Sign Flag (` `)
```cpp
fl::snprintf(buf, 10, "% d", 42);   // " 42"
fl::snprintf(buf, 10, "% d", -42);  // "-42"
```

### 7. Alternate Form Flag (`#`)
```cpp
fl::snprintf(buf, 10, "%#x", 0x2A);  // "0x2a"
fl::snprintf(buf, 10, "%#X", 0x2A);  // "0X2A"
fl::snprintf(buf, 10, "%#o", 8);     // "010"
fl::snprintf(buf, 10, "%#x", 0);     // "0" (no prefix for zero)
```

### 8. Combined Features
```cpp
fl::snprintf(buf, 10, "%#06x", 0x2A);       // "0x002a"
fl::snprintf(buf, 10, "%+5d", 42);          // "  +42"
fl::snprintf(buf, 10, "%-8d", 42);          // "42      "
fl::snprintf(buf, 20, "%08lx", 0xDEADBEEF); // "deadbeef"
```

## Real-World Use Case: MD5/Hash Formatting

**Before:** Had to use standard C `snprintf` for hash formatting
```cpp
// Using standard C library
#include <stdio.h>
unsigned char hash[16] = {...};
for (int i = 0; i < 16; i++) {
    snprintf(buf + i*2, 3, "%02x", hash[i]);
}
```

**After:** Can use `fl::snprintf` everywhere
```cpp
// Using fl::snprintf (works on all platforms including AVR)
#include "fl/stl/stdio.h"
unsigned char hash[16] = {...};
for (int i = 0; i < 16; i++) {
    fl::snprintf(buf + i*2, 3, "%02x", hash[i]);  // ✅ Now works!
}
```

## Test Coverage

**Test Suite:** `tests/fl/stl/cstdio.cpp`

- ✅ 10 new test cases (added pointer format test)
- ✅ 41+ individual assertions
- ✅ 100% pass rate
- ✅ All existing tests still pass (backward compatible)

**Test Results:**
```bash
bash test cstdio --cpp
✅ All tests passed (1/1 in 28.22s)

bash test stdio --cpp
✅ All tests passed (1/1 in 4.56s)
```

## Files Modified

1. **`src/fl/stl/stdio.h`** (~150 lines changed)
   - Added 5 fields to `FormatSpec`: width, left_align, zero_pad, show_sign, space_sign, alt_form
   - Rewrote `parse_format_spec()` to parse `%[flags][width][.precision][length]type`
   - Added `to_octal()` helper for octal conversion
   - Added `apply_width()` helper for width/padding
   - Rewrote `format_arg()` to build formatted strings before output

2. **`tests/fl/stl/cstdio.cpp`** (~100 lines added)
   - Added comprehensive test cases for all new features

## Format Specification Support

### Fully Supported
```
%[flags][width][.precision][length]type
```

**Flags:** `-` `+` ` ` `#` `0`
**Width:** Numeric field width (e.g., `5`, `10`, `08`)
**Precision:** `.N` for floating point (e.g., `.2f`)
**Length:** `l` `ll` `h` `hh` `L` `z` `t` `j` (parsed, not used)
**Type:** `d` `i` `u` `o` `x` `X` `f` `c` `s` `%`

### 8. Pointer Format (`%p`) ✅
```cpp
int value = 42;
int* ptr = &value;
fl::snprintf(buf, 20, "%p", ptr);  // "0x7fff5fbff8ac" (example address)

void* null_ptr = nullptr;
fl::snprintf(buf, 20, "%p", null_ptr);  // "0x0"
```

### Not Implemented (Lower Priority)
- `%e`/`%E` - Scientific notation
- `%g`/`%G` - General float
- `%n` - Character count (security risk)
- `%*d` - Dynamic width (requires runtime argument consumption in variadic templates)
- `%.*f` - Dynamic precision (requires runtime argument consumption in variadic templates)
- `%*.*f` - Both dynamic (same complexity as above)

## Performance Impact

- ✅ Minimal overhead - formatting done once per argument
- ✅ All helpers are `inline` functions
- ✅ No heap allocations for numeric formatting
- ✅ Efficient string building with `fl::string`

## Backward Compatibility

✅ **100% backward compatible** - all existing code continues to work unchanged.

## Migration Path

Code using standard `snprintf` for hex formatting can now migrate to `fl::snprintf`:

**Before:**
```cpp
#include <stdio.h>  // Standard C library
snprintf(buf, 3, "%02x", hash_byte);
```

**After:**
```cpp
#include "fl/stl/stdio.h"  // FastLED cross-platform
fl::snprintf(buf, 3, "%02x", hash_byte);
```

Benefits:
- Works on AVR (no standard library)
- Smaller code size
- Consistent behavior across platforms
- Part of FastLED's unified API

## Documentation

See `printf_analysis.md` for complete analysis and feature comparison.
