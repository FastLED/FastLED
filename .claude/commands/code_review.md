# Code Review Agent

You are a specialized code review agent for FastLED. Review staged and unstaged changes according to these strict rules.

## Your Task
1. Run `git diff --cached` to see staged changes
2. Run `git diff` to see unstaged changes
3. Review ALL changes against the rules below
4. For each violation, either:
   - Fix it directly (if straightforward)
   - Ask user for confirmation (if removal/significant change needed)
5. Report summary of all findings

## Review Rules by File Type

### src/** changes - STRICT RULES
- ❌ **NO try-catch blocks allowed**
- ❌ **NO try-except blocks allowed**
- Use alternative error handling patterns (return values, error codes, etc.)
- If found: Report violation with exact line number and fix or ask user

### src/** and examples/** C++ changes - SPAN USAGE MANDATES
**Core Principle**: Use `fl::span` instead of raw pointer+size pairs for contiguous data.

**Rules**:
1. ❌ **NEVER pass raw `(pointer, size)` pairs** between FastLED functions when `fl::span` can be used
   - ❌ Bad: `void process(const u8* data, size_t len)`
   - ✅ Good: `void process(fl::span<const u8> data)`
2. ❌ **NEVER use Arduino `String` type** in FastLED code
   - Use `fl::string` instead
   - Exception: Unavoidable when calling external library APIs (e.g., NimBLE `getValue()` returns `String`)
   - In such cases, convert to `fl::string` immediately at the API boundary
3. ✅ **Raw pointer+size is OK for external API boundaries** — no need to wrap in `fl::span` when calling third-party APIs
   - ✅ Fine: `externalApi((const u8*)str.c_str(), str.size());`
   - ❌ Bad: Wrapping in span just to immediately unwrap: `fl::span<const u8> data(...); externalApi(data.data(), data.size());`
4. ✅ **`fl::span` auto-converts from containers** — pass containers directly
   - ✅ Good: `process(myVector);` (implicit `fl::span` conversion)
   - ❌ Bad: `process(fl::span<const u8>(myVector.data(), myVector.size()));`

**Check Process**:
1. Scan for function signatures with `(const u8* data, size_t size)` or similar pointer+length pairs
2. Scan for `Arduino String` usage (`String val`, `String&`, etc.)
3. Scan for raw pointer casts passed directly without span intermediary
4. If found: Report violation and suggest `fl::span` refactoring

### examples/** changes - QUALITY CONTROL
1. **Check for new *.ino files** (newly added files)
2. **Evaluate each new *.ino for "AI slop"**:
   - Indicators of AI slop:
     - Generic, boilerplate comments
     - Overly verbose or redundant code
     - No meaningful functionality
     - Placeholder patterns or incomplete logic
     - Copied/duplicated code from other examples
     - Missing explanation of what it demonstrates
3. **Action**:
   - If AI slop detected: Remove the file and report
   - If critical functionality BUT questionable quality: Ask user "This *.ino file appears to be AI-generated. Do you want to keep it?"
   - If acceptable: Keep it and note it passed review
- ⚠️ **Important**: Creating new *.ino is historically a major problem with AI - scrutinize carefully

### ci/**.py changes - TYPE SAFETY & INTERRUPT HANDLING
1. **KeyboardInterrupt Handling**:
   - ANY try-except catching general exceptions MUST also handle `KeyboardInterrupt`
   - Pattern requirement:
     ```python
     try:
         # code
     except (KeyboardInterrupt, SomeException):
         import _thread
         _thread.interrupt_main()  # MUST be called first
         # cleanup code
         sys.exit(1)
     ```
   - Missing handler: Report violation and fix
2. **NO partial type annotations**:
   - ❌ Bad: `my_list = []`
   - ✅ Good: `my_list: list[str] = []`
   - ❌ Bad: `my_dict = {}`
   - ✅ Good: `my_dict: dict[str, int] = {}`
   - Apply to: list, dict, set, tuple
   - Required for pyright type checking
   - Missing annotation: Fix directly by adding proper type hints

### **/meson.build changes - BUILD SYSTEM ARCHITECTURE

**FastLED Meson Build Architecture** (3-tier structure):
```
meson.build (root)          → Source discovery, library compilation
├── tests/meson.build       → Test discovery, categorization, executable creation
└── examples/meson.build    → Example compilation target registration
```

**Critical Rules**:
1. **NO embedded Python scripts** - Extract to standalone `.py` files in `ci/` or `tests/`
2. **NO code duplication** - Use loops/functions instead of copy-paste patterns
3. **Configuration as data** - Hardcoded values go in Python config files (e.g., `test_config.py`)
4. **External scripts for complex logic** - Call Python scripts via `run_command()`, don't inline

**Common Violations to Check**:
- ❌ Multiple `run_command(..., '-c', 'import ...')` blocks → Extract to script
- ❌ Repeated if-blocks with same pattern → Use loop
- ❌ Hardcoded paths/lists in build logic → Move to config file
- ❌ String manipulation in Meson → Move to Python helper

**Architecture Summary**:
- **Root meson.build**: Discovers C++ sources from `src/` subdirectories, builds `libfastled.a`
- **tests/meson.build**: Uses `organize_tests.py` to discover/categorize tests, creates executables
- **examples/meson.build**: Registers PlatformIO compilation targets for Arduino examples

**If violations found**: Recommend refactoring similar to `tests/meson.build` (see git history)

### src/platforms/** changes - PLATFORM DISPATCH CONVENTIONS

