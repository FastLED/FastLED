# FastLED Platform: WebAssembly (WASM)

## Optional feature defines

- `FASTLED_HAS_NETWORKING` (0|1): Enable socket networking bridges (affects POSIX/WIN/WASM socket shims). Default 0.
- `FASTLED_ALLOW_INTERRUPTS` (0|1): Default 1 for WASM.
- `FASTLED_USE_PROGMEM` (0|1): Default 0.

Notes:
- WASM builds run in the browser with Asyncify support; see project docs for JS interop.
