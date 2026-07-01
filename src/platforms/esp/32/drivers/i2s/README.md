# Classic-ESP32 I2S Parallel-Out Clockless Driver

Hardware driver that uses the I2S1 peripheral in parallel-out mode to drive up to 24 WS2812-family LED strips simultaneously on classic ESP32 (`FL_IS_ESP_32DEV`). Powered by a streaming DMA ring with ISR-driven buffer refill — the wire keeps running while the CPU encodes the next row of pixels.

## Files

| File | Purpose |
|---|---|
| `i2s_esp32dev.{h,cpp.hpp}` | Core Yves Bazin driver — register + DMA + IRAM ISR machinery on I2S1 |
| `clockless_i2s_esp32.{h,cpp.hpp}` | `ClocklessI2S<>` chipset template — routes `addLeds<WS2812, PIN, GRB>()` through `i2s_esp32dev` |
| `i2s_periph_compat.h` | IDF-version peripheral clock/reset compat shim (see below) |
| `ii2s_peripheral_esp32dev.h` | Modern channel-manager peripheral interface |
| `i2s_peripheral_esp32dev_esp.{h,cpp.hpp}` | Modern peripheral impl (Stage 4 — synchronous stub, waiting for Phase 2) |
| `i2s_peripheral_esp32dev_mock.{h,cpp.hpp}` | Host-test mock of the peripheral interface (66 unit tests) |
| `channel_engine_i2s_esp32dev.{h,cpp.hpp}` | Modern `IChannelDriver` engine — state machine READY → BUSY → READY |
| `channel_driver_i2s.{h,cpp.hpp}` | Modern `IChannelDriver` alias (BusTraits binding target) |
| `bus_traits.h` | `BusTraits<Bus::FLEX_IO, 0>` specialization (currently binds to I2S-SPI, not the parallel-out clockless — see Phase 4 below) |
| `wave8_encoder_i2s.{h,cpp.hpp}` | Wave8 slot encoder for the modern engine |

## User-facing binding today

Two coexisting paths — pick one, they don't share hardware state.

### Legacy `addLeds<>` template (production, IDF 4.x / 5.x / 6.x)

Set the compile-time flag, then use the standard `FastLED.addLeds<>()` template. On classic ESP32 (`FL_IS_ESP_32DEV`), that routes to `ClocklessI2S<>` which pokes the `i2s_esp32dev` globals directly.

```ini
build_flags = -DFASTLED_ESP32_I2S=1
```

```cpp
FastLED.addLeds<WS2812, 18, GRB>(leds, N);   // up to 24 parallel strips
```

