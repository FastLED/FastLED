# C++ Code Standards

## Namespace and Headers
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Vector type usage**: Use `fl::vector<T>` for dynamic arrays. The implementation is in `src/fl/stl/vector.h`.

## `fl::net` Namespace Convention

Files in `src/fl/net/` follow a two-level namespace pattern:

- **Primary user-facing type** (if a single one exists) → lives directly in `fl::net`
  - Example: `fl::net::OTA`
- **Supporting types** (enums, transport types, options) → live in `fl::net::<module>` sub-namespace
  - Example: `fl::net::ota::Service`
- **Facade / collection modules** (no single primary type) → all types in `fl::net::<module>`
  - Example: `fl::net::http::*`, `fl::net::ble::*`

When moving a type into a sub-namespace, **drop the module prefix** from the name since the namespace already provides context:
- `OTAService` → `fl::net::ota::Service`
- `BleStatusInfo` → `fl::net::ble::StatusInfo`
- `BleTransportState` → `fl::net::ble::TransportState`

| File | Primary type (`fl::net`) | Sub-namespace (`fl::net::<mod>`) |
|------|--------------------------|----------------------------------|
| `ota.h` | `OTA` | `ota::Service` |
| `rpc.h` | *(none — transport aliases)* | `rpc::StreamClient`, `rpc::StreamServer` |
| `http.h` | *(none — facade)* | `http::Server`, `http::Response`, `http::fetch_get`, … |
| `ble.h` | *(none — collection)* | `ble::TransportState`, `ble::StatusInfo`, `ble::createTransport`, … |

## API Object Pattern

An **API object** is a public-facing wrapper header that lives alongside a directory of the same name containing implementation details. Users only ever `#include` the API header.

**File layout:**
```
src/fl/stl/fixed_point.h        ← API object (public interface)
src/fl/stl/fixed_point/          ← implementation directory
    s16x16.h                     ← concrete type
    base.h                       ← shared internals
    ...
```

**Rules:**
1. **One header, one directory, same name.** `foo.h` is the public API; `foo/` holds everything behind it.
2. **The API object wraps, it doesn't implement.** It inherits from or delegates to concrete types in the directory. It adds uniformity (common interface, operator forwarding) and convenience (type promotion, free functions) but contains no core logic itself.
3. **Concrete types are self-contained.** Each file in the directory is independently functional. The API object composes them — it doesn't modify them.
4. **A trait maps parameters to concrete types.** A dispatch mechanism (e.g. `fixed_point_impl<IntBits, FracBits, Sign>`) selects the right concrete type at compile time. Invalid combinations fail via an undefined primary template.
5. **The API object re-exposes everything through a uniform interface.** Operators, math functions, conversions — all forwarded. The wrapper adds no new logic, just type-safe forwarding.
6. **Free functions live in the API header.** ADL-enabled free functions (e.g. `fl::sin()`, `fl::floor()`) are SFINAE-gated to the wrapper type, giving users a natural calling convention.
7. **Cross-type interactions live in the API header.** Operations spanning multiple concrete types (e.g. `s0x32 * s16x16 → s16x16`) are defined at the bottom, after all types are visible.

**Exemplar:** `src/fl/stl/fixed_point.h` wrapping `src/fl/stl/fixed_point/`

## Platform Dispatch Headers
- FastLED uses dispatch headers in `src/platforms/` (e.g., `int.h`, `io_arduino.h`) that route to platform-specific implementations via coarse-to-fine detection. See `src/platforms/README.md` for details.
- **Platform-specific headers (`src/platforms/**`)**: Header files typically do NOT need platform guards (e.g., `#ifdef ESP32`). Only the `.cpp` implementation files require guards. When the `.cpp` file is guarded from compilation, the header won't be included. This approach provides better IDE code assistance and IntelliSense support.
  - Correct pattern:
    - `header.h`: No platform guards (clean interface)
    - `header.cpp`: Has platform guards (e.g., `#ifdef ESP32 ... #endif`)
  - Avoid: Adding `#ifdef ESP32` to both header and implementation files (degrades IDE experience)