**Authoritative reference:** `agents/docs/cpp-standards.md` → "Naming convention (future standard)" and "No-op Fallback (`_noop.hpp`)".

**The three file-suffix roles in `src/platforms/`:**

| Suffix | Role | Where it lives | Included by |
|---|---|---|---|
| `<component>.impl.cpp.hpp` | **Dispatcher / router.** Contains `#if defined(FL_IS_*)` selecting which platform fragment to include. | `src/platforms/` | Included from exactly **ONE** `.cpp` translation unit |
| `<component>_<platform>.impl.hpp` | **Platform fragment.** Implementation for a specific MCU family. | `src/platforms/<family>/` | Included **only** by the matching dispatcher |
| `<component>_noop.hpp` | **No-op fallback.** Empty/zero-returning bodies so unsupported platforms compile. | `src/platforms/shared/` | Included by dispatcher's `#else` branch |

**Hard rules — flag any deviation:**

1. **No-op fallbacks use the literal suffix `_noop`** — not `_null`, `_stub`, `_dummy`, `_empty`, or `_default`. The keyword `noop` is the grep marker for this convention.
   - ✅ Good: `pin_noop.hpp`, `memory_noop.hpp`, `simd_noop.hpp`, `watchdog_noop.hpp`
   - ❌ Wrong: `pin_null.hpp`, `memory_stub.hpp`, `simd_dummy.hpp`

2. **No-op bodies do nothing.** Return `0`, `false`, `nullptr`, or empty span. Do not assert, throw, or log. The whole point is unconditional compilation.

3. **No-op functions live in `fl::platforms::` namespace** — not `fl::` directly.

4. **All three suffix types start with `// IWYU pragma: private`.** They are not meant for direct user inclusion.

5. **Dispatcher (`.impl.cpp.hpp`) lives in `src/platforms/` root** — not inside a per-family subdirectory.

6. **Platform fragments use `.impl.hpp`, not `.cpp.hpp`.** The `.cpp.hpp` suffix is reserved for the single dispatcher.

7. **The dispatcher's `#else` branch MUST include a `_noop.hpp` fallback.** A dispatcher with no fallback breaks unsupported-platform builds.

8. **Stub-only no-ops** (host-build pretending to be an OS primitive) live in `src/platforms/stub/` with the `_stub_noop.h` suffix instead. Use `shared/<x>_noop.hpp` when ANY platform might fall back; use `stub/<x>_stub_noop.h` when only the host build consumes it.

**Exemplars to reference:**
- Dispatcher: `src/platforms/coroutine.impl.cpp.hpp`, `src/platforms/channel_drivers.impl.cpp.hpp`
- No-op fallback: `src/platforms/shared/memory_noop.hpp`, `src/platforms/shared/pin_noop.hpp`, `src/platforms/shared/simd_noop.hpp`

### Macro Naming — THREE TYPES, NO ACRONYM-STYLE CAPABILITY FLAGS

**Authoritative reference:** `agents/docs/cpp-standards.md` → "Macro Definition Patterns".

The codebase recognizes three macro types:

| Type | Pattern | Has value? | Use case |
|---|---|---|---|
| 1. Platform detection | `FL_IS_<PLATFORM>` | No (defined/undefined) | "Am I running on this platform?" |
| 2. Configuration | `FASTLED_<NAME>` | Yes (0/1/numeric, explicit) | User-tunable build settings |
| 3. Component capability flag | `FL_<COMPONENT>_HAS_<FEATURE>` or `FL_<COMPONENT>_<NUMERIC>` | Boolean: no value. Numeric: explicit value. | "Does this subsystem have this feature on this platform?" |

**CRITICAL: Type 3 capability macros use SPELLED-OUT component names — no acronyms.**

The macro identifier and the public API name share a single vocabulary. If the API is `FastLED.watchdog()`, the macros are `FL_WATCHDOG_*`. If the API is `FastLED.audio()`, the macros are `FL_AUDIO_*`. Forcing readers to learn an acronym in addition to the spelled-out API name is exactly the jargon failure these conventions exist to prevent.

- ✅ Correct: `FL_WATCHDOG_HAS_WINDOW_MODE`, `FL_WATCHDOG_PERSIST_BYTES`, `FL_AUDIO_HAS_I2S`, `FL_CODEC_HAS_H264`
- ❌ Wrong: `FL_WDT_HAS_WINDOW_MODE` (acronym hides the component), `FASTLED_WATCHDOG_*` (use `FL_` for newer per-component capability flags, reserve `FASTLED_` for Type 2 build settings), `WATCHDOG_HAS_WINDOW_MODE` (missing prefix)

**Check Process for PRs:**
1. Grep for any new `#define FL_*` in the diff.
2. If it contains a known acronym for a longer component name (`WDT`, `IRQ`, `MUX`, `DMA` as a *component name* vs as a feature descriptor), flag it. (Note: `FL_WATCHDOG_HAS_DMA_RESET` is fine — `DMA` is a feature within the watchdog component, not the component itself.)
3. If a no-op header is added and lacks the `_noop` suffix, flag it.
4. If a new `*.cpp.hpp` lives in a per-family subdirectory rather than `src/platforms/` root, flag it as a misuse of the dispatcher suffix.

### **/*.h and **/*.cpp changes - PLATFORM HEADER ISOLATION

**Core Principle**: Platform-specific headers MUST ONLY be included in .cpp files, NEVER in .h files.

**Rationale**:
- Improves IDE code assistance and IntelliSense support
- Prevents platform-specific headers from polluting the interface
- Enables better cross-platform compatibility
- Allows headers to be parsed cleanly without platform-specific dependencies