This is the path validated by [PR #3479](https://github.com/FastLED/FastLED/pull/3479) on WROOM COM11. `bash autoresearch` doesn't exercise this driver yet (blocked by Phase 4 — see below).

### Modern channel-manager (`Bus::FLEX_IO, 0`) — not there yet

As of 2026-07-01, `Bus::FLEX_IO, 0` on classic ESP32 routes to the **I2S-SPI** driver (in `../i2s_spi/`), not the parallel-out clockless one. Wiring the Yves clockless path through `Bus::FLEX_IO, 0` (with subtype info in the device-info JSON) is [FastLED#3512 Phase 4](https://github.com/FastLED/FastLED/issues/3512). Once that lands:

```cpp
FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();  // opt into I2S1 parallel-out
FastLED.addLeds<WS2812, 18, GRB>(leds, N);
```

At that point `-DFASTLED_ESP32_I2S=1` is removed.

## ESP-IDF version compatibility

| IDF | Status | Path |
|---|---|---|
| 4.x | ✅ Production | `periph_module_enable(PERIPH_I2S1_MODULE)` — public API |
| 5.x | ✅ Production | Same as 4.x |
| 6.x | ✅ Compiles (bench validation pending) | `i2s_ll_enable_bus_clock(1, true)` + `i2s_ll_reset_register(1)` from `hal/i2s_ll.h`, wrapped in `PERIPH_RCC_ATOMIC()` — see [PR #3511](https://github.com/FastLED/FastLED/pull/3511) + [PR #3514](https://github.com/FastLED/FastLED/pull/3514) for the compat shim + `RCC_ATOMIC` fix verified against `espressif/esp-idf@master:components/esp_hal_i2s/esp32/include/hal/i2s_ll.h`. |

The version dispatch lives in `i2s_periph_compat.h` behind a single `fl::fl_i2s_periph_enable(port)` call.

## Streaming ISR DMA refill invariant (REQUIRED)

The whole reason to use I2S parallel mode over RMT is streaming — the DMA fires an EOF interrupt as each buffer drains, the ISR refills that buffer with the next 8-pixel row's encoded bits, and the DMA engine continues on to the next buffer in the ring. If the ISR misses a refill deadline, the wire goes silent between frames and the RX side sees gaps.

**Invariant:** the ISR handler at `i2s_esp32dev.cpp.hpp:145` must:
- Stay `FL_IRAM`-resident (never fall into flash).
- Call `gCallback()` on every buffer-EOF while `!gDoneFilling`.
- Give the completion semaphore only when all buffers have drained (`gCntBuffer == 0` in the multi-buffer case).

Any future refactor of this driver — including [FastLED#3512 Phase 2](https://github.com/FastLED/FastLED/issues/3512)'s planned migration into `I2sPeripheralEsp32DevEsp` — must preserve this pattern. Load-whole-frame-then-transmit is prohibited (defeats the parallel-IO capacity that motivates using this peripheral in the first place).

See `agents/docs/cpp-standards.md` → "Parallel-IO Driver: Unified Clockless + SPI Engine" for the broader parallel-IO invariant and the unified-engine requirement.

## Global state (transient — pending Phase 6)

The classic driver carries 10+ file-scope statics (`gCallback`, `gCntBuffer`, `dmaBuffers[]`, `gTX_semaphore`, `gI2S_intr_handle`, etc.) that are file-allowlisted for the `SingletonElisionChecker` via a `FL_LINT_ALLOW_GLOBAL_FILE` marker at the top of `i2s_esp32dev.cpp.hpp`.

[FastLED#3489](https://github.com/FastLED/FastLED/issues/3489) tracks their migration into `Singleton<I2SDriverState>::instance()` with a cached-IRAM-pointer pattern for ISR safety. That's picked up naturally by [FastLED#3512 Phase 6](https://github.com/FastLED/FastLED/issues/3512) once the peripheral migration owns the state.

## Known bugs

- **[FastLED#1165](https://github.com/FastLED/FastLED/issues/1165)** (2020) — `#define I2S_DEVICE 1` doesn't drive output. GPIO matrix routing bug in the port-1 path; fixed by [FastLED#3512 Phase 2](https://github.com/FastLED/FastLED/issues/3512)'s peripheral rewrite when the port becomes an explicit config-surface field.
- **[FastLED#1384](https://github.com/FastLED/FastLED/issues/1384)** (2020) — the semaphore-take at the end of `showPixels()` blocks the CPU, preventing network I/O interleaving. Async surface via the modern channel API arrives in [FastLED#3512 Phase 4](https://github.com/FastLED/FastLED/issues/3512).

## References

- Umbrella: [FastLED#3516](https://github.com/FastLED/FastLED/issues/3516) — classic-ESP32 I2S driver dev meta
- Bring-up: [FastLED#3515](https://github.com/FastLED/FastLED/issues/3515) — AutoResearch validation on IDF 4/5/6 (Phase D is bench)
- Architecture: [FastLED#3512](https://github.com/FastLED/FastLED/issues/3512) — unified path Phases 2, 4-8
- Singleton migration: [FastLED#3489](https://github.com/FastLED/FastLED/issues/3489)
- Production baseline restore: [PR #3479](https://github.com/FastLED/FastLED/pull/3479) — Stage 5
- IDF 6 shim: [PR #3511](https://github.com/FastLED/FastLED/pull/3511) + [PR #3514](https://github.com/FastLED/FastLED/pull/3514) (RCC_ATOMIC fix)
- Unity build re-include: [PR #3520](https://github.com/FastLED/FastLED/pull/3520)
- Runtime-selection guardrail: [PR #3519](https://github.com/FastLED/FastLED/pull/3519) — `agents/docs/cpp-standards.md` → "Runtime driver selection is `Bus::FLEX_IO`"
- IDF 6 CI target: [PR #3518](https://github.com/FastLED/FastLED/pull/3518)