## Sparse Platform Dispatch Pattern (`.cpp.hpp` files)

A `.cpp.hpp` file is a **compile-time component router** that assembles a complete feature from sparse, per-platform fragments with null/no-op fallbacks.

**Key properties:**
1. **Sparse** — No platform implements everything. Each platform contributes only the pieces it supports (e.g., ESP32 provides FreeRTOS tasks but not cooperative coroutines; Teensy provides cooperative context switching but not OS tasks).
2. **Fallback** — Missing pieces get a null/no-op implementation automatically. If a platform doesn't provide a component, the system still works — those features are simply inert.
3. **Component routing** — The `.cpp.hpp` file examines platform defines and `#include`s the right fragments. It composes a complete system from whatever pieces the current platform offers.

**Why `.cpp.hpp` instead of `.cpp` or `.h`?**
- Contains **function definitions** (like `.cpp`) — so it can't be a normal `.h` (would cause multiple-definition linker errors).
- Designed to be **`#include`d from exactly one translation unit** (like `.hpp`) — not compiled on its own.
- Marked `// IWYU pragma: private` to enforce single inclusion.
- The extension signals: "I contain implementations, I'm meant to be included, and I must only be included once."

**Naming convention (future standard):**
- **`component.impl.cpp.hpp`** — The router file. Contains the `#if`/`#elif` dispatch logic that selects which platform fragment to include. Included from exactly one translation unit.
- **`component_<platform>.impl.hpp`** — Platform-specific implementation fragments that the router includes (e.g., `coroutine_esp32.impl.hpp`, `coroutine_wasm.impl.hpp`).

The names make roles explicit: `.impl.cpp.hpp` = "implementation router, include me once." `_<platform>.impl.hpp` = "platform fragment, the router includes me."

**Structure of a `.impl.cpp.hpp` router:**
```cpp
// IWYU pragma: private

// Platform detection
#include "platforms/arm/is_arm.h"
#include "platforms/esp/is_esp.h"
#include "platforms/wasm/is_wasm.h"

#if defined(FL_IS_WASM)
#include "platforms/wasm/feature_wasm.impl.hpp"
#elif defined(FASTLED_STUB_IMPL)
#include "platforms/stub/feature_stub.impl.hpp"
#elif defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_esp32.impl.hpp"
#else
// Fallback: null/no-op implementation
#include "platforms/shared/feature_null.impl.hpp"
#endif
```

**Current exemplar:** `src/platforms/coroutine.impl.cpp.hpp`.

## Span Usage
- **Automatic span conversion**: `fl::span<T>` has implicit conversion constructors - you don't need explicit `fl::span<T>(...)` wrapping in function calls. Example:
  - Correct: `verifyPixels8bit(output, leds)` (implicit conversion)
  - Verbose: `verifyPixels8bit(output, fl::span<const CRGB>(leds, 3))` (unnecessary explicit wrapping)
- **Container auto-conversion**: `fl::span` auto-converts from containers with `data()`/`size()`. Pass the container directly:
  - ✅ `AudioSample(data, timestamp)` or `fl::span<const fl::i16> s = myVector;`
  - ❌ `AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp)`
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
- **Use proper warning macros** from `fl/stl/compiler_control.h`
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

## Channel Engine DMA Wait Pattern
- **`onBeginFrame()` / `show()` must wait for `poll() == READY` before starting a new frame** — use a simple `while (poll() != READY)` loop
- **Yield in wait loops with `fl::task::run()`** — never busy-spin without yielding. Use `fl::task::run(250, fl::task::ExecFlags::SYSTEM)` inside wait loops to yield to the OS scheduler (FreeRTOS `vTaskDelay(0)` on ESP32, `std::this_thread::yield()` on host). This prevents watchdog timeouts and starvation of WiFi/BT/system tasks. Include `fl/task/executor.h`.
  - **Do NOT use** bare `fl::yield()`, `vTaskDelay()`, or `taskYIELD()` — `fl::task::run()` is the unified API
  - For tight DMA polling use `ExecFlags::SYSTEM` (OS yield only, minimal overhead)
  - For longer waits use `ExecFlags::ALL` (also pumps coroutines and scheduled tasks)
