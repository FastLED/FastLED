# C++ Code Standards

## Namespace and Headers
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Vector type usage**: Use `fl::vector<T>` for dynamic arrays. The implementation is in `src/fl/stl/vector.h`.

## Platform Dispatch Headers
- FastLED uses dispatch headers in `src/platforms/` (e.g., `int.h`, `io_arduino.h`) that route to platform-specific implementations via coarse-to-fine detection. See `src/platforms/README.md` for details.
- **Platform-specific headers (`src/platforms/**`)**: Header files typically do NOT need platform guards (e.g., `#ifdef ESP32`). Only the `.cpp` implementation files require guards. When the `.cpp` file is guarded from compilation, the header won't be included. This approach provides better IDE code assistance and IntelliSense support.
  - Correct pattern:
    - `header.h`: No platform guards (clean interface)
    - `header.cpp`: Has platform guards (e.g., `#ifdef ESP32 ... #endif`)
  - Avoid: Adding `#ifdef ESP32` to both header and implementation files (degrades IDE experience)

## Span Usage
- **Automatic span conversion**: `fl::span<T>` has implicit conversion constructors - you don't need explicit `fl::span<T>(...)` wrapping in function calls. Example:
  - Correct: `verifyPixels8bit(output, leds)` (implicit conversion)
  - Verbose: `verifyPixels8bit(output, fl::span<const CRGB>(leds, 3))` (unnecessary explicit wrapping)
- **Prefer passing and returning by span**: Use `fl::span<T>` or `fl::span<const T>` for function parameters and return types unless a copy of the source data is required:
  - Preferred: `fl::span<const uint8_t> getData()` (zero-copy view)
  - Preferred: `void process(fl::span<const CRGB> pixels)` (accepts arrays, vectors, etc.)
  - Avoid: `std::vector<uint8_t> getData()` (unnecessary copy)
  - Use `fl::span<const T>` for read-only views to prevent accidental modification

## Macro Definition Patterns

### Type 1: Platform/Feature Detection Macros (defined/undefined pattern)

**Platform Identification Naming Convention:**
- MUST follow pattern: `FL_IS_<PLATFORM><_OPTIONAL_VARIANT>`
- Correct: `FL_IS_STM32`, `FL_IS_STM32_F1`, `FL_IS_STM32_H7`, `FL_IS_ESP_32S3`
- Wrong: `FASTLED_STM32_F1`, `FASTLED_STM32` (missing `FL_IS_` prefix)
- Wrong: `FL_STM32_F1`, `IS_STM32_F1` (incorrect prefix pattern)

**Detection and Usage:**
- Platform defines like `FL_IS_ARM`, `FL_IS_STM32`, `FL_IS_ESP32` and their variants
- Feature detection like `FASTLED_STM32_HAS_TIM5`, `FASTLED_STM32_DMA_CHANNEL_BASED` (not platform IDs)
- These are either **defined** (set) or **undefined** (unset) - NO values
- Correct: `#ifdef FL_IS_STM32` or `#ifndef FL_IS_STM32`
- Correct: `#if defined(FL_IS_STM32)` or `#if !defined(FL_IS_STM32)`
- Define as: `#define FL_IS_STM32` (no value)
- Wrong: `#if FL_IS_STM32` or `#if FL_IS_STM32 == 1`
- Wrong: `#define FL_IS_STM32 1` (do not assign values)

### Type 2: Configuration Macros with Defaults (0/1 or numeric values)
- Settings that have a default when undefined (e.g., `FASTLED_USE_PROGMEM`, `FASTLED_ALLOW_INTERRUPTS`)
- Numeric constants (e.g., `FASTLED_STM32_GPIO_MAX_FREQ_MHZ 100`, `FASTLED_STM32_DMA_TOTAL_CHANNELS 14`)
- These MUST have explicit values: 0, 1, or numeric constants
- Correct: `#define FASTLED_USE_PROGMEM 1` or `#define FASTLED_USE_PROGMEM 0`
- Correct: `#define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 100`
- Check as: `#if FASTLED_USE_PROGMEM` or `#if FASTLED_USE_PROGMEM == 1`
- Wrong: `#define FASTLED_USE_PROGMEM` (missing value - ambiguous default behavior)

## Warning and Debug Output
- **Use proper warning macros** from `fl/compiler_control.h`
- **Use `FL_DBG("message" << var)`** for debug prints (easily stripped in release builds)
- **Use `FL_WARN("message" << var)`** for warnings (persist into release builds)
- **Avoid `fl::printf`, `fl::print`, `fl::println`** - prefer FL_DBG/FL_WARN macros instead
- Note: FL_DBG and FL_WARN use stream-style `<<` operator, NOT printf-style formatting

## Teensy 3.x `__cxa_guard` Conflicts
- **Problem**: Function-local statics with non-trivial constructors generate implicit `__cxa_guard_*` function calls. If Teensy's `<new.h>` is included after the compiler sees the static, the signatures conflict.
- **Preferred Solution (`.cpp.hpp` files)**: Include `<new.h>` early on Teensy 3.x to declare the guard functions before use:
  ```cpp
  // Teensy 3.x compatibility: Include new.h before function-local statics
  #if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
      #include <new.h>
  #endif
  ```
  - Example: `src/fl/async.cpp.hpp` (includes `<new.h>` early to prevent conflicts)
  - This ensures the compiler uses the correct `__guard*` signature
- **Alternative Solution (header files)**: Move static initialization to corresponding `.cpp` file
  - Example: See `src/platforms/shared/spi_hw_1.{h,cpp}` for the correct pattern
- **Exception**: Statics inside template functions are allowed (each template instantiation gets its own static, avoiding conflicts)
- **Linter**: Enforced by `ci/lint_cpp/test_no_static_in_headers.py` for critical directories (`src/platforms/shared/`, `src/fl/`, `src/fx/`)
- **Suppression**: Add `// okay static in header` comment if absolutely necessary (use sparingly)

## Naming Conventions
- **Member variable naming**: All member variables in classes and structs MUST use mCamelCase (prefix with 'm'):
  - Correct: `int mCount;`, `fl::string mName;`, `bool mIsEnabled;`
  - Wrong: `int count;`, `fl::string name;`, `bool isEnabled;`
  - The 'm' prefix clearly distinguishes member variables from local variables and parameters
- **Follow existing code patterns** and naming conventions
