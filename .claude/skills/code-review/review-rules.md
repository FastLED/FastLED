# Code Review Rules Reference

## src/** changes - STRICT RULES
- NO try-catch blocks allowed
- NO try-except blocks allowed
- Use alternative error handling patterns (return values, error codes, etc.)
- If found: Report violation with exact line number and fix or ask user

## src/** and examples/** C++ changes - SPAN USAGE MANDATES
**Core Principle**: Use `fl::span` instead of raw pointer+size pairs for contiguous data.

**Rules**:
1. NEVER pass raw `(pointer, size)` pairs between FastLED functions when `fl::span` can be used
   - Bad: `void process(const u8* data, size_t len)`
   - Good: `void process(fl::span<const u8> data)`
2. NEVER pass raw `T*` for fixed-size arrays when the size is known at compile time — use `fl::span<T, N>` (static extent)
   - Bad: `void processFFT(const float* bins)` (caller must know it's 16 elements)
   - Good: `void processFFT(fl::span<const float, 16> bins)` (size enforced by compiler)
   - This prevents buffer overreads where a single-element pointer is passed to a function expecting N elements
   - `float arr[16]` and `fl::array<T, 16>` both implicitly convert to `fl::span<T, 16>`
3. NEVER use Arduino `String` type in FastLED code
   - Use `fl::string` instead
   - Exception: Unavoidable when calling external library APIs
   - In such cases, convert to `fl::string` immediately at the API boundary
4. Raw pointer+size is OK for external API boundaries
5. `fl::span` auto-converts from containers — pass containers directly

**Check Process**:
1. Scan for function signatures with `(const u8* data, size_t size)` or similar
2. Scan for function signatures with `(const T* ptr)` that iterate a fixed number of elements — suggest `span<const T, N>`
3. Scan for `Arduino String` usage
4. Scan for raw pointer casts passed directly without span intermediary

## examples/** changes - QUALITY CONTROL
1. Check for new *.ino files (newly added files)
2. Evaluate each new *.ino for "AI slop":
   - Generic, boilerplate comments
   - Overly verbose or redundant code
   - No meaningful functionality
   - Placeholder patterns or incomplete logic
   - Copied/duplicated code from other examples
3. Action:
   - If AI slop detected: Remove the file and report
   - If questionable quality: Ask user
   - If acceptable: Keep and note it passed

## ci/**.py changes - TYPE SAFETY & INTERRUPT HANDLING
1. **KeyboardInterrupt Handling**:
   - ANY try-except catching general exceptions MUST also handle `KeyboardInterrupt`
   - Pattern: `except (KeyboardInterrupt, SomeException): import _thread; _thread.interrupt_main()`
2. **NO partial type annotations**:
   - Bad: `my_list = []`
   - Good: `my_list: list[str] = []`

## **/meson.build changes - BUILD SYSTEM ARCHITECTURE
**Critical Rules**:
1. NO embedded Python scripts — Extract to standalone `.py` files
2. NO code duplication — Use loops/functions
3. Configuration as data — Hardcoded values go in Python config files
4. External scripts for complex logic

## **/*.h and **/*.cpp changes - PLATFORM HEADER ISOLATION
Platform-specific headers MUST ONLY be included in .cpp files, NEVER in .h files.

**Platform-Specific Headers**:
- ESP32: `soc/*.h`, `driver/*.h`, `esp_*.h`, `freertos/*.h`, `rom/*.h`
- STM32: `stm32*.h`, `hal/*.h`, `cmsis/*.h`
- Arduino: `Arduino.h` (exception: may be in headers when needed)
- AVR: `avr/*.h`, `util/*.h`
- Teensy: `core_pins.h`, `kinetis.h`

## **/*.h and **/*.cpp changes - FILE/CLASS NAME NORMALIZATION
**Naming Convention Rules**:
1. File name matches primary class name (snake_case)
2. Classes use PascalCase
3. Name by WHAT it IS, not by role

## src/** changes - SIGNED INTEGER OVERFLOW (UNDEFINED BEHAVIOR)
All potentially-overflowing arithmetic on signed types must be performed in the corresponding unsigned type, then cast back.

**Rules**:
1. NEVER add/subtract signed integers that may overflow without casting to unsigned first
2. NEVER negate a signed value without casting to unsigned first
3. NEVER left-shift a signed value
4. NEVER negate a signed value when assigning to unsigned

## **/*.h and **/*.cpp changes - REDUNDANT VIRTUAL ON OVERRIDE
1. NEVER use `virtual` together with `override` — `override` already implies virtuality
2. NEVER omit `override` on methods that override a base class virtual

## src/** changes - SINGLETON AND THREAD-LOCAL SINGLETON USAGE
1. NEVER use raw `static ThreadLocal<T>` — use `SingletonThreadLocal<T>::instance()`
2. NEVER use the C++ `thread_local` keyword — use `SingletonThreadLocal<T>::instance()`
3. NEVER put locking in header-only types — put the lock in the singleton's T (in .cpp.hpp)
4. `fl::Singleton<T>` and `fl::SingletonThreadLocal<T>` MUST be in `.cpp.hpp` files, NEVER in `.h` headers
   - Exception: `fl::SingletonShared<T>` — designed for `.h` headers

## src/** changes - ALIGNMENT ATTRIBUTES FOR RAW STORAGE
1. NEVER use unaligned `char[]` for placement new — use `FL_ALIGN_AS_T` or `FL_ALIGNAS`
2. NEVER use bare `alignas()` or `__attribute__((aligned(...)))` directly
3. Wrap aligned buffer in a struct for portability

## tests/** and **/*_mock.* changes - AVOID THREADING IN MOCKS
**Anti-Patterns to Flag**:
1. Background threads (`fl::thread`, `std::thread`)
2. Synchronization primitives (`fl::mutex`, `fl::condition_variable`, `fl::atomic`)
3. Timing-based behavior (`fl::micros()`, `fl::millis()` for completion detection)
4. Sleep/poll loops

**Recommended**: Synchronous callbacks, simulated time, re-entrancy guards, no threads.

## tests/** changes - FL_CHECK vs FL_REQUIRE FOR PRECONDITIONS
- NEVER use `FL_CHECK` when subsequent code depends on the condition — use `FL_REQUIRE`
- `FL_CHECK` is fine for non-critical assertions where execution can safely continue

## tests/** changes - STACK-USE-AFTER-SCOPE WITH LED ARRAYS
Tests that register stack-allocated `CRGB` arrays with `FastLED.addLeds()` must detach those pointers before the array goes out of scope.

## src/platforms/** changes - MISSING PLATFORM VERSION GUARDS
Platform SDK functions that may not exist in all versions must be wrapped with version guards.

## src/** and tests/** changes - UNNECESSARY SUPPRESSION COMMENTS
**Known Suppression Comments**: `// ok bare allocation`, `// ok sleep for`, `// ok thread_local`, `// ok header path`, `// ok include path`, `// ok reinterpret cast`, `// ok include`, `// ok platform headers`, `// ok relative include`, `// ok static in header`, `// ok no namespace fl`, `// ok span from pointer`, `// ok bare using`, `// ok no header`, `// ok reading register`, `// okay banned header`, `// NOLINT`

- Flag every NEW suppression comment — verify the suppressed pattern actually exists
- Remove suppressions where the violation was fixed
- IWYU pragmas are exempt from audit

## src/** changes - API UNIT CHANGES MUST PROPAGATE
When changing parameter units (e.g., ms to us), all call sites must be updated in the same changeset.

## src/** changes - UNUSED VARIABLES AFTER REFACTORING
Remove variables that are no longer referenced after refactoring.

## src/** changes - PERFORMANCE ATTRIBUTES ON HOT-PATH FUNCTIONS
Available macros: `FL_OPTIMIZE_FUNCTION`, `FL_NO_INLINE_IF_AVR`, `FL_BUILTIN_MEMCPY`
Flag new functions in hot-path files missing appropriate optimization attributes.

## src/** changes - FL_WARN / FL_ERROR DEFAULT-VISIBILITY INVARIANT
`FL_WARN` and `FL_ERROR` MUST remain active by default on NON-release
builds. Flag any change that:

1. Adds an outer guard around `FL_WARN(...)` / `FL_ERROR(...)` that
   requires an opt-in macro to fire on debug/non-release builds.
2. Changes the unset default of `FASTLED_LOG_VERBOSITY` so that
   non-release builds (no `NDEBUG`) default below `1`. Release builds
   (`NDEBUG` defined) default to `0` — that is intentional and not
   a violation.
3. Adds a new logging knob whose default suppresses FL_WARN/FL_ERROR
   on non-release builds without explicit user opt-in.

Rationale: developers depend on warnings/errors firing during
development. The bloat-reduction work in #2886 is scoped to release
builds via `NDEBUG`; it explicitly preserves the debug-build default
at verbosity 1.

**See:** `src/fl/log/log.h` (resolution order: `FASTLED_TESTING` → 1,
`NDEBUG` → 0, otherwise → 1) and #2886 Stage 1.
