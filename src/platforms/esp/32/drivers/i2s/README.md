# Classic-ESP32 I2S Parallel-Out Clockless Driver

Modern channel-manager driver that uses the I2S1 peripheral in parallel-out mode to drive up to 24 WS2812-family LED strips simultaneously on classic ESP32 (`FL_IS_ESP_32DEV`). Real DMA + IRAM ISR + variable-clock waveN encoding — same-peripheral unified clockless + SPI dispatch per the parallel-IO rule.

## Attribution

**Original I2S1 parallel-out driver: Yves Bazin** — first FastLED contributor to prove that classic-ESP32 I2S1 in LCD/parallel mode could drive multiple WS2812-family strips in lock-step through a DMA + ISR pipeline. His original driver (`ClocklessI2S<>` + `i2s_esp32dev`) was FastLED's I2S parallel-out solution from 2018 through FastLED#3526 Phase 2e (2026-07-01). Every register write in `initialize()`, every DMA descriptor field in `transmit()`, and the clock-divider math for the pixel-clock rate all trace their lineage to Yves's original driver. The Yves files were deleted at Phase 2e once this modern engine reached parity, but Yves's work is the foundation everything here stands on.

## Files

| File | Purpose |
|---|---|
| `channel_engine_i2s_esp32dev.{h,cpp.hpp}` | `IChannelDriver` engine — READY → BUSY → READY state machine, per-batch clockless/SPI dispatch |
| `wave8_encoder_i2s1.{h,cpp.hpp}` | Wave8 + Wave3 pulse-major encoders (call shared `fl::wave8Transpose_*` / `wave3Transpose_16` kernels) |
| `ii2s_peripheral_esp32dev.h` | Peripheral interface (`initialize`, `transmit`, `routeLanePin`, `waitTransmitDone`, callbacks) |
| `i2s_peripheral_esp32dev_esp.{h,cpp.hpp}` | Real-hardware peripheral (`fl::Singleton<>` instance, direct `soc/i2s_struct.h` register writes, `esp_intr_alloc()` ISR) |
| `i2s_peripheral_esp32dev_mock.{h,cpp.hpp}` | Host-test mock — captures every call for byte-exact test assertions |
| `i2s_periph_compat.h` | IDF-version peripheral clock/reset compat shim |
| `channel_driver_i2s.{h,cpp.hpp}` | ESP32-S3 LCD_CAM legacy driver (S3-only, unrelated to classic ESP32) |
| `bus_traits.h` | ESP32-S3 LCD_CAM `BusTraits` placeholder |

The classic-ESP32 modern engine binds to `Bus::FLEX_IO, 0` via `../i2s_spi/bus_traits.h` (kept there for now to avoid churning the `_build.cpp.hpp` on the same PR that flipped the binding — deleted at the follow-up SPI fold-in).

## User-facing binding

Runtime driver selection through the channel manager — legacy `-DFASTLED_ESP32_I2S=1` compile-time opt-in was **removed at FastLED#3526 Phase 2e**.

### Default `addLeds<>` (RMT)

By default, `FastLED.addLeds<WS2812, PIN, GRB>()` still routes to RMT on classic ESP32. To use the modern I2S1 engine, opt in via:

```cpp
FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
FastLED.addLeds<WS2812, 18, GRB>(leds, N);   // up to 24 parallel strips
```

### Modern channel API (async, non-blocking)

