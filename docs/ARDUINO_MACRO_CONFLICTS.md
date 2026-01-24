# Arduino Macro Conflicts - Developer Guide

## Problem

Arduino.h (and Arduino AVR in particular) defines several common function names as macros:

```cpp
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
```

These macros conflict with:
- C++ standard library functions (`std::min`, `std::max`, `std::abs`, `std::round`)
- FastLED namespace functions (`fl::numeric_limits<T>::max()`, `fl::numeric_limits<T>::min()`)
- Function definitions with these names

## Solutions

### Solution 1: Parentheses for Function Calls (Preferred for Call Sites)

**Wrap the function name in parentheses** to prevent macro expansion:

```cpp
// ❌ WRONG - macro will expand
u32 oldest_time = fl::numeric_limits<u32>::max();
// Error: macro "max" requires 2 arguments, but only 1 given

// ✅ CORRECT - parentheses prevent macro expansion
u32 oldest_time = (fl::numeric_limits<u32>::max)();
```

**Pattern:**
```cpp
// Instead of:
fl::numeric_limits<T>::max()
fl::numeric_limits<T>::min()

// Use:
(fl::numeric_limits<T>::max)()
(fl::numeric_limits<T>::min)()
```

**When to use:** Call sites where you need to invoke `max()`, `min()`, `abs()`, or `round()`.

### Solution 2: Push/Pop Macros for Function Definitions

**Use `#pragma push_macro`/`#pragma pop_macro`** around function definitions:

```cpp
// Arduino.h defines round as a macro, so we need to temporarily hide it
#pragma push_macro("round")
#undef round

float round_impl_float(float value) {
    return ::roundf(value);
}

double round_impl_double(double value) {
    return ::round(value);
}

#pragma pop_macro("round")
```

**Pattern:**
```cpp
#pragma push_macro("macro_name")  // Save current definition
#undef macro_name                 // Remove macro temporarily
// ... declare/define functions named macro_name ...
#pragma pop_macro("macro_name")   // Restore original definition
```

**When to use:** Function definitions or header sections where you need to declare functions with conflicting names.

### Solution 3: Using fl/undef.h (For Headers)

FastLED provides `fl/undef.h` which undefines common conflicting macros:

```cpp
#include "fl/undef.h"  // Undefines min, max, abs, round

// Now safe to declare functions with these names
static T min() { ... }
static T max() { ... }
```

**Note:** This permanently removes the macros in that compilation unit. Use push/pop if you need to restore them.

## Files Fixed in This Project

The following files were fixed to prevent Arduino macro conflicts:

### Using Parentheses for Calls:
- `src/fl/hash_map_lru.h` - `(fl::numeric_limits<u32>::max)()`
- `src/fl/noise_woryley.cpp.hpp` - `(fl::numeric_limits<i32>::max)()`
- `src/fl/task.cpp.hpp` - `(numeric_limits<uint32_t>::max)()` (2 locations)
- `src/fl/fx/video/pixel_stream.cpp.hpp` - `(fl::numeric_limits<int32_t>::max)()`
- `src/fl/spi/parallel_device.h` - Default parameter `(fl::numeric_limits<uint32_t>::max)()`
- `src/fl/json.cpp.hpp` - Multiple `(max)()` and `(min)()` calls
- `src/fl/traverse_grid.h` - 8 instances of `(max)()` in ternary expressions
- `src/platforms/arm/d21/spi_hw_2_samd21.cpp.hpp` - Default parameter
- `src/platforms/arm/d51/spi_hw_2_samd51.cpp.hpp` - Default parameter
- `src/platforms/arm/d51/spi_hw_4_samd51.cpp.hpp` - Default parameter
- `src/platforms/arm/nrf52/spi_hw_2_nrf52.cpp.hpp` - Comparison expressions

### Using Push/Pop Macros:
- `src/fl/stl/malloc.cpp.hpp` - `abs()` function definition
- `src/fl/stl/math.cpp.hpp` - `round_impl_float()` and `round_impl_double()` functions

### Already Using Push/Pop:
- `src/fl/stl/limits.h` - Uses push/pop around numeric_limits definitions

## Best Practices

1. **Always use parentheses** when calling `numeric_limits<T>::max()` or `::min()` in Arduino-compatible code
2. **Add a comment** explaining why parentheses are used:
   ```cpp
   // Use (max)() to prevent macro expansion by Arduino.h's max macro
   u32 value = (fl::numeric_limits<u32>::max)();
   ```
3. **Use push/pop macros** for function definitions to preserve macro state
4. **Test on Arduino AVR** platforms (Uno, Mega, etc.) to catch macro conflicts early

## Related Files

- `src/fl/undef.h` - Utility header for undefining conflicting macros
- `src/fl/stl/limits.h` - Already uses push/pop pattern for numeric_limits definitions
- `src/third_party/arduinojson/json.h` - Example of macro handling in third-party code

## References

- GitHub Issue: https://github.com/FastLED/FastLED/issues/2156
- C++ preprocessor: Parentheses prevent macro expansion
- `#pragma push_macro`/`#pragma pop_macro` - GCC/Clang extensions (also MSVC)
