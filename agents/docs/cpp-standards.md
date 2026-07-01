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

### No-op Fallback (`_noop.hpp`)

The dispatcher's `#else` branch needs a target — that's the **no-op implementation**. Platforms without hardware support for a feature get an implementation that satisfies the same API signature with empty / zero-returning bodies, so user code compiles unconditionally on any platform.

**Naming and location:**
- Lives in `src/platforms/shared/` with the suffix `_noop.hpp` — e.g., `pin_noop.hpp`, `memory_noop.hpp`, `simd_noop.hpp`, `codec/h264_noop.hpp`.
- Never call it `_null.hpp`, `_stub.hpp`, or `_dummy.hpp`. The keyword is **`noop`** — this is what to grep for to find the convention.

**File contents:**
- Mark `// IWYU pragma: private` (header is only included via the dispatcher, never directly).
- Functions are `inline` to stay ODR-safe if the header is reached from multiple TUs.
- Live in the `fl::platforms::` namespace (not `fl::` — keeps the public surface clean).
- Body convention: return a safe API-defined inert default — commonly `0`, `false`, `nullptr`, or empty span, but **documented sentinel values are also valid when the API contract requires them** (e.g., `pin_noop.hpp::needsPwmIsrFallback` returns `true` because the no-op platform genuinely needs the ISR fallback path; `setPwmFrequencyNative` returns `-4` as the documented "not supported" error code). **Never assert, throw, or call `FL_WARN`.** A no-op is a no-op — the whole point is that user code keeps running on unsupported platforms.
- For multi-tier APIs that ship `FL_<COMPONENT>_HAS_<FEATURE>` capability macros (see Type 3 below), the no-op header **must not define any of the boolean capability flags** — so `#ifdef FL_<COMPONENT>_HAS_<FEATURE>` evaluates to false on the unsupported platform. It **must** still define any numeric properties (e.g., `#define FL_WATCHDOG_PERSIST_BYTES 0`) so user code that reads them compiles.

**Stub-only variant (`_stub_noop.h`):**
When the no-op only makes sense for the host/stub build (because it pretends to be a real OS primitive that no real MCU exposes), put it in `src/platforms/stub/` with the `_stub_noop.h` suffix instead — e.g., `mutex_stub_noop.h`, `thread_stub_noop.h`, `semaphore_stub_noop.h`. Rule of thumb:
- **`shared/<x>_noop.hpp`** — fallback that *any* unsupported platform may include via the dispatcher.
- **`stub/<x>_stub_noop.h`** — only the host/stub build consumes it.

**Current exemplars:** `src/platforms/shared/memory_noop.hpp`, `src/platforms/shared/pin_noop.hpp`, `src/platforms/shared/simd_noop.hpp`, `src/platforms/shared/codec/h264_noop.hpp`.

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

### Type 3: Component Capability Flags (per-subsystem, defined/undefined for booleans, numeric for properties)

For a subsystem (e.g., `Watchdog`, `Audio`, `Codec`) that has multi-tier platform support, document each platform's capabilities with `FL_<COMPONENT>_HAS_<FEATURE>` flags. These let user code compile-time gate optional features without doing platform detection itself.

**Spelled-out names — no acronyms in macro identifiers.** The macro name and the public API name share a single vocabulary. If the API is `FastLED.watchdog()` then the macros are `FL_WATCHDOG_*`, not `FL_WDT_*`. If the API is `FastLED.audio()` then the macros are `FL_AUDIO_*`. Acronyms in macro names cost grepability and force readers to learn a second name for the same thing.
- ✅ Correct: `FL_WATCHDOG_HAS_WINDOW_MODE`, `FL_WATCHDOG_PERSIST_BYTES`, `FL_AUDIO_HAS_I2S`, `FL_CODEC_HAS_H264`
- ❌ Wrong: `FL_WDT_HAS_WINDOW_MODE` (abbreviation hides the component), `FASTLED_WATCHDOG_WINDOW_MODE` (use `FL_` for newer per-component flags), `WATCHDOG_HAS_WINDOW_MODE` (missing prefix)

**Boolean capability flags:**
- Defined or undefined (same shape as Type 1) — NO values
- Define as: `#define FL_WATCHDOG_HAS_WINDOW_MODE` (no value)
- Check as: `#if defined(FL_WATCHDOG_HAS_WINDOW_MODE)` or `#ifdef FL_WATCHDOG_HAS_WINDOW_MODE`