For non-blocking output (issue #1384's use case — CPU processes network I/O while DMA runs):

```cpp
auto ch = fl::Channel::create<fl::Bus::FLEX_IO, 0, WS2812, GRB>(leds, N, /*pin=*/18);
FastLED.add(ch);
// Later:
ch->show();       // returns immediately, DMA runs in background
while (ch->poll() == fl::DriverState::BUSY) { /* do other work */ }
```

## ESP-IDF version compatibility

| IDF | Status | Path |
|---|---|---|
| 4.x | ✅ Production | `periph_module_enable(PERIPH_I2S1_MODULE)` — public API |
| 5.x | ✅ Production | Same as 4.x |
| 6.x | ✅ Compiles (bench validation pending — tracked in #3515 Phase D) | `i2s_ll_enable_bus_clock(1, true)` + `i2s_ll_reset_register(1)` from `hal/i2s_ll.h`, wrapped in `PERIPH_RCC_ATOMIC()` per PR #3511 + PR #3514. |

Version dispatch: `i2s_periph_compat.h` behind a single `fl::fl_i2s_periph_enable(port)` call.

## Variable-rate waveN encoding

The peripheral's `initialize()` computes the I2S1 clock divider (N/A/B) at runtime from the config's `mPixelClockHz` field. Not hardcoded to 8 MHz — supports:

- WS2812 at 800 kHz → 8 MHz clock (N=10, A=1, B=0)
- WS2811 at 400 kHz → 3.2 MHz clock (N=25, A=1, B=0)
- Any wave3-eligible chipset at its per-chipset rate via `wave3ClockFrequencyHz(timing)`

Falls back to 8 MHz if `mPixelClockHz` is zero. Divider solver is `solveI2sClockDivider()` in `i2s_peripheral_esp32dev_esp.cpp.hpp` — adapted from Yves's `i2s_define_bit_patterns` clock math. See FastLED#3562 for hoisting this helper to a shared location so other ESP32 drivers can consume it.

## Streaming ISR DMA refill invariant (REQUIRED)

The whole point of using I2S parallel mode over RMT is streaming — the DMA fires an EOF interrupt as each buffer drains, the ISR refills that buffer with the next row's encoded bits, and the DMA engine continues onto the next buffer in the ring.

**Current state (Phase 2b step B, PR #3557):** single-descriptor DMA. Cap: 4092 bytes per transmit ≈ 10 LEDs/strip at wave8 encoding.

**Follow-up (Phase 2b step D, tracked in #3515):** multi-descriptor ring with ISR-driven refill. Yves's original code has the reference pattern (`NUM_DMA_BUFFERS`, `qe.stqe_next` circular chain, `gCallback` inside the ISR) preserved in git history at PR #3557's parent commit.

**Invariant when Phase 2b step D lands:** the ISR handler must stay `FL_IRAM`-resident, call the refill callback on every buffer-EOF while `!done`, and defer the completion callback until all buffers have drained.

## Multi-lane GPIO routing (up to 24)

Classic ESP32 I2S{0,1} each expose 24 parallel data output signals (`I2S{n}O_DATA_OUT0_IDX` .. `I2S{n}O_DATA_OUT23_IDX` per `soc/gpio_sig_map.h`). Route pins via:

```cpp
peripheral->routeLanePin(lane_idx, gpio_pin);  // lane 0..23, pin 0..39
peripheral->routeLanePin(lane_idx, -1);        // detach: routes SIG_GPIO_OUT_IDX in its place
```

`routeLanePin` handles the pad-select (via `esp_rom_gpio_pad_select_gpio`) so pins booting in an alternate function (SD_CMD on GPIO11 in some board configs, etc.) get switched to plain GPIO before matrix routing takes effect. Honors the peripheral config's `mInvertMask` — chipsets that need polarity inversion get it "for free" through the matrix.

Wave8 encoder is currently 16-lane wide; lanes 16..23 are reserved for a future `wave8Transpose_24` (or padded-32) kernel — the peripheral surface already accepts the full hardware range so the encoder upgrade doesn't have to churn this API.

**Historical bug: FastLED#1165** — Yves's `-DFASTLED_ESP32_I2S=1` path silently misrouted GPIO when `I2S_DEVICE=1`. Fixed at Phase 2b step C: the port number is an explicit config field, and `routeLanePin` picks the correct `I2S{n}O_DATA_OUT{lane}_IDX` per port.

## Peripheral ownership

`I2sPeripheralEsp32DevEsp` is a `fl::Singleton<>` — one process-wide instance, never destroyed. The singleton's `mInitialized` member is the sole ownership record for I2S1 on classic ESP32. Concurrent `initialize()` attempts fail with a warning.

`createI2sEsp32DevEngine()` wraps the singleton in `fl::make_shared_no_tracking<II2sPeripheralEsp32Dev>(singleton)` — zero-overhead non-owning handle, no control block, no `delete` possible.

## Parallel-IO unified-engine rule

Per `agents/docs/cpp-standards.md` → "Parallel-IO Driver: Unified Clockless + SPI Engine": the same engine handles BOTH clockless and SPI on the same peripheral. The engine's `canHandle()` accepts both `data->isClockless()` and `data->isSpi()`. `show()` dispatches per-batch:

- Homogeneous clockless batch → wave8/wave3 encoding, DMA transmit
- Homogeneous SPI batch → delegate to `createI2sSpiEngine()` (folded inline at follow-up)
- Mixed batch → rejected (I2S1 can only run one mode at a time)

The delegate call chain is why `drivers/i2s_spi/` still exists post-Phase-2e. Full deletion is tracked in the follow-up SPI fold-in.

## Known follow-ups

- **Phase 2b step D — streaming multi-descriptor DMA ring** (tracked in #3515)
- **`drivers/i2s_spi/` full directory deletion** — needs folding I2S_SPI transmit logic INLINE into the modern engine's SPI branch
- **WROOM bench validation matrix** — 4+ parallel WS2812B strips clockless + APA102 SPI in same run, byte-exact RX loopback via `rmt_rx_4` for clockless / wire capture for SPI (tracked in #3515 Phase D)
- **PARLIO variable-timing** — apply the wavN variable-rate pattern to PARLIO (FastLED#3561)
- **Hoist `solveI2sClockDivider`** — make it available to SPI, RMT, and other ESP32 drivers (FastLED#3562)

## References

- Umbrella: [FastLED#3516](https://github.com/FastLED/FastLED/issues/3516) — classic-ESP32 I2S driver dev meta
- Modern-engine architecture directive: [FastLED#3526](https://github.com/FastLED/FastLED/issues/3526) — wave8/wave3 general clockless + SPI unified (this PR series' scope)
- Bring-up: [FastLED#3515](https://github.com/FastLED/FastLED/issues/3515) — AutoResearch validation on IDF 4/5/6 (Phase D is bench)
- Architecture unified-path: [FastLED#3512](https://github.com/FastLED/FastLED/issues/3512) — Phases 2, 4-8 (superseded by #3526's fresh-implementation approach; original plan was to wrap Yves, which was rejected)
- Singleton migration: [FastLED#3489](https://github.com/FastLED/FastLED/issues/3489) (closed by #3526 Phase 2e — Yves globals deleted with the files)
- I2S_DEVICE=1 pin routing: [FastLED#1165](https://github.com/FastLED/FastLED/issues/1165) (closed by #3526 Phase 2b step C — `routeLanePin` handles both ports)
- Async surface: [FastLED#1384](https://github.com/FastLED/FastLED/issues/1384) (closed by #3526 Phase 2c — `Channel::create<>` + `poll()` provides non-blocking API)
- IDF 6 shim: [PR #3511](https://github.com/FastLED/FastLED/pull/3511) + [PR #3514](https://github.com/FastLED/FastLED/pull/3514)
- Parallel-IO unified-engine rule: `agents/docs/cpp-standards.md`
