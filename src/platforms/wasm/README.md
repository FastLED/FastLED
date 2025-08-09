# FastLED Platform: wasm

WebAssembly/browser platform with C++ ↔ JavaScript bridges and a pure data‑export pattern.

## Files and directories (quick pass)
- `js_bindings.cpp`: Critical C++↔JS bridge (EMSCRIPTEN_KEEPALIVE functions). Exports frame/strip/UI data as JSON and avoids embedded JS. JavaScript side handles async via modules in `compiler/modules/`.
- `entry_point.cpp`, `timer.cpp`, `ui.cpp`, `js.cpp`, `fs_wasm.*`, `fastspi_wasm.*`: WASM platform runtime and I/O.
- `compiler/`: Browser launcher and JS modules (async controller, events, graphics, UI, audio). Also includes `build_flags.toml` for WASM builds.

## Asyncify and exported function contract

When adding or modifying C++ ↔ JS bridge functions:

- Use Asyncify.handleAsync on the JavaScript side for any operation that can yield (I/O, timers, UI waits). Avoid callback chains; prefer async/await for clarity and correctness.
- Keep exported C functions stable. Treat names and parameter/return types as a strict ABI with the JS loader. If a signature must change, update both C++ and `compiler/modules/` consumers together.
- Prefer a data-export pattern: allocate in C++, return ownership/size to JS, and provide matching free functions (e.g., `getFrameData`/`freeFrameData`). Do not embed JS in C++; centralize browser logic under `compiler/modules/`.
- Match official Emscripten header prototypes exactly for any externs (for example, `extern "C" void emscripten_sleep(unsigned int ms);`).

Recommended exported patterns:

- Buffer export: `extern "C" EMSCRIPTEN_KEEPALIVE const uint8_t* getFrameData(uint32_t* out_size);` paired with `freeFrameData(const uint8_t* ptr)`.
- JSON export: return `const char*` to a stable, heap-allocated UTF-8 string with a corresponding free.
- UI bridge: expose functions that poll/apply updates on frame boundaries (e.g., `applyPendingUiJson()`), and keep them non-blocking.

## Behavior and constraints
- Follows a pure data‑export approach for C++↔JS boundaries: C++ allocates/returns buffers and JS polls/reads; no embedded JS in C++.
- Exported function signatures are a contract with JS (e.g., `getFrameData`, `freeFrameData`, `getStripPixelData`); changes require synchronized JS updates.

## Optional feature defines

- **`FASTLED_STUB_IMPL`**: Enables stubbed Arduino/system behaviors for WASM builds (set in `led_sysdefs_wasm.h`).
- **`FASTLED_HAS_MILLIS`**: Default `1`.
- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`.

Define before including `FastLED.h` if overriding defaults.