**Numeric properties (sizes, limits, capacities):**
- Explicit numeric values (same shape as Type 2)
- Define as: `#define FL_WATCHDOG_PERSIST_BYTES 16`
- Check as: `#if FL_WATCHDOG_PERSIST_BYTES >= 8`
- User code may `static_assert(FL_WATCHDOG_PERSIST_BYTES >= 8, "...")` to enforce minimums the unified API promises.

**Where they live:** Defined in the per-platform `*.impl.hpp` (or the component's public header) that implements the feature. The `_noop.hpp` fallback defines **none** of the boolean flags (so `#ifdef` checks correctly evaluate false on unsupported platforms) and **always** defines a zero/default for every numeric property the unified API exposes (so `#if`-on-numeric-value still compiles).

**Distinction from Type 1 (`FL_IS_*`):** Type 1 says "I am running on platform X." Type 3 says "this component has feature Y on this platform." A single platform may carry many Type 3 flags from independent components.

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
- **Linter**: Enforced by `StaticInHeaderChecker` in the Rust crate (`ci/lint_cpp_rs/src/checkers/style.rs`) for critical directories (`src/platforms/shared/`, `src/fl/`, `src/fx/`)
- **Suppression**: Add `// okay static in header` comment if absolutely necessary (use sparingly)

## Peripheral Mocks: Singleton + `MockWrapper` Forwarder (REQUIRED)
- **Rule:** Every peripheral mock in FastLED (`Rmt4PeripheralMock`, `Rmt5PeripheralMock`, `UartPeripheralMock`, `I2sLcdCamPeripheralMock`, `I2sPeripheralEsp32DevMock`, `SpiPeripheralMock`, `FlexIOPeripheralMock`, etc.) MUST expose `static XxxMock& instance() FL_NO_EXCEPT` backed by `fl::Singleton<XxxMockImpl>`. Per-test factories (`createShared()`, `create()`, `make_shared<Mock>()`) are **not** acceptable — they fragment state across the process, break the mental model shared with every peer mock, and turn `reset()` between tests from mandatory hygiene into an unreachable no-op.
- **How to wire a singleton mock into an engine that takes `fl::shared_ptr<IPeripheral>`:** define a thin `MockWrapper : public IPeripheral` class inside the test TU that forwards every interface method to `XxxMock::instance().foo(...)`, then pass `fl::make_shared<MockWrapper>()` to the engine ctor. See `tests/platforms/esp/32/drivers/i2s/channel_driver_i2s.cpp:43-87` for the canonical `createMockPeripheral()` helper — every new engine test copies this pattern.
- **Why:** The peripheral is a real hardware resource — there is exactly one on the chip. The mock mirrors that constraint. Tests that need different mock configurations use error-injection knobs on the singleton (`setTransmitFailure(true)` etc.) and `reset()` between cases, not a fresh mock per test.
- **Exception:** none. If your engine's ctor doesn't take a `shared_ptr<IPeripheral>`, put the mock in a member field of the test fixture and call `instance()` directly — the singleton pattern still holds.
- **History:** Established during #3471 (I2S esp32dev bring-up) after an earlier draft used `createShared()`. Every peer mock in the tree pre-dates that draft and uses the singleton — the wrapper pattern was documented in code but not in the standards until this point.

## Parallel-IO Driver: Unified Clockless + SPI Engine (REQUIRED)
- **Rule:** Any parallel-IO peripheral driver (FlexIO, ObjectFLED, ESP32 PARLIO, LCD_CAM, I2S, etc.) MUST put its SPI mode and its clockless mode in the SAME `ChannelEngine` — do NOT fork into separate `ChannelEngineXxx` + `ChannelEngineXxxSPI` classes, separate `Bus::X` + `Bus::X_SPI` enum values, or separate `BusTraits<>` specializations.
- **How:** The unified engine's `getCapabilities()` returns `Capabilities(true, true)`. Both `BusSupports<Bus::X, ClocklessChipset>` and `BusSupports<Bus::X, SpiChipsetConfig>` specialize to `fl::true_type`. `canHandle()` accepts both `data->isClockless()` and `data->isSpi()` channels (gated by pin / timing routability for each mode). `show()` routes per-channel — if the prior channel ran clockless and the current channel is SPI, reconfigure the peripheral (shifter / timer / DMA TCD) before transmitting.
- **Why:** The peripheral itself can only run in one mode at a time, so the mode switch always happens AT show-time anyway. Forking duplicates Bus enum entries, priority-table rows, registration calls, `_build.cpp.hpp` includes, and `BusTraits<>` specializations for zero behavioral benefit. The peripheral is the dispatch boundary, not the mode.
- **Exception:** genuinely separate peripherals (e.g. ESP32 LPSPI vs I2S_SPI are different silicon blocks with completely different register surfaces) get separate drivers. The "parallel-IO" qualifier excludes the different-peripheral-same-protocol case.
- **History:** Established 2026-06-27 during #3428 FlexIO-SPI/ObjectFLED-SPI implementation. Initial design forked into separate FLEX_IO_SPI / OBJECT_FLED_SPI bus slots; user reverted because forking made the maintenance surface 4× larger with no benefit. See `src/fl/channels/README.md` → "Rule: Parallel-IO peripherals — one engine for both clockless and SPI modes" for the full pattern + reference code.

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
5. ⚠️ **Strict for new code; transitional allowlist for legacy names only.** Every *new* public global setter under `fl::` must ship with a `CFastLED` wrapper. A small transitional allowlist (`PUBLIC_SETTINGS_GRANDFATHERED` in `ci/lint_cpp_rs/src/checkers/public_settings.rs`) exempts pre-existing bare setters (e.g. `fl::set_input_gamut` #2710, `fl::enable_rgbw_colorimetric_lut`, `fl::set_rgbww_colorimetric_profile`) until their wrappers land. Entries are removed as each name is wrapped — the goal is an empty allowlist. New additions do NOT get grandfathered.

**Check Process**:
1. For every new public function in `src/fl/**/*.h` whose name matches `^set_|^enable_|^disable_|^use_` and that mutates a static / global / namespace-scope variable, grep `src/FastLED.h` for a `CFastLED` method that delegates to it.
2. If none exists: violation — add the wrapper in the same PR.
3. Enforced by `PublicSettingsPatternChecker` in `ci/lint_cpp_rs/src/checkers/public_settings.rs`. The checker carries a shrinking `PUBLIC_SETTINGS_GRANDFATHERED` allowlist for legacy bare setters; remove a name from the list once its `CFastLED` wrapper is merged.

**Where the rule does NOT apply**:
- Helpers, constructors, factory functions — these are not setters of global state.
- Functions in `fl::` that operate on caller-owned objects without touching global state (`fl::fill_solid(span, color)`).
- Internal `fl::detail::` / anonymous-namespace functions — not public API.

## Singleton-Stored Configuration: Own by Value, Never by Borrowed Pointer

**Core Principle**: Any public `fl::set_*` API that stores a configuration profile / settings object in process-wide (or static / namespace-scope) state MUST **store by value** (copy / move) and never retain a caller-owned pointer. The parameter SHOULD be `const T&` (or `T&&`); a `const T*` parameter is permitted **only** as a signal for nullable-reset (`nullptr` means "revert to default") and only if the implementation copies the pointed-to value into a value-typed slot on non-null and clears that slot on null. Storing the raw pointer (`sActive = profile`) is always a deterministic use-after-scope bug when callers pass a stack temporary like `auto p = make_profile(); fl::set_x(&p); /* p goes out of scope */`.

**Rules**:
1. ❌ **NEVER store a borrowed pointer to a caller-owned value in a singleton.**
   - ❌ Bad:
     ```cpp
     namespace { const RgbcctProfile* sActive = nullptr; }
     void set_rgbww_colorimetric_profile(const RgbcctProfile* p) FL_NO_EXCEPT {
         sActive = p;  // caller's lifetime, BOOM if they pass a stack temporary
     }
     ```
   - ✅ Good (value semantics, preferred):
     ```cpp
     namespace { RgbcctProfile sActive = kRgbwwDefaultProfile; }
     void set_rgbww_colorimetric_profile(const RgbcctProfile& p) FL_NO_EXCEPT {
         sActive = p;  // copy, lifetime owned by library
     }
     ```
   - ✅ Good (nullable-reset exception — `const T*` allowed when null means "reset"):
     ```cpp
     namespace { RgbcctProfile sActive = kRgbwwDefaultProfile; }
     void set_rgbww_colorimetric_profile(const RgbcctProfile* p) FL_NO_EXCEPT {
         sActive = (p != nullptr) ? *p : kRgbwwDefaultProfile;  // copy on non-null, reset on null
     }
     ```
2. ✅ **Profile getters return `const T&` to the owned copy**, not a pointer that could be `nullptr`.
3. ✅ **For polymorphic configuration** (rare), use `fl::shared_ptr<T>` or `fl::unique_ptr<T>` — never a raw `const T*`.

**Rationale**: The colorimetric / power-model / driver-config setters are the only place this rule fires in the current codebase, but every one of them got a use-after-scope CodeRabbit finding (#2554, #2560, #2588, #2683, #2682) before the rule was written. The Public Settings Pattern (above) tells you *where* the setter lives (god instance); this rule tells you *how* it stores the value. The nullable-reset exception above accommodates the established `set_rgbww_colorimetric_profile(const T*)` shape — pointer parameter, value-typed storage — which is correct and idiomatic for "pass nullptr to revert."

**Check Process**:
1. For each `fl::set_*` setter that stores into a static / namespace-scope variable, verify the **storage** is value-typed (`T`, not `const T*`).
2. If the parameter is `const T*` AND the storage is `const T*`, flag as HIGH severity (pointer storage = use-after-scope).
3. If the parameter is `const T*` but the storage is `T` and the body copies on non-null + resets on null, that's the allowed nullable-reset shape — no violation.

## In-Place Profile Mutation Bumps a Version (Cache Invalidation Contract)

**Core Principle**: If `set_X(T* obj, ...)` writes into the fields of a caller-owned `*obj`, and any subsystem caches values *derived from* `*obj` keyed only on `(obj_ptr, ...)`, then the setter MUST bump `obj->mCacheVersion` (or call `obj->invalidate()`) and the cache key MUST include that version. Otherwise the cache returns stale data the next time the same pointer is passed in.

**Rules**:
1. ❌ **NEVER mutate a profile field in place without bumping a version counter** when a cache exists keyed on the profile pointer.
   - ❌ Bad:
     ```cpp
     void set_input_gamut(DiodeProfile* p, InputGamut g) FL_NO_EXCEPT {
         apply_gamut(p, g);  // rewrites p->input_xy_*
         // …no version bump; ProfileCache keyed on (p, cct) still returns stale M_src
     }
     ```
   - ✅ Good:
     ```cpp
     void set_input_gamut(DiodeProfile* p, InputGamut g) FL_NO_EXCEPT {
         apply_gamut(p, g);
         ++p->mCacheVersion;  // forces any derived-value cache to rebuild
     }
     // and:
     struct CacheKey { const DiodeProfile* p; u32 version; int cct; };
     ```
2. ✅ **Document the cache-invalidation contract** in the setter's doxygen: "Mutates `*p` in place; bumps `p->mCacheVersion` so derived caches rebuild on next access."
3. ✅ **Prefer immutable profiles when possible.** A `DiodeProfile` that ships as `const` to all consumers and is replaced wholesale (`set_profile(const T&)`) eliminates this entire class of bug — see Singleton-Stored Configuration rule above.

**Rationale**: Drove four CodeRabbit findings in the survey window (#2554, #2589, #2707, #2711) and is the root cause of the colorimetric "looks right in tests but stale in production" class. Tests typically construct one profile per test case so the cache never hits — the bug only shows up in real applications that mutate one shared profile across frames.

**Check Process**:
1. For each `fl::set_*(T* obj, ...)` where the body mutates `*obj`, search the codebase for a cache keyed on `obj` (typical names: `ProfileCache`, `kCache`, `sCachedDerived`).
2. If a cache exists and the setter doesn't bump a version field, flag as HIGH severity.

## Contract Change Fanout (API Surface Changes Touch All Layers in One PR)

**Core Principle**: When a PR changes a numeric bound, enum range, struct shape, or semantic of a public API surface, it MUST update **every** consumer of that contract in the same PR: (a) the producer / setter, (b) the validator, (c) unit tests, (d) docs (`agents/docs/*`, AutoResearch JSON-RPC reference, RPC handlers), and (e) any matching debug-metrics / stats struct. Partial sweeps cause silent producer/consumer skew.

**Rules**:
1. ❌ **NEVER widen a bound without sweeping the validator and tests.**
   - ❌ Bad: `examples/AutoResearch/AutoResearchRemote.cpp` bumps `laneSizes` max from 8 to 16, but `src/fl/channels/validation.cpp.hpp` still rejects > 8 — silent rejection at runtime.
   - ✅ Good: Same PR updates the example, the validator, the test fixture asserting the new max, and the JSON-RPC schema doc.
2. ❌ **NEVER add a struct field without updating the debug-metrics dump and the parser.**
3. ❌ **NEVER change `available()` / `empty()` / `size()` semantics without updating the docstring AND every caller that compares against constants.**

**Rationale**: This is a broadening of the existing "API Unit Change" rule (which was scoped to `_ms` / `_us` / `_bytes` suffixes). The recurring CodeRabbit pattern (#2621, #2648, #2669, #2682) is more general — any **contract** change, not just unit changes, needs the same fanout.

**Check Process** (PR-level):
1. If the diff modifies a public setter, enum, struct field, or named constant that defines a contract — grep for the contract name across `src/`, `examples/`, `tests/`, and `agents/docs/`.
2. If any consumer doesn't appear in the diff, either update it in the same PR or document why it doesn't need updating (with a comment in the PR description).

## `fl::span` for Callback Typedefs and Small Fixed-Size Arrays

**Core Principle**: The general `fl::span` rule (above) covers function parameters but is repeatedly missed for two specific shapes: (a) `fl::function<...>` callback / handler typedefs, and (b) small fixed-size array parameters like `const float xy[2]`. These slip through because they don't look like the canonical `void f(const u8*, size_t)` pattern.

**Rules**:
1. ❌ **NEVER declare a callback typedef with a `(ptr, size)` shape.**
   - ❌ Bad: `using write_bytes_handler_t = fl::function<size_t(const u8*, size_t)>;`
   - ✅ Good: `using write_bytes_handler_t = fl::function<size_t(fl::span<const u8>)>;`
2. ❌ **NEVER take small fixed-size arrays as raw pointers.**
   - ❌ Bad: `void set_input_gamut(DiodeProfile* p, InputGamut g, const float white_xy[2]);`
   - ✅ Good: `void set_input_gamut(DiodeProfile* p, InputGamut g, fl::span<const float, 2> white_xy);`
   - ✅ Good (alternative — by-value pair): `void set_white_point(DiodeProfile* p, fl::vec2f white_xy);`
3. ❌ **NEVER use function parameter array spelling (`T value[N]` or `T value[]`).**
   - C++ adjusts these to `T* value`; the extent is not part of the function type and is not enforced.
   - ❌ Bad: `void cct_to_xy(int cct, float out[2]);`
   - ❌ Bad: `void encode(fl::u8 output[4]);`
   - ✅ Good for normal APIs: `void cct_to_xy(int cct, fl::span<float, 2> out);`
   - ✅ Good for ISR / `FL_IRAM` APIs: `void encode(fl::u8 (&output)[4]);`
   - ✅ Good where a wrapper type would obscure the domain: `void set_white_point(DiodeProfile* p, fl::vec2f white_xy);`
   - Suppress only intentional C ABI or platform compatibility cases with `// ok array parameter`.
4. ⚠️ **For ISR / `FL_IRAM` callgraphs, prefer array references over `fl::span<T, N>`.**
   - `fl::span<T, N>` is the right general API shape, but current span accessors are not explicitly `FASTLED_FORCE_INLINE` / `FL_IRAM`. Until an explicitly ISR-safe span exists, ISR-facing fixed-size output buffers should use `T (&value)[N]`.
5. ✅ **For RGBW / RGBWW multi-channel output**, prefer a `fl::span<u8, 4>` (or `5`) over five separate `u8*` parameters, except inside ISR / `FL_IRAM` callgraphs where `u8 (&out)[4]` / `[5]` is preferred.

**Scoped exception — deferred legacy migrations**: A `(ptr, size)` callback typedef MAY be temporarily retained in a legacy layer when migrating it would force a synchronized rewrite of every caller and the migration is being deferred to a follow-up PR. The exception requires (a) a comment at the typedef site referencing the migration plan / tracking issue, (b) naming the legacy location (current example: `src/fl/stl/cstdio.h` retains raw pointer+length handler signatures intentionally), and (c) the exception is timeboxed — it must be revisited at the next layer-wide migration window. Permanent divergence is not permitted; the goal is "all-or-nothing per-layer sweep when the engineering budget allows."

**Rationale**: The base span rule prevents most of the pattern but CodeRabbit caught these two shapes in #2560, #2683, #2711. Issue #3233 adds clang-query lint coverage for source-spelled array parameters because `T value[N]` is especially misleading: it looks fixed-size but compiles as a pointer. The scoped exception accommodates the real-world case where touching every caller in one PR isn't tractable; without it the rule pushes engineers toward unprincipled mixed APIs.

## ISR-Shared State: `fl::atomic` or Critical Section, Never Bare `volatile`

**Core Principle**: Any field that is **written from an ISR callback context and read from task context** (or vice versa), or **written from two task contexts** without other locking, MUST be either an `fl::atomic<T>` with explicit `memory_order`, or guarded by `portENTER_CRITICAL_ISR/portEXIT_CRITICAL_ISR` (ISR side) and `portENTER_CRITICAL/portEXIT_CRITICAL` (task side). Plain `volatile` only prevents the compiler from caching the load — it does NOT provide atomicity, ordering, or visibility across cores.

**Rules**:
1. ❌ **NEVER use `int` / `bool` / `volatile int` for state shared with an ISR.**
   - ❌ Bad:
     ```cpp
     volatile int mPendingTransmits = 0;
     // ISR: --mPendingTransmits;          (not atomic on dual-core, no ordering)
     // task: while (mPendingTransmits) {} (no acquire fence)
     ```
   - ✅ Good (atomic):
     ```cpp
     fl::atomic<int> mPendingTransmits{0};
     // ISR: mPendingTransmits.fetch_sub(1, fl::memory_order_release);
     // task: while (mPendingTransmits.load(fl::memory_order_acquire) > 0) {}
     ```
   - ✅ Good (critical section, when the state is more complex than a single word):
     ```cpp
     // ISR side:
     portENTER_CRITICAL_ISR(&mLock);
     mTransmitting = false;
     mNextDescriptor = next;
     portEXIT_CRITICAL_ISR(&mLock);
     // task side:
     portENTER_CRITICAL(&mLock);
     bool tx = mTransmitting;
     portEXIT_CRITICAL(&mLock);
     ```
2. ❌ **NEVER use `std::atomic<T>`** — FastLED targets AVR / older embedded toolchains that don't have `<atomic>`. Use `fl::atomic<T>` from `fl/stl/atomic.h`. (See the project-wide rule: prefer `fl::` over `std::` except for low-level metaprogramming traits.)
3. ❌ **NEVER clear an ISR-shared flag in task context before all in-flight ISR work has drained.**
   - This is the underrun-race pattern — clearing `mTransmitting = false` in `refill()` while a chunk-done ISR is still pending corrupts the next frame's state.

**Rationale**: ESP32 / RP2040 / dual-core MCUs make this a hardware-correctness issue, not just a style preference. Caught only twice in the survey window (#2682, #2703), but both were high-severity races that would have shipped without manual review.

**Check Process**:
1. For each field declared in a class that has an `IRAM_ATTR` callback or `static void IRAM_ATTR on_*` ISR, verify it's either `fl::atomic<T>` or guarded by a `portMUX_TYPE` critical section.
2. Plain `volatile` on these fields is a violation.

## Unsigned Narrowing Cast Must Clamp

**Core Principle**: When summing channel values, brightness contributions, or any per-element accumulator whose total may exceed the destination type's max, you MUST clamp **before** the narrowing cast. `static_cast<u8>(x)` silently wraps at 256, which is defined behavior in C++ but almost always the wrong behavior.

**Rules**:
1. ❌ **NEVER `static_cast<u8>` a value that can exceed 255.**
   - ❌ Bad:
     ```cpp
     u32 total_mW = red_mW + (white_mW + warm_white_mW) / 3;
     u8 byte = static_cast<u8>(total_mW);  // wraps if total > 255
     ```
   - ✅ Good:
     ```cpp
     u32 total_mW = red_mW + (white_mW + warm_white_mW) / 3;
     u8 byte = static_cast<u8>(fl::min<u32>(255u, total_mW));
     ```
   - ✅ Good (saturating addition helper):
     ```cpp
     u8 byte = fl::qadd8(red_byte, white_byte);  // saturates at 255
     ```
2. ✅ **The same rule applies to `u16` narrowing** — `static_cast<u16>(x)` where `x` is `u32` and the source can exceed 65535 must clamp first.
3. ✅ **Pure narrowing for known-in-range values is fine** — e.g. `u8 byte = static_cast<u8>(rgb.r);` when `rgb.r` is already a `u8` typed via a different value path. The rule fires when there's been arithmetic that can overflow the destination type.

**Rationale**: Sibling to the existing "Signed Integer Overflow (UB)" rule. Unsigned wrap is *defined* but produces silently-wrong color / power / brightness values. The signed-UB rule (above) does NOT catch this — `u8 + u16 → wrap` is fine according to the standard but is the bug pattern flagged twice in #2560 (first review ignored, second review re-flagged).

## Test Mirrors Must Call Production Symbols, Not Redeclare Them

**Core Principle**: When `tests/fl/foo/test_bar.cpp` mirrors part of `src/fl/foo/bar.cpp.hpp` (e.g. to verify mathematical identities like Hermite-basis sum = 1), the test MUST call the production symbol directly. Re-declaring the algorithm as a local lambda or helper inside the test file means the test passes regardless of bugs introduced into the production implementation.

**Rules**:
1. ❌ **NEVER redefine a production algorithm inside a test file just to assert properties of it.**
   - ❌ Bad:
     ```cpp
     // tests/fl/gfx/rgbw_colorimetric.cpp
     FL_TEST_CASE("hermite basis sums to 1") {
         auto h0 = [](float t) { return (1 - t)*(1 - t)*(1 + 2*t); };
         auto h1 = [](float t) { return t*t*(3 - 2*t); };
         // …asserts h0(t) + h1(t) == 1. Production hermite_basis is never called.
     }
     ```
   - ✅ Good:
     ```cpp
     #include "fl/gfx/rgbw_colorimetric.h"
     FL_TEST_CASE("hermite basis sums to 1") {
         float out[2];
         fl::gfx::hermite_basis(0.5f, out);
         FL_CHECK_CLOSE(out[0] + out[1], 1.0f, 1e-6f);
     }
     ```
2. ✅ **Mirror files are allowed only when test is verifying byte-for-byte text equivalence** of two sources (e.g. a port from Python reference). In that case the mirror must be obvious from the file name (e.g. `*_mirror_for_test.cpp.hpp`) and a static_assert / runtime check must compare the mirror's output against the production output on a sample input.
3. ✅ **Header-only `inline` algorithms** can be tested by calling the production header directly — there's no link concern.

**Rationale**: Caught in #2683, #2707, #2709. The bug pattern: a future regression in the production implementation (e.g. someone swaps the sign of a Hermite coefficient) passes all tests because the tests verify a local copy of the *correct* algorithm. The production code can silently drift.

## ASCII-Only in Source Files (No Emoji, No Unicode Math Glyphs)

**Core Principle**: Every C++ source file (`.h`, `.cpp`, `.hpp`, `.cpp.hpp`) and every Python source file in this repo MUST contain only 7-bit ASCII. No emoji in comments, no Unicode math symbols in identifiers or print statements, no curly-quote characters from copy/pasted documentation.

**Rules**:
1. ❌ **NEVER use emoji in C++ comments.**
   - ❌ Bad: `// ⚠️ This runs in ISR context — keep it short`
   - ✅ Good: `// WARNING: This runs in ISR context — keep it short`
2. ❌ **NEVER use Unicode math glyphs in source code.**
   - ❌ Bad (Python): `print("FastLED − Reference")`, `ρ = 0.5`, `Δ = abs(a - b)`
   - ✅ Good (Python): `print("FastLED - Reference")`, `rho = 0.5`, `delta = abs(a - b)`
3. ❌ **NEVER paste curly quotes / em-dashes from documentation tools.** Replace `"smart quotes"` with `"straight quotes"`, `—` with `-` or `--`.
4. ✅ **Markdown documentation files (`*.md`) are exempt** — Unicode in human-reading docs is fine. This rule is for code only.

**Rationale**: Source files are read by lots of tools (compilers, linters, AVR / RP2040 / Teensy toolchains with various Unicode story, terminal log dumps, grep, IDE search). Some toolchains choke; others render the glyphs as garbage in compile errors. Pure ASCII keeps the source legible in every context. Flagged in #2648 (C++ comments with emoji) and #2709 (Python with `×`, `−`, `ρ`, `∆`).

**Check Process**:
1. Run `grep -rPn '[^\x00-\x7F]' src/ tests/ ci/` (excluding `*.md`).
2. Any hits are violations.
3. Suppression: none — there is always an ASCII equivalent.