- **Do NOT branch on DRAINING or other intermediate states** inside the wait loop in `show()` or `onBeginFrame()`. The `poll()` method drives the state machine to READY; callers just wait for it.
- **`onEndFrame()` may wait for READY *or* DRAINING** — after `show()` kicks off DMA, it's fine to return once DMA is running (DRAINING). `onBeginFrame()` will ensure READY before the next frame.
- **Rationale**: Branching on intermediate states in the "wait for previous frame" path splits logic across multiple places and makes the code harder to reason about.
- **Reference**: See `src/fl/channels/README.md` → "DMA Wait Pattern" section

## Naming Conventions
- **Member variable naming**: All member variables in classes and structs MUST use mCamelCase (prefix with 'm'):
  - Correct: `int mCount;`, `fl::string mName;`, `bool mIsEnabled;`
  - Wrong: `int count;`, `fl::string name;`, `bool isEnabled;`
  - The 'm' prefix clearly distinguishes member variables from local variables and parameters
- **Follow existing code patterns** and naming conventions

## Public Settings Pattern (Global Configuration via `CFastLED`)

**Core Principle**: Any new global / library-wide configuration setter MUST be exposed as a public method on the `CFastLED` god instance in `src/FastLED.h`. The implementation may live as a free function in the `fl::` namespace for ADL / testability, but the documented user-facing entry point is `FastLED.setX(...)`, not `fl::set_x(...)`.

**Rationale**: `CFastLED` is the discoverable surface — users see `FastLED.setBrightness()`, `FastLED.setMaxRefreshRate()`, `FastLED.setPowerModel()` in every sketch and expect every other knob to live there too. Free-function-only configuration ratchets the API surface into a second, undocumented place that users won't find and that drifts out of sync with the god instance.

**Exemplar** (`src/FastLED.h:1455`):
```cpp
// Free function in fl:: namespace — does the work, testable in isolation
namespace fl { void set_power_model(const PowerModelRGB& m) noexcept; }

// Public entry point on CFastLED — thin delegator, this is what users call
class CFastLED {
    inline void setPowerModel(const PowerModelRGB& model) {
        set_power_model(model);
    }
};
```

**Rules**:
1. ❌ **NEVER ship a new `fl::set_*` / `fl::enable_*` / `fl::disable_*` / `fl::use_*` free function that mutates library-wide state without a matching `CFastLED::setX()` / `enableX()` wrapper.**
   - ❌ Bad: `fl::set_input_gamut(&profile, fl::InputGamut::Rec709);` as the documented call site
   - ✅ Good: `FastLED.setInputGamut(&profile, fl::InputGamut::Rec709);` (god instance), with a `fl::set_input_gamut(...)` free function backing it
2. ✅ **The wrapper is a `inline` one-liner that delegates** — no logic, no validation, no error handling. The free function holds all behavior.
3. ✅ **Per-object configuration (e.g. one strip's diode profile, one controller's correction) MAY live on the per-object API.** This rule targets *library-wide / process-wide / default-profile* state.
4. ✅ **Documentation, examples, and PR descriptions reference the god-instance form.** The free function is an implementation detail.
5. ⚠️ **Grandfathered offenders** (free-function-only, predate this rule): `fl::set_rgbw_colorimetric_profile`, `fl::set_input_gamut` (#2710). New code MUST wrap; existing offenders should be wrapped opportunistically.

**Check Process**:
1. For every new public function in `src/fl/**/*.h` whose name matches `^set_|^enable_|^disable_|^use_` and that mutates a static / global / namespace-scope variable, grep `src/FastLED.h` for a `CFastLED` method that delegates to it.
2. If none exists: violation — add the wrapper in the same PR.
3. Enforced by `ci/lint_cpp/` (rule TBD) with the grandfathered names allowlisted.

**Where the rule does NOT apply**:
- Helpers, constructors, factory functions — these are not setters of global state.
- Functions in `fl::` that operate on caller-owned objects without touching global state (`fl::fill_solid(span, color)`).
- Internal `fl::detail::` / anonymous-namespace functions — not public API.