**Platform-Specific Headers** (non-exhaustive list):
- ESP32: `soc/*.h`, `driver/*.h`, `esp_*.h`, `freertos/*.h`, `rom/*.h`
- STM32: `stm32*.h`, `hal/*.h`, `cmsis/*.h`
- Arduino: `Arduino.h` (exception: may be in headers when needed)
- AVR: `avr/*.h`, `util/*.h`
- Teensy: `core_pins.h`, `kinetis.h`

**Rules**:
1. ❌ **NEVER include platform headers in .h files** (with rare exceptions)
2. ✅ **ALWAYS include platform headers in .cpp files**
3. ✅ Use forward declarations in headers instead
4. ✅ Define platform-specific constants/macros with conservative defaults in headers
5. ✅ Perform runtime validation in .cpp files where platform headers are available

**Pattern Examples**:
```cpp
// ❌ BAD: header.h
#include "soc/soc_caps.h"
#define MAX_WIDTH SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH

// ✅ GOOD: header.h
// Conservative default - runtime validation happens in .cpp
#define MAX_WIDTH 16

// ✅ GOOD: header.cpp
#include "soc/soc_caps.h"
void validateHardware() {
    if (requested_width > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) {
        // handle error
    }
}
```

**Exceptions** (must be justified):
- Arduino.h in examples or Arduino-specific helpers
- Platform detection macros already defined by build system
- Forward declarations with extern "C" guards

**Check Process**:
1. For each .h file, scan for `#include` directives
2. Identify platform-specific headers (soc/, driver/, esp_, freertos/, hal/, stm32, avr/, etc.)
3. If found:
   - **Violation**: Report with exact file and line number
   - **Action**: Move include to corresponding .cpp file
   - **Refactor**: Replace compile-time logic with runtime validation
   - **Simplify**: Use conservative defaults in header

**Action on Violation**:
- Report the platform header include with file and line number
- Suggest moving to .cpp file
- Recommend refactoring approach (forward declarations, runtime validation)
- For new changes: Fix immediately
- For existing code: Create GitHub issue or ask user if they want to fix now

---

### **/*.h and **/*.cpp changes - FILE/CLASS NAME NORMALIZATION

**Core Principle**: Header and source file names MUST match the primary class/type they define.

**Naming Convention Rules**:
1. **One-to-One Mapping**: File name matches primary class name
   - Class `Channel` → File `channel.h` + `channel.cpp`
   - Class `ColorAdjustment` → File `color_adjustment.h` + `color_adjustment.cpp`
   - Class `ActiveStripTracker` → File `active_strip_tracker.h` + `active_strip_tracker.cpp`

2. **Case Convention**:
   - C++ files: `snake_case` (all lowercase with underscores)
   - Classes: `PascalCase` (capitalize each word)
   - Example: `ChannelDriver` → `channel_driver.h`

3. **Name by WHAT it IS, not by role**:
   - ✅ Good: `bulk_strip.h` (describes the entity)
   - ❌ Bad: `sub_controller.h` (describes internal implementation detail)
   - ✅ Good: `peripheral_tags.h` (describes what it contains)
   - ❌ Bad: `type_markers.h` (too generic/vague)

4. **Multi-Class Files** (rare exceptions - must be justified):
   - OK: `peripheral_tags.h` - Contains multiple related tag types and traits
   - OK: `constants.h` - Contains multiple constant definitions
   - OK: `color.h` - Contains color enums, but consider if primary type warrants name
   - NOT OK: `utilities.h` containing `Channel` class (rename to `channel.h`)

**Check Process**:
1. For each new/modified .h file, extract primary class name(s)
2. Convert class name to snake_case
3. Compare with actual filename
4. If mismatch:
   - **Minor mismatch** (e.g., `strip_controller.h` → `bulk_strip.h`): Suggest rename
   - **Major mismatch** (e.g., `sub_controller.h` → `bulk_strip.h`): Flag as violation, ask for fix
   - **Multi-class file**: Verify it's justified (related utilities/tags/constants)

**Examples of Violations**:
```cpp
// ❌ VIOLATION: File "sub_controller.h" contains class Channel
// Fix: Rename to "channel.h" + "channel.cpp"

// ❌ VIOLATION: File "helpers.h" contains single class ChannelHelper
// Fix: Rename to "channel_helper.h"

// ✅ ACCEPTABLE: File "peripheral_tags.h" contains LCD_I80, RMT, I2S, SPI_BULK tags
// Reason: Multiple related types, descriptive collective name
```

**Action on Violation**:
- Report the mismatch with exact file and class names
- Suggest correct file name following snake_case convention
- For new files: Fix immediately by asking user to rename
- For existing files: Create GitHub issue or ask user if they want to rename now

### src/** changes - PUBLIC SETTINGS PATTERN (god-instance enforcement)

**Core Principle**: New global / library-wide configuration setters MUST be exposed on `CFastLED` (`FastLED.setX(...)`), not as bare `fl::set_*` free functions. The free function may exist for ADL/testability, but the documented entry point is the god instance.

**Exemplar** (`src/FastLED.h:1455`):
```cpp
inline void setPowerModel(const PowerModelRGB& model) {
    set_power_model(model);  // delegates to fl::set_power_model
}
```

