# FastLED Platform: wasm

WebAssembly/browser platform with C++ ↔ JavaScript bridges and a pure data‑export pattern.

## Files and directories (quick pass)
- `js_bindings.cpp`: Critical C++↔JS bridge (EMSCRIPTEN_KEEPALIVE functions). Exports frame/strip/UI data as JSON and avoids embedded JS. JavaScript side handles async via modules in `compiler/modules/`.
- `entry_point.cpp`, `timer.cpp`, `ui.cpp`, `js.cpp`, `fs_wasm.*`, `fastspi_wasm.*`: WASM platform runtime and I/O.
- `compiler/`: Browser launcher and JS modules (async controller, events, graphics, UI, audio). Also includes `build_flags.toml` for WASM builds.

## Threading Architecture (Worker Thread Mode)

**FastLED WASM uses dedicated Web Workers** (PROXY_TO_PTHREAD) for background execution. Asyncify was removed in 2025-01 to reduce binary size (44.4% reduction) and fix audio reactive mode issues.

### Multithreading Support

WASM uses the **stub platform threading profile** (identical to host testing builds):

- **Threading detection**: Defers to `fl/stl/thread.h` default logic (`FASTLED_TESTING` + `pthread.h` availability)
- **When enabled** (`FASTLED_MULTITHREADED=1`):
  - **Real mutexes**: `fl::mutex` → `std::mutex` (POSIX pthread mutexes via Web Workers)
  - **Real atomics**: `fl::atomic<T>` → `AtomicReal<T>` (using `__atomic` compiler builtins)
  - **Condition variables**: `fl::condition_variable` → `std::condition_variable`
  - **Thread-safe primitives**: All FastLED synchronization primitives are thread-safe
- **When disabled** (`FASTLED_MULTITHREADED=0`):
  - Falls back to fake mutex/atomic implementations (single-threaded mode)

Build flags in `compiler/build_flags.toml` provide pthread support:
- `-pthread` (compiler and linker)
- `-sUSE_PTHREADS=1` (Emscripten pthread implementation)
- `-sPROXY_TO_PTHREAD` (run main() on a pthread)

**Design principle**: WASM and stub platform share identical threading configuration, ensuring consistent behavior between WASM browser builds and desktop testing environments.

When adding or modifying C++ ↔ JS bridge functions:

- **Synchronous execution**: All C++ ↔ JS bridge functions execute synchronously in the worker thread. No Asyncify.handleAsync or async/await needed for WASM calls.
- **Keep exported C functions stable**: Treat names and parameter/return types as a strict ABI with the JS loader. If a signature must change, update both C++ and `compiler/modules/` consumers together.
- **Prefer a data-export pattern**: Allocate in C++, return ownership/size to JS, and provide matching free functions (e.g., `getFrameData`/`freeFrameData`). Do not embed JS in C++; centralize browser logic under `compiler/modules/`.
- **Worker thread benefits**: True background threading without blocking UI. No manual yielding needed - OS scheduler handles thread scheduling naturally.

Recommended exported patterns:

- Buffer export: `extern "C" EMSCRIPTEN_KEEPALIVE const uint8_t* getFrameData(uint32_t* out_size);` paired with `freeFrameData(const uint8_t* ptr)`.
- JSON export: return `const char*` to a stable, heap-allocated UTF-8 string with a corresponding free.
- UI bridge: expose functions that poll/apply updates on frame boundaries (e.g., `applyPendingUiJson()`), and keep them non-blocking.

## Behavior and constraints
- Follows a pure data‑export approach for C++↔JS boundaries: C++ allocates/returns buffers and JS polls/reads; no embedded JS in C++.
- Exported function signatures are a contract with JS (e.g., `getFrameData`, `freeFrameData`, `getStripPixelData`); changes require synchronized JS updates.

## Optional feature defines

- **`FASTLED_STUB_IMPL`**: Enables stubbed Arduino/system behaviors for WASM builds (set in `led_sysdefs_wasm.h`).
- **`FASTLED_MULTITHREADED`**: Inherits stub platform's detection (based on `FASTLED_TESTING` + `pthread.h`). Can be overridden before including FastLED.h.
- **`FASTLED_HAS_MILLIS`**: Default `1`.
- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`.

Define before including `FastLED.h` if overriding defaults.
