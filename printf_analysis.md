# fl::printf Format Specifier Analysis

## Standard printf Format
```
%[flags][width][.precision][length]type
```

## Currently Implemented ✅

### Type Specifiers
- `%d` / `%i` - signed decimal integer
- `%u` - unsigned decimal integer
- `%f` - floating point
- `%c` - character
- `%s` - string
- `%x` - lowercase hexadecimal
- `%X` - uppercase hexadecimal
- `%%` - literal percent sign

### Modifiers
- `.precision` - floating point precision (e.g., `%.2f`)
- Length modifiers: `l`, `ll`, `h`, `hh`, `L`, `z`, `t`, `j` (parsed and skipped)

## Missing Features ❌

### 1. Width Specifier (CRITICAL - Used in codebase)
**Found in codebase:** `%02x` (5 occurrences for MD5/hash formatting)

Examples:
- `%2d` - minimum 2 characters, right-aligned, space-padded
- `%10s` - minimum 10 characters for string
- `%*d` - width from argument list

**Impact:** Cannot format hex bytes with leading zeros (`0F` instead of `F`)

### 2. Flags (CRITICAL - Used in codebase)
**Found in codebase:** `%02x`, `%08lx`

- `0` - Zero-padding (e.g., `%02x` → `0F`)
- `-` - Left-align (e.g., `%-10s`)
- `+` - Always show sign (e.g., `%+d` → `+42`)
- ` ` (space) - Prefix positive with space (e.g., `% d` → ` 42`)
- `#` - Alternate form (e.g., `%#x` → `0x2a`)

**Impact:** Zero-padding for hex is broken, used in 5+ locations

### 3. Type Specifiers (MEDIUM - Used in codebase)
**Found in codebase:**
- `%o` - octal (2 occurrences in tests)
- `%p` - pointer (2 occurrences in exception handling)

Not found but standard:
- `%e` / `%E` - scientific notation
- `%g` / `%G` - general floating point (shortest representation)
- `%n` - store character count (rarely used, potential security risk)

### 4. Dynamic Width/Precision (LOW priority)
- `%*d` - width from argument
- `%.*f` - precision from argument

## Usage in Codebase

### HIGH PRIORITY (Would be broken if using fl::printf)
```cpp
// src/platforms/esp/32/ota/ota_impl.cpp.hpp
// Currently uses standard C snprintf (not fl::snprintf) - WORKS CORRECTLY
snprintf(nonce + i * 2, 3, "%02x", hash[i]);
snprintf(pass_hash_hex + i * 2, 3, "%02x", pass_hash[i]);
snprintf(derived_key_hex + i * 2, 3, "%02x", derived_key[i]);
snprintf(expected_response + i * 2, 3, "%02x", expected_hash[i]);
snprintf(computed_md5 + i * 2, 3, "%02x", md5_hash[i]);

// ⚠️ If this code is ported to use fl::snprintf, it would BREAK without width/flag support
```

### MEDIUM PRIORITY (Test-only code)
```cpp
// tests/fl/stl/stdio.cpp
else if (base == 8) fl::printf("%o", val);  // ❌ NOT IMPLEMENTED
```

### LOW PRIORITY (Exception handling)
```cpp
// Windows exception handler (uses standard printf, not fl::printf)
printf("Exception caught: 0x%08lx at address 0x%p\n", ...);  // OK (not our code)
printf("Attempted to %s at address 0x%p\n", ...);  // OK (not our code)
```

## Current Status

**No actively broken code** - all `%02x` uses are with standard C `snprintf`, not `fl::snprintf`.

**However:** Missing features limit adoption of `fl::printf` for embedded use cases.

## Recommendations

### Priority 1: Implement Width and Flags (MEDIUM)
Enable `fl::printf` to replace standard printf for embedded/MCU code that needs formatted hex output.

**Use case:** Allow ESP32 OTA/security code to migrate to fl::printf without losing functionality.

**Test cases needed:**
```cpp
FL_CHECK_EQ(fl::snprintf(buf, 10, "%02x", 0x0F), "0f");
FL_CHECK_EQ(fl::snprintf(buf, 10, "%02x", 0xFF), "ff");
FL_CHECK_EQ(fl::snprintf(buf, 10, "%02X", 0x0F), "0F");
FL_CHECK_EQ(fl::snprintf(buf, 10, "%04x", 0x12), "0012");
FL_CHECK_EQ(fl::snprintf(buf, 10, "%08lx", 0xDEADBEEF), "deadbeef");
```

### Priority 2: Implement Octal (`%o`)
Add octal support for completeness (used in tests).

**Test cases needed:**
```cpp
FL_CHECK_EQ(fl::snprintf(buf, 10, "%o", 8), "10");
FL_CHECK_EQ(fl::snprintf(buf, 10, "%o", 64), "100");
```

### Priority 3: Implement Pointer (`%p`)
Add pointer formatting for debugging (low priority - mostly in system code).