**Rules**:
1. ❌ **NEVER add a new public `fl::set_*` / `fl::enable_*` / `fl::disable_*` / `fl::use_*` free function that mutates library-wide state without a matching `CFastLED::setX()` wrapper.**
   - ❌ Bad (PR diff adds only):
     ```cpp
     // src/fl/gfx/rgbw.h
     namespace fl { void set_input_gamut(DiodeProfile*, InputGamut) noexcept; }
     ```
   - ✅ Good (PR diff adds BOTH):
     ```cpp
     // src/fl/gfx/rgbw.h
     namespace fl { void set_input_gamut(DiodeProfile*, InputGamut) noexcept; }
     // src/FastLED.h, near setPowerModel
     inline void setInputGamut(fl::DiodeProfile* p, fl::InputGamut g) FL_NOEXCEPT {
         fl::set_input_gamut(p, g);
     }
     ```
2. ✅ **The wrapper is a thin `inline` delegator** — no logic, validation, or error handling embedded.
3. ✅ **Examples, README, and PR descriptions** should call `FastLED.setX(...)`, never `fl::set_x(...)`.

**Grandfathered Exceptions** (predate the rule — do not flag for these names alone):
- `fl::set_rgbw_colorimetric_profile`
- `fl::set_input_gamut` (#2710)

These should be wrapped opportunistically, but their bare existence does not block the PR they appear in.

**Does NOT apply to**:
- Helpers, constructors, factories (not "setters of global state")
- Per-object / per-strip configuration on the object's own API
- Functions that only mutate caller-owned objects (`fl::fill_solid(span, color)`)
- `fl::detail::` / anonymous-namespace internals

**Check Process**:
1. Scan the diff for new public function declarations in `src/fl/**/*.h` whose name matches `^set_|^enable_|^disable_|^use_`.
2. For each match, check whether the function mutates a namespace-scope / static / global variable (look at the implementation in the matching `.cpp.hpp`).
3. If yes and the name isn't grandfathered: grep `src/FastLED.h` for a `CFastLED` method whose body mentions that free function. If none, **flag as HIGH severity** and either:
   - Add the wrapper inline in the same diff (if the change is small), or
   - Ask the user to add it before merge.
4. Reference: `agents/docs/cpp-standards.md` → "Public Settings Pattern".

### src/** changes - SIGNED INTEGER OVERFLOW (UNDEFINED BEHAVIOR)

**Core Principle**: Signed integer arithmetic that can overflow is undefined behavior in C++. Compilers exploit this for optimization, causing silent miscompilation. All potentially-overflowing arithmetic on signed types (`i8`, `i16`, `i32`) must be performed in the corresponding unsigned type, then cast back.

**Rules**:
1. ❌ **NEVER add/subtract signed integers that may overflow**
   - ❌ Bad: `return Derived::from_raw(mValue + b.mValue);`
   - ✅ Good: `return Derived::from_raw(static_cast<raw_type>(static_cast<unsigned_raw_type>(mValue) + static_cast<unsigned_raw_type>(b.mValue)));`
2. ❌ **NEVER negate a signed value without casting to unsigned first**
   - ❌ Bad: `return Derived::from_raw(-mValue);` (negating INT_MIN is UB)
   - ✅ Good: `return Derived::from_raw(static_cast<raw_type>(0u - static_cast<unsigned_raw_type>(mValue)));`
3. ❌ **NEVER left-shift a signed value**
   - ❌ Bad: `static_cast<i32>(val) << 8;` (shifting into sign bit is UB)
   - ✅ Good: `static_cast<i32>(static_cast<u32>(val) << 8);`
4. ❌ **NEVER negate a signed value when assigning to unsigned**
   - ❌ Bad: `u32 v = -value;` (where value is signed)
   - ✅ Good: `u32 v = -static_cast<u32>(value);`

**Check Process**:
1. Scan for arithmetic operators (`+`, `-`, `<<`, unary `-`) on signed integer types
2. Focus especially on: fixed-point types (`s16x16`, `s12x4`, `s24x8`, `s8x24`), SIMD code, charconv
3. Scan for `static_cast<i32>(.*) <<` — signed left-shift pattern
4. Scan for negation of signed values assigned to unsigned types
5. If found: Report violation and fix by casting operands to unsigned before the operation

### **/*.h and **/*.cpp changes - REDUNDANT VIRTUAL ON OVERRIDE METHODS

**Core Principle**: When `override` is present, `virtual` is redundant and must be removed. When a method overrides a base class virtual, `override` must be present.

**Rules**:
1. ❌ **NEVER use `virtual` together with `override`** — `override` already implies virtuality
   - ❌ Bad: `virtual void init() override { }`
   - ✅ Good: `void init() override { }`
2. ❌ **NEVER omit `override` on methods that override a base class virtual**
   - ❌ Bad: `virtual u16 getMaxRefreshRate() const { return 800; }` (overrides base but missing `override`)
   - ✅ Good: `u16 getMaxRefreshRate() const override { return 800; }`

**Check Process**:
1. Scan for regex `virtual\s+.*\boverride\b` — redundant virtual, remove it
2. For new method declarations with `virtual`, check if the class has a base class with the same method — if so, replace `virtual` with `override`
3. If found: Fix directly by removing the redundant `virtual` keyword

### src/** changes - SINGLETON AND THREAD-LOCAL SINGLETON USAGE

**Core Principle**: Use `fl::Singleton<T>` for process-wide singletons and `fl::SingletonThreadLocal<T>` for thread-local heap-allocated scratch buffers. Never use raw `static ThreadLocal<T>` — always use the wrapper.

**When to use `fl::Singleton<T>`**:
- Process-wide shared state (managers, registries, schedulers)
- Objects that must be shared across threads (with internal locking in T)
- Example: `Singleton<LockedRandom>` where `LockedRandom` contains `fl::mutex` + `fl_random`

**When to use `fl::SingletonThreadLocal<T>`**:
- Large scratch buffers that would blow the stack if allocated locally
- Thread-local caches (each thread gets its own instance, no locking needed)
- Any pattern that was previously `static ThreadLocal<T> tl; return tl.access();`
- Examples: FFT scratch buffers, pixel reorder buffers, trace storage, temp allocators

**Rules**:
1. ❌ **NEVER use raw `static ThreadLocal<T>`** — use `SingletonThreadLocal<T>::instance()` instead
   - ❌ Bad: `static fl::ThreadLocal<T> tl; return tl.access();`
   - ✅ Good: `return SingletonThreadLocal<T>::instance();`
2. ❌ **NEVER use the C++ `thread_local` keyword** — use `SingletonThreadLocal<T>::instance()` instead
   - The keyword doesn't work on AVR and single-threaded embedded platforms
   - The linter (`ThreadLocalKeywordChecker`) enforces this
3. ❌ **NEVER put locking in header-only types to avoid circular includes** — put the lock in the singleton's T (defined in the .cpp.hpp)
   - ❌ Bad: Adding `fl::mutex` to `fl_random` in `random.h` (causes circular includes)
   - ✅ Good: `struct LockedRandom { fl::mutex mtx; fl_random rng; };` in `random.cpp.hpp`, then `Singleton<LockedRandom>::instance().rng`
4. ❌ **`fl::Singleton<T>` and `fl::SingletonThreadLocal<T>` MUST be in `.cpp.hpp` files, NEVER in `.h` headers**
   - These use internal linkage (static storage) — putting them in headers creates per-translation-unit copies
   - ❌ Bad: `Singleton<Foo>::instance()` in `foo.h`
   - ✅ Good: `Singleton<Foo>::instance()` in `foo.cpp.hpp`
   - **Exception**: `fl::SingletonShared<T>` — designed for `.h` headers, uses a process-wide registry to share across DLL/translation unit boundaries (for template code, cross-DLL singletons)
5. ✅ **All singleton types are never destroyed** — aligned storage + placement new, no static destruction order issues
6. ✅ **All singleton types use LSAN ScopedDisabler** — no false leak reports

**Check Process**:
1. Scan for `static.*ThreadLocal<` — should be `SingletonThreadLocal<T>::instance()` instead
2. Scan for `\bthread_local\b` keyword — should be `SingletonThreadLocal<T>::instance()` instead
3. Scan for `fl::mutex` or `fl::unique_lock` in `.h` files — locking belongs in `.cpp.hpp` impl structs
4. Scan for `Singleton<` or `SingletonThreadLocal<` in `.h` files — must be in `.cpp.hpp` (except `SingletonShared<`)
5. Scan for large stack allocations (`fl::vector<T> buf;` as local variable in hot paths) — suggest `SingletonThreadLocal`

### src/** changes - ALIGNMENT ATTRIBUTES FOR RAW STORAGE

**Core Principle**: Any raw `char[]` buffer used with placement `new` MUST be properly aligned using `FL_ALIGN_AS_T` or `FL_ALIGNAS` macros. Never use bare `alignas()` directly — the FL_ macros handle platform quirks (GCC 4.x, AVR, ESP8266, WASM).

**Rules**:
1. ❌ **NEVER use unaligned `char[]` for placement new**
   - ❌ Bad: `static char buf[sizeof(T)]; new (buf) T();`
   - ✅ Good: `struct FL_ALIGN_AS_T(alignof(T)) S { char data[sizeof(T)]; }; static S s; new (&s.data) T();`
2. ❌ **NEVER use bare `alignas()` or `__attribute__((aligned(...)))` directly**
   - ❌ Bad: `alignas(T) char buf[sizeof(T)];`
   - ❌ Bad: `__attribute__((aligned(8))) char buf[sizeof(T)];`
   - ✅ Good: Use `FL_ALIGNAS(N)` for numeric alignment or `FL_ALIGN_AS_T(expr)` for template-dependent expressions
3. ✅ **Use the correct alignment macro for the context**:
   - `FL_ALIGNAS(N)` — numeric constant alignment (e.g., `FL_ALIGNAS(8)`)
   - `FL_ALIGN_AS(T)` — align to match type T (non-template contexts)
   - `FL_ALIGN_AS_T(expr)` — template-dependent alignment expressions (e.g., `FL_ALIGN_AS_T(alignof(T))`)
   - `FL_ALIGN_MAX` — maximum platform alignment (for type-erased storage)
4. ✅ **Wrap aligned buffer in a struct** — applying alignment to a struct is more portable than to a bare array
   - ✅ Pattern: `struct FL_ALIGN_AS_T(alignof(T)) AlignedStorage { char data[sizeof(T)]; };`

**Platform Behavior Summary** (defined in `fl/stl/align.h`):
- **AVR**: All alignment macros are no-ops (8-bit, no alignment needed)
- **ESP8266**: Capped at 4-byte alignment (no `memalign()` available)
- **GCC < 5.0**: Uses `__attribute__((aligned(...)))` (alignas bug with template exprs)
- **WASM/Emscripten**: `FL_ALIGN_AS_T` and `FL_ALIGN_AS` are no-ops
- **Modern compilers**: Uses standard `alignas()`

**Check Process**:
1. Scan for `new (` (placement new) — verify the target buffer has proper alignment
2. Scan for bare `alignas(` — should use `FL_ALIGNAS`, `FL_ALIGN_AS`, or `FL_ALIGN_AS_T` instead
3. Scan for `__attribute__((aligned` — should use FL_ macros instead
4. Scan for `char buf[sizeof(` or `char data[sizeof(` without an enclosing aligned struct

### tests/** and **/*_mock.* changes - AVOID THREADING IN MOCKS

**Core Principle**: Mock/test implementations MUST be synchronous and deterministic. Threading introduces flakiness.

**Anti-Patterns to Flag** (❌ VIOLATIONS):
1. **Background threads** in mock implementations
   - ❌ `fl::thread`, `std::thread`
   - ❌ Simulation/worker threads
   - ❌ Background async processing loops
2. **Synchronization primitives** (indicate attempted thread safety)
   - ❌ `fl::mutex`, `std::mutex`
   - ❌ `fl::condition_variable`, `std::condition_variable`
   - ❌ `fl::lock_guard`, `std::lock_guard`
   - ❌ `fl::atomic<bool>`, `std::atomic`
3. **Timing-based behavior** in mocks
   - ❌ `fl::micros()`, `fl::millis()` for completion detection
   - ❌ `fl::sleep()`, `fl::delay()` in mock logic
   - ❌ Time-based state transitions or callbacks
   - ✅ **OK**: Simulated time via explicit time advancement (e.g., `mSimulatedTimeUs += ms`)
4. **Sleep/poll loops** waiting for state changes
   - ❌ `while (condition) fl::sleep(...);`
   - ❌ Spin-loops checking timing
   - ❌ `std::condition_variable::wait_for()`

**Recommended Pattern** (✅ GOOD):
- **Synchronous callbacks**: Fire immediately or defer until explicitly pumped
- **Simulated time**: Track virtual time that advances only via explicit `delay()`/`advance()` calls
- **Re-entrancy guard**: Use boolean flag to handle nested callback scenarios
- **No threads**: Everything runs on calling thread, fully deterministic

**Example Good Pattern**:
```cpp
class MockI2S {
    u64 mSimulatedTimeUs = 0;
    bool mFiringCallbacks = false;
    size_t mDeferredCallbackCount = 0;

    void transmit(const u8* data) {
        // Queue callback (synchronous, no threading)
        mDeferredCallbackCount++;
        pumpDeferredCallbacks();
    }

    void pumpDeferredCallbacks() {
        if (mFiringCallbacks) return;  // Re-entrancy guard
        mFiringCallbacks = true;
        while (mDeferredCallbackCount > 0) {
            mDeferredCallbackCount--;
            fireCallback();  // Fires synchronously
        }
        mFiringCallbacks = false;
    }

    void delay(u32 ms) {
        mSimulatedTimeUs += static_cast<u64>(ms) * 1000;  // Advance virtual time
    }
};
```

**Check Process**:
1. For each file matching `**/tests/**` or `**/*_mock.*`:
   - Search for `fl::thread`, `std::thread`, `unique_ptr<thread>`
   - Search for `fl::mutex`, `fl::condition_variable`, `fl::lock_guard`
   - Search for `fl::atomic<`, `std::atomic<`
   - Search for timing-based logic using `fl::micros()` or `fl::millis()`
   - Search for `fl::sleep()` or `fl::delay()` in non-delay logic
2. If found:
   - **Violation**: Report with exact file, line number, and pattern type
   - **Action**: Recommend refactoring to synchronous + simulated time approach
   - **Reference**: Point to `src/platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp` as working example

**Rationale**:
- **Flakiness**: Timing-based completion detection fails in CI/testing environments
- **Platform variance**: Thread scheduling differs across OS/hardware
- **Race conditions**: Mutexes and atomics introduce subtle synchronization bugs
- **Hard to debug**: Multi-threaded tests are non-deterministic and unreproducible
- **Determinism**: Simulated time guarantees reproducible behavior

### tests/** changes - FL_CHECK vs FL_REQUIRE FOR PRECONDITIONS

**Core Principle**: When code after an assertion depends on the asserted condition being true, use `FL_REQUIRE` (which aborts on failure), not `FL_CHECK` (which logs and continues). Using `FL_CHECK` for preconditions leads to crashes or undefined behavior when the check fails silently.

**Rules**:
1. ❌ **NEVER use `FL_CHECK` when subsequent code depends on the condition**
   - ❌ Bad: `FL_CHECK(ids.size() == 1); uint32_t id = ids[0];` (crash if size != 1)
   - ✅ Good: `FL_REQUIRE(ids.size() == 1); uint32_t id = ids[0];`
2. ❌ **NEVER use `FL_CHECK` before array/pointer access that requires the check**
   - ❌ Bad: `FL_CHECK(ptr != nullptr); ptr->doThing();`
   - ✅ Good: `FL_REQUIRE(ptr != nullptr); ptr->doThing();`
3. ✅ **`FL_CHECK` is fine for non-critical assertions** where execution can safely continue
   - ✅ OK: `FL_CHECK(count > 0);` when subsequent code handles count==0 gracefully

**Check Process**:
1. Scan for `FL_CHECK(...)` in test code
2. Check if subsequent lines (within 3 lines) access/dereference/index the checked variable
3. If found: Replace `FL_CHECK` with `FL_REQUIRE` and report

### tests/** changes - STACK-USE-AFTER-SCOPE WITH LED ARRAYS

**Core Principle**: Tests that register stack-allocated `CRGB` arrays with `FastLED.addLeds()` or `CLEDController` must detach those pointers before the array goes out of scope. Otherwise, subsequent tests may access freed stack memory through stale controller pointers.

**Anti-Pattern** (❌ BAD):
```cpp
FL_TEST_CASE("my test") {
    CRGB leds[NUM_LEDS];
    FastLED.addLeds<WS2812>(leds, NUM_LEDS);
    // ... test code ...
    FastLED.clear(true);
    // BUG: leds[] goes out of scope but controller still holds pointer
}
```

**Correct Pattern** (✅ GOOD):
```cpp
FL_TEST_CASE("my test") {
    CRGB leds[NUM_LEDS];
    FastLED.addLeds<WS2812>(leds, NUM_LEDS);
    // ... test code ...
    // Detach LED pointers before leds[] goes out of scope
    for (auto* p = CLEDController::head(); p; p = p->next()) {
        p->setLeds(nullptr, 0);
    }
    ActiveStripTracker::resetForTesting();
}
```

**Check Process**:
1. In test files, scan for stack-allocated `CRGB` arrays combined with `addLeds`
2. Verify that before the scope closes, the test detaches the LED pointer from the controller
3. If missing: Report violation and add the detach pattern

### src/platforms/** changes - MISSING PLATFORM VERSION GUARDS

**Core Principle**: Platform SDK functions that may not exist in all versions must be wrapped with version guards. Unguarded calls cause compilation failures on older SDK versions.

**Rules**:
1. ❌ **NEVER call ESP-IDF functions without version guards** when the function was added in a specific IDF version
   - ❌ Bad: `usb_serial_jtag_is_connected();` (not available in IDF < 5.3)
   - ✅ Good:
     ```cpp
     #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
         return usb_serial_jtag_is_connected();
     #else
         return true;  // Conservative fallback
     #endif
     ```
2. ✅ **Always provide a conservative fallback** in the `#else` branch

**Check Process**:
1. In `src/platforms/` files, scan for new ESP-IDF API calls (`esp_*`, `usb_*`, `gpio_*`, etc.)
2. Check if the call is inside a version guard (`#if ESP_IDF_VERSION >= ...`)
3. If missing: Report violation and wrap with appropriate version guard

### src/** and tests/** changes - UNNECESSARY SUPPRESSION COMMENTS

**Core Principle**: AI coding agents frequently add suppression comments that aren't needed. Every suppression comment must be audited.

**Known Suppression Comments** (complete list from codebase):
- `// ok bare allocation` — suppresses `BareAllocationChecker` (bare new/delete/malloc/free)
- `// ok sleep for` — suppresses `SleepForChecker` (raw OS sleep)
- `// ok thread_local` — suppresses `ThreadLocalKeywordChecker` (raw thread_local keyword)
- `// ok header path` / `// ok include path` — suppresses `HeaderPathValidator` (include path validation)
- `// ok reinterpret cast` — suppresses `ReinterpretCastChecker`
- `// ok include` / `// ok include cpp` — suppresses various include checkers
- `// ok platform headers` — suppresses `PlatformIncludesChecker` (platform headers in .h files)
- `// ok relative include` — suppresses `RelativeIncludeChecker`
- `// ok static in header` / `// okay static in header` / `// okay static definition in header` / `// okay static in class` — suppresses `StaticInHeaderChecker`
- `// ok no namespace fl` / `// okay fl namespace` — suppresses namespace checkers
- `// okay std namespace` — suppresses `StdNamespaceChecker`
- `// ok span from pointer` — suppresses `SpanFromPointerChecker`
- `// ok bare using` — suppresses bare using-declaration checker
- `// ok no header` — suppresses header pair checker
- `// ok reading register` — suppresses volatile/register access checker
- `// okay banned header` — suppresses `BannedHeadersChecker`
- `// NOLINT` / `// NOLINTBEGIN` / `// NOLINTEND` — suppresses clang-tidy
- `// okay static in header` — suppresses static-in-header for intentional static locals

**Exempt from audit** (assume correct unless obviously wrong):
- `// IWYU pragma:` — Include-What-You-Use pragmas are generally intentional and validated by the IWYU tool itself; do not flag these

**Rules**:
1. ❌ **Flag every NEW suppression comment** — verify the suppressed pattern actually exists on that line
   - ❌ Bad: `int x = 5; // ok bare allocation` (no allocation here — unnecessary suppression)
   - ✅ Good: `T* ptr = new T(); // ok bare allocation` (genuine bare allocation that's intentional)
2. ❌ **Remove suppressions where the violation was fixed** — if the code was refactored to avoid the pattern, the suppression is dead weight
3. ❌ **Flag speculative suppressions** — AI agents often add suppressions "just in case" on code that doesn't trigger any checker

**Check Process**:
1. For each new/modified line containing `// ok `, `// okay `, `// NOLINT`, or `// IWYU pragma:`
2. Verify the suppressed pattern actually exists in the code on that line
3. If the suppression is unnecessary: remove it and report

### src/** changes - API UNIT CHANGES MUST PROPAGATE TO ALL CALL SITES

**Core Principle**: When changing the unit of a function parameter (e.g., milliseconds to microseconds), all call sites must be updated in the same changeset. Partial propagation causes silent bugs where callers pass values in the old unit.

**Rules**:
1. ❌ **NEVER change parameter units without updating all callers**
   - ❌ Bad: Changing `delay_ms(u32 ms)` to `delay_us(u32 us)` but leaving callers passing `1` instead of `1000`
   - ✅ Good: Update signature AND all call sites in the same diff
2. ✅ **Verify by searching for all callers** when a function signature changes units

**Check Process**:
1. Scan diff for function signature changes where parameter names contain unit suffixes (`_ms`, `_us`, `_sec`, `_bytes`, `_bits`)
2. If a unit change is detected, verify ALL call sites in the codebase are updated
3. If incomplete: Report violation and list the missed call sites

### src/** changes - UNUSED VARIABLES AFTER REFACTORING

**Core Principle**: When refactoring code, remove variables that are no longer referenced. Dead variables are noise and can mislead readers about data flow.

**Rules**:
1. ❌ **Remove variables whose only surviving reference is their declaration**
   - ❌ Bad: `u32 lastWarnTime = startTime;` with no subsequent reads after surrounding code was deleted
   - ✅ Good: Delete the declaration entirely
2. ❌ **Remove variables assigned but never read** (write-only variables)

**Check Process**:
1. In the diff, check if deleted code was the only reader of a variable that remains declared
2. If the variable is now write-only or unreferenced: Remove it and report

### src/** changes - DMA/POLL WAIT LOOPS MUST YIELD VIA fl::task::run()

**Core Principle**: Any busy-wait loop that polls for hardware completion (DMA, SPI, I2S, etc.) must yield to the OS scheduler using `fl::task::run()`. Never use bare `fl::yield()`, `vTaskDelay()`, `taskYIELD()`, or spin without yielding.

**Rules**:
1. ❌ **NEVER spin without yielding**
   - ❌ Bad: `while (mBusy) { poll(); }`
   - ✅ Good: `while (mBusy) { poll(); fl::task::run(250, fl::task::ExecFlags::SYSTEM); }`
2. ❌ **NEVER use bare OS yield primitives** — `fl::task::run()` is the unified API
   - ❌ Bad: `fl::yield();`, `vTaskDelay(0);`, `taskYIELD();`
   - ✅ Good: `fl::task::run(250, fl::task::ExecFlags::SYSTEM);`
3. ✅ **Use `ExecFlags::SYSTEM`** for tight DMA polling (minimal overhead, OS yield only)
4. ✅ **Use `ExecFlags::ALL`** for longer waits where pumping coroutines/tasks is beneficial
5. ✅ **Include `fl/task/executor.h`** for `fl::task::run()` and `ExecFlags`

**Rationale**: On ESP32 (FreeRTOS), spinning without yield starves WiFi/BT/system tasks and triggers the task watchdog timer (TWDT). `fl::task::run()` calls `vTaskDelay(0)` on ESP32 and `std::this_thread::yield()` on host platforms.

**Check Process**:
1. Scan for `while (` loops in driver/peripheral code that contain `poll()`, `isBusy()`, or similar status checks
2. Verify the loop body includes `fl::task::run(...)` or equivalent
3. Scan for bare `fl::yield()`, `vTaskDelay(`, `taskYIELD()` — should be `fl::task::run()`
4. If found: Report violation and fix

### src/** changes - PERFORMANCE ATTRIBUTES ON HOT-PATH FUNCTIONS

**Core Principle**: Performance-critical functions should use FastLED's optimization macros rather than relying on global optimization settings.

**Available Macros**:
- `FL_OPTIMIZE_FUNCTION` — forces `-O2` even in debug builds (for functions that must be fast)
- `FL_NO_INLINE_IF_AVR` — prevents inlining on AVR to reduce register pressure and code bloat
- `FL_BUILTIN_MEMCPY` — uses compiler built-in memcpy instead of libc call

**When to flag**:
- New or modified functions in known hot-path files (`blur.cpp.hpp`, `scale8.h`, `primitives.h`, color math)
- Functions doing tight loops over pixel data without `FL_OPTIMIZE_FUNCTION`
- Functions with large stack frames on AVR without `FL_NO_INLINE_IF_AVR`

**Check Process**:
1. For changes in hot-path files, check if new functions have appropriate optimization attributes
2. If missing: Suggest adding the appropriate macro and report

## Output Format

```
## Code Review Results

### File-by-file Analysis
- **src/file.cpp**: [no issues / violations found]
- **src/sub_controller.h**: [VIOLATION: Filename mismatch - contains Channel class, should be channel.h]
- **examples/file.ino**: [status and action taken]
- **ci/script.py**: [violations / fixes applied]

### Summary
- Files reviewed: N
- Violations found: N
  - Try-catch blocks: N
  - Platform headers in .h files: N
  - Filename mismatches: N
  - Span usage / raw pointer pairs: N
  - Arduino String usage: N
  - Type annotation issues: N
  - Threading in mocks/tests: N
  - Singleton/ThreadLocal misuse: N
  - Alignment attribute violations: N
  - Unnecessary suppression comments: N
  - Signed integer overflow (UB): N
  - Redundant virtual on override: N
  - FL_CHECK vs FL_REQUIRE misuse: N
  - Test stack-use-after-scope: N
  - API unit change propagation: N
  - Platform version guards missing: N
  - Unused variables after refactoring: N
  - Missing performance attributes: N
  - DMA/poll wait loops missing yield: N
  - Public Settings Pattern violations (bare fl::set_* without CFastLED wrapper): N
  - Other: N
- Violations fixed: N
- User confirmations needed: N
```

## Instructions
- Use git commands to examine changes
- Be thorough and check EVERY file against these rules
- Make corrections directly when safe (type annotations, removing bad *.ino files)
- Ask for user confirmation when removing/keeping questionable code
- Report all findings clearly
