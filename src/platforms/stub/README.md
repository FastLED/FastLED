# FastLED Platform: stub

Native (no‑hardware) platform used for tests and host builds; may delegate to WASM clockless for browser builds or a generic stub.

## Files (quick pass)
- `fastled_stub.h`: Aggregator enabling stub clockless and SPI; defines `HAS_HARDWARE_PIN_SUPPORT` for compatibility.
- `clockless_stub.h`: Selects between WASM clockless (`emscripten`) or `clockless_stub_generic.h` when `FASTLED_STUB_IMPL` is set.

## Subdirectories
- `generic/`: Generic stub sysdefs/pin helpers used when not targeting WASM.

## Behavior
- Used by C++ unit tests and host builds to provide a hardware‑free implementation. When compiled to WASM, this path can route to the WASM clockless backend for browser visualization.

## Unit test usage

- The stub platform is the default for C++ unit tests. It provides no‑hardware implementations for pin, SPI, and clockless paths so rendering logic can be validated on hosts.
- Tests that verify export behavior use the `shared/active_strip_data` facilities to snapshot frame/strip data without physical LEDs.
- When targeting browser demos, the stub can route to the WASM clockless backend under `emscripten` to visualize frames without hardware.