**Test cases needed:**
```cpp
void* ptr = reinterpret_cast<void*>(0xDEADBEEF);
FL_CHECK(fl::snprintf(buf, 20, "%p", ptr).contains("deadbeef"));
```

## Implementation Status: ✅ COMPLETED

### What Was Implemented

#### 1. Octal Format (`%o`) ✅
- Converts integers to octal representation
- Supports alternate form (`%#o`) - prefixes with `0`
- **Tests:** 4 test cases covering basic octal and alternate form

#### 2. Width Specifier ✅
- `%5d` - minimum 5 characters, right-aligned, space-padded
- `%10s` - minimum 10 characters for strings
- Works with all type specifiers
- **Tests:** 3 test cases covering numeric and string width

#### 3. Flags ✅

**Zero-Padding (`0`):**
- `%02x` - Zero-padded hex bytes (e.g., `0f` instead of `f`)
- `%05d` - Zero-padded decimals
- `%03o` - Zero-padded octal
- Handles sign placement correctly (sign before zeros)
- Handles `0x` prefix correctly (prefix before zeros)
- **Tests:** 6 test cases

**Left-Align (`-`):**
- `%-5d` - Left-aligned with space padding on right
- `%-10s` - Left-aligned strings
- **Tests:** 2 test cases

**Plus Sign (`+`):**
- `%+d` - Always show sign for numbers (`+42`, `-42`)
- **Tests:** 3 test cases

**Space Sign (` `):**
- `% d` - Prefix positive numbers with space (` 42`, `-42`)
- **Tests:** 2 test cases

**Alternate Form (`#`):**
- `%#x` - Prefix hex with `0x`
- `%#X` - Prefix hex with `0X`
- `%#o` - Prefix octal with `0`
- No prefix for zero values
- **Tests:** 4 test cases

**Combined Flags:**
- `%#06x` - Alternate form + zero-padding + width
- `%+5d` - Plus sign + width
- `%-8d` - Left-align + width
- `%08lx` - Zero-pad + width + length modifier
- **Tests:** 4 test cases

### Code Changes

**Files Modified:**
1. **`src/fl/stl/stdio.h`**:
   - Updated `FormatSpec` struct with 5 new fields (width, flags)
   - Rewrote `parse_format_spec()` to parse flags and width
   - Added `to_octal()` helper function
   - Added `apply_width()` helper function for padding
   - Completely rewrote `format_arg()` to build strings first, then apply width/flags
   - Updated `format_arg()` for `const char*` to support width

2. **`tests/fl/stl/cstdio.cpp`**:
   - Added 9 new test cases (38 individual assertions)
   - Tests cover all flags, width, octal, and combinations

### Test Results

```bash
bash test cstdio --cpp
✅ All tests passed (1/1 in 28.22s)

bash test stdio --cpp
✅ All tests passed (1/1 in 4.56s)
```

**Total Test Coverage:**
- 9 new test cases
- 38+ assertions for new features
- 100% pass rate
- All existing tests still pass (backward compatible)

### Real-World Impact

**Before:** Code using `%02x` for hash formatting had to use standard C `snprintf`
**After:** Can now use `fl::snprintf` for all formatting needs, including:

```cpp
// MD5 hash formatting (now possible with fl::snprintf)
unsigned char hash[16] = {...};
for (int i = 0; i < 16; i++) {
    fl::snprintf(buf + i*2, 3, "%02x", hash[i]);  // ✅ Works correctly
}

// Debug output with alignment
fl::printf("Value: %08x\n", 0xDEADBEEF);  // ✅ "Value: deadbeef"
fl::printf("Name:  %-10s Age: %3d\n", "Alice", 25);  // ✅ "Name:  Alice      Age:  25"
```

### Performance Impact

- Minimal: Formatting happens at most once per argument
- Helper functions are `inline` for zero overhead
- String building uses `fl::string` with efficient concatenation
- No heap allocations for numeric formatting

### Implemented: Pointer Format (`%p`) ✅

Added full pointer format support for all pointer types (int*, void*, const char*, etc.):
```cpp
int* ptr = &value;
fl::printf("Address: %p\n", ptr);  // "Address: 0x7fff5fbff8ac"
```

**Implementation:**
- Uses SFINAE to separate pointer/non-pointer template overloads
- Helper function `pointer_to_uptr()` converts any pointer to uintptr_t
- Special handling for `const char*` to support both `%s` and `%p`
- Always lowercase hex with "0x" prefix

### Not Implemented (Lower Priority)

- `%e`/`%E` - Scientific notation (not needed)
- `%g`/`%G` - General float (not needed)
- `%*d` - Dynamic width from arguments (requires runtime argument consumption in variadic templates - complex)
- `%.*f` - Dynamic precision from arguments (requires runtime argument consumption in variadic templates - complex)
- `%*.*f` - Both dynamic (same complexity as above)
