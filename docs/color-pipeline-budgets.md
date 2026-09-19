# Colour pipeline: flash, RAM and throughput budgets (P9)

P9 (#4043) asks for the pipeline's bloat and throughput budgets to be recorded
for representative platforms, and for the TINY tier to link no float or solver
symbols. These are measurements, with the commands that reproduce them. Numbers
move as the pipeline does, so re-measure rather than quote when it matters.

## What binding a profile costs

Measured with `examples/ColorProfile`, built twice per board: once as shipped,
and once with `-DFASTLED_EXAMPLE_NO_COLOR_PROFILE=1`, which removes the
`setColorProfile` call and nothing else. Both builds ask for `BINARY_DITHER`,
so the dither setting is identical. What differs is that one channel is
colour-managed, so the delta is the whole pipeline, including the temporal
dither code a managed channel with `BINARY_DITHER` runs.

```
bash compile <board> --examples ColorProfile
bash compile <board> --examples ColorProfile --defines FASTLED_EXAMPLE_NO_COLOR_PROFILE=1
```

| board | CPU | flash | RAM |
|---|---|---:|---:|
| `esp32dev` | ESP32 (Xtensa LX6) | +10,436 B | +544 B |
| `esp32s3` | ESP32-S3 (Xtensa LX7) | +10,708 B | +536 B |
| `adafruit_feather_m4` | SAMD51 (Cortex-M4F) | +11,200 B | +540 B |
| `teensy41` | i.MX RT1062 (Cortex-M7) | +10,240 B | +2,624 B |

Teensy's figures are rounded to its linker's reporting granularity.

- **Flash.** About 10–11 KB is the whole pipeline: decode, gamut mapping, device solve, flux and dither. The bind-time derivation is included; it still uses float (see below).
- **RAM.** About 540 B is mostly the per-channel pipeline (held behind a pointer, #4440) and the hook table. On Teensy the extra is its `.data`/`.bss` rounding, not the pipeline.
- **Not calling it costs nothing.** A sketch whose code never calls `setColorProfile` links none of this. The whole pipeline is reached through `ColorPipelineHooks`, installed only by `setColorProfile`. uno Blink is byte-identical to before the pipeline existed (5170 B flash / 633 B RAM). On ESP32-S3, `examples/ColorProfile` built with and without the `setColorProfile` call differs by about 100 symbols, and none of them is in Blink. The ESP32-S3 bloat gate enforces this: it fails if Blink links any pipeline entry point, and names the symbol (#4455). What an unbound build does carry is the profile API itself: the empty hook table, two `ColorProfileEvent` listener lists and the `colorPipeline()` accessors, about 0.7 KB.
- **Compiled in but never called costs the whole pipeline.** Linking is decided at compile time, by whether any code path calls `setColorProfile`, not by whether that call runs. `examples/ColorProfile` on ESP32-S3 with the call behind a `volatile` flag that is never set: 588,004 B flash / 84,808 B RAM, against 587,984 / 84,808 bound and 576,268 / 84,264 without the call. That is +11,736 B flash and +544 B static RAM for a profile that is never bound, the same as binding one (+11,716 B / +544 B). All three are `bash compile` image sizes, the same method as the table above, but measured on master `8999762397` (2026-09-19); the table's +10,708 B / +536 B is from an earlier master, before the pipeline grew. All ~100 pipeline symbols link. At runtime an unbound channel takes the legacy path: the hooks are installed only when `setColorProfile` runs, so each frame costs a null check per channel and nothing per pixel. To pay nothing in flash, exclude the call at compile time, as `FASTLED_EXAMPLE_NO_COLOR_PROFILE` does in the example.

## Per-pixel throughput on the real path

`colorPipelinePerf`, an AutoResearch RPC, times `ColorManagedPixelSource::loadAndScaleRGB`. That is decode, gamut map, device solve, flux and the final quantize: exactly what an encoder pulls per pixel on a managed channel. It is timed against the legacy `loadAndScale0/1/2` over the same 256-pixel buffer, 20 frames each.

```
bash autoresearch <board> --rpc-smoke        # builds, deploys (fbuild), smoke-tests
uv run python -m ci.autoresearch.rpc_bench <port> colorPipelinePerf \
    --args '[{"pixels":256,"frames":20,"dither":false}]'
```

Measured on the bench, 2026-09-19:

| board | CPU | managed | legacy | ratio | managed px/s | + temporal dither |
|---|---|---:|---:|---:|---:|---:|
| ESP32-C6 | RISC-V, 160 MHz | 4.76 µs/px | 0.27 µs/px | 17.8× | 210,000 | 4.99 µs/px |
| Pico 2 W (RP2350) | Cortex-M33, 150 MHz | 4.71 µs/px | 0.33 µs/px | 14.4× | 212,000 | 4.92 µs/px |

- **What this means for a sketch.** At 60 fps one core can push about 3,500 colour-managed pixels per frame before the pipeline alone fills the frame, against about 60,000 on the legacy path. C2's streaming model runs the pipeline inside the encode, so this is also the cost a parallel-output driver pays per pixel per lane.
- **Determinism.** The RPC returns an order-sensitive FNV-1a over every byte each path emits. With dither off, both boards return the same managed checksum (`1509334301`) and the same legacy one (`952363461`). That is strong evidence, not proof, that the fixed-point per-pixel path emits the same bytes on RISC-V and ARM: a 32-bit hash can collide, so treat a match as a regression signal and a mismatch as a definite difference. With dither on, the managed checksums differ between boards, and they should. The phase comes from the shared frame counter, which reflects how many frames each board has shown. The legacy checksums happen to agree there only because, at full scale, its offset rounds to 0 for most phases.
- **Bounded.** The RPC refuses more than 200,000 pixel-frames (about a second), because it runs synchronously inside the RPC handler and the watchdog is fed only after it returns.

## TINY tier: no pipeline, no float, no per-controller state

On TINY (`FL_PLATFORM_HAS_TINY_MEMORY`) the colour pipeline is compiled out: `FL_COLOR_PROFILE_RUNTIME` is 0. Symbol inspection of Blink:

```
bash compile attiny85 --examples Blink    # 3492 B flash, 421 B RAM
bash compile uno --examples Blink         # 5170 B flash, 633 B RAM
avr-nm -C firmware.elf | grep -iE 'Pipeline|processPixelQ16|gamut|oklab|colorimetric|solve|StreamingPipeline'
avr-nm firmware.elf    | grep -E '__(add|sub|mul|div)sf3|__fix|__float|__gesf2|__ltsf2'
```

| board | pipeline symbols | float runtime symbols |
|---|---|---|
| attiny85 (TINY) | none | none |
| uno (low tier) | none | none |

The one name the pattern matches is `CLEDController::staticEmitterProfile()`. It is a 6-byte virtual that returns null, and it is the only trace of the profile API in a sketch that does not use it. No per-controller state is added on TINY: the profile binding in `ChannelOptions` exists only under `FL_COLOR_PROFILE_RUNTIME`.

## What this does not cover

- **Bind time is float-free too (#4458).** Binding a profile no longer reaches the soft-float runtime. The profile's floats are converted through their IEEE-754 bits (`q16FromFloatBits`), and the source matrix, the Bradford adaptation and the device solve are derived in s16.16. `ci/tests/test_q16_inverse_is_float_free.py` proves it on Cortex-M0+ and Cortex-M33 without an FPU. The A1 case is unchanged: 0.3951 dE2000 worst above the floor, luminance 3.8e-4. On an ESP32 the platform links the float runtime anyway, so the saving there is only code the pipeline no longer calls.
- **Measured profiles.** The budgets above use the `WS2812B` placeholder profile. A measured profile has the same shape, so it costs the same; P10 supplies measured ones.
