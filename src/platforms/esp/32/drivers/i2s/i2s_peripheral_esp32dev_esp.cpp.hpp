// IWYU pragma: private

/// @file i2s_peripheral_esp32dev_esp.cpp.hpp
/// @brief Real-hardware `II2sPeripheralEsp32Dev` impl for the modern
///        classic-ESP32 I2S1 parallel-out clockless engine.
///
/// ## Attribution
///
/// **Original I2S1 parallel-out clockless proof-of-concept: Yves Bazin**
/// (`ClocklessI2S<>` and the `i2s_esp32dev` register-level driver, first
/// merged as the FastLED#Yves I2S driver in 2018-2019). Yves proved
/// that classic-ESP32 I2S1 in parallel/LCD mode could drive multiple
/// WS2812-family strips in lock-step through a DMA + ISR pipeline —
/// years before ESP-IDF grew official parallel-IO peripherals like
/// PARLIO or LCD_CAM. Every register write in this impl's
/// `initialize()`, every DMA descriptor field in `transmit()`, and the
/// clock-divider math for the 8 MHz wave8 pixel rate all trace their
/// lineage to Yves's original driver. The Yves files
/// (`clockless_i2s_esp32.{h,cpp.hpp}`, `i2s_esp32dev.{h,cpp.hpp}`) were
/// deleted at FastLED#3526 Phase 2e once this modern engine reached
/// parity, but Yves's work is the foundation everything here stands on.
///
/// ## Architecture
///
/// Unlike Yves's driver (which encoded bit patterns per chipset via
/// `i2s_define_bit_patterns`), this modern impl is a **general wave8/
/// wave3 clockless driver** in the same shape as PARLIO — fixed 8 MHz
/// pixel clock, per-channel `ChipsetTimingConfig` translated to a wave8
/// byte-LUT at show time. See `channel_engine_i2s_esp32dev.h` for the
/// engine-level design and `wave8_encoder_i2s1.h` for the encoder.
///
/// Very-low-level ESP-IDF headers only (`esp_heap_caps.h`,
/// `driver/gpio.h`, `soc/i2s_struct.h`, `esp_intr_alloc.h`, etc.) — no
/// `driver/i2s.h` because that drags in `driver_ng` and clashes with
/// FastLED's legacy-ADC pin-init path (#3474).

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.h"
#include "platforms/esp/32/drivers/i2s/i2s_periph_compat.h"  // fl_i2s_periph_enable — IDF-version-safe power gate
#include "platforms/esp/32/drivers/i2s/i2s_port_claim.h"     // cross-driver I2S port ownership (FastLED#3576)

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_rom_gpio.h"  // esp_rom_gpio_connect_out_signal (replaces gpio_matrix_out)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"  // I2S1O_DATA_OUT0_IDX ..
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/soc.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

namespace {

// I2S APB clock — used to derive the clock-divider N/A/B for the
// requested `mPixelClockHz`. Classic ESP32 I2S1 runs off the 80 MHz
// APB reference; the divider formula is
// `f_out = I2S_BASE_CLK / (N + B/A)`.
constexpr u32 kI2sBaseClkHz = 80000000u;

// Owner tag for the cross-driver port-claim registry. Stable literal —
// claim identity is the pointer.
constexpr const char *kI2sClocklessOwner = "I2S_CLOCKLESS";

/// @brief Solve the classic-ESP32 I2S1 fractional clock divider
///        (`clkm_conf.clkm_div_{num,a,b}`) for a target frequency.
///
/// Selects `(N, A, B)` such that `80 MHz / (N + B/A)` best matches
/// `target_hz`. Adapted from Yves's `i2s_define_bit_patterns` — same
/// algorithm, just wrapped so it produces the divider triple without
/// building the per-chipset pulse LUT (the modern engine uses wave8
/// encoding, not per-chipset pulse patterns).
inline void solveI2sClockDivider(u32 target_hz,
                                  int* out_N, int* out_A, int* out_B) FL_NO_EXCEPT {
    if (target_hz == 0) {
        *out_N = 10;   // Default to 8 MHz (wave8 @ 800 kHz)
        *out_A = 1;
        *out_B = 0;
        return;
    }
    const double f_target = static_cast<double>(target_hz);
    const double f_base   = static_cast<double>(kI2sBaseClkHz);
    int N = static_cast<int>(f_base / f_target);
    if (N < 2) N = 2;
    const double residual = (f_base / f_target) - static_cast<double>(N);

    // Fractional part B/A — A ≤ 63 hardware limit. Sweep A, pick the
    // (A, B) that minimises |residual - B/A|.
    int best_A = 1;
    int best_B = 0;
    double best_err = residual;
    for (int A = 1; A < 64; ++A) {
        for (int B = 0; B < A; ++B) {
            const double err = residual - (static_cast<double>(B) / A);
            const double abs_err = err < 0 ? -err : err;
            if (abs_err < best_err) {
                best_err = abs_err;
                best_A = A;
                best_B = B;
            }
        }
    }
    // Double-precision 0.9999 corner: `A == B` collapses to the integer
    // next up. Kept from Yves's original code.
    if (best_A == best_B) {
        best_A = 1;
        best_B = 0;
        ++N;
    }
    *out_N = N;
    *out_A = best_A;
    *out_B = best_B;
}

// Register-level reset helpers — inline replacements for Yves's
// `i2s_reset*` free functions so the modern peripheral does not link
// against `i2s_esp32dev.cpp.hpp`. Bit-identical to Yves's implementations
// (same reset-flag mask, same set-then-clear sequence). See Yves source
// for register-map derivation.
inline void reset_i2s_registers(i2s_dev_t* i2s) FL_NO_EXCEPT {
    const u32 lc_conf_reset_flags =
        I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
    i2s->lc_conf.val |= lc_conf_reset_flags;
    i2s->lc_conf.val &= ~lc_conf_reset_flags;

    const u32 conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M |
                                  I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    i2s->conf.val |= conf_reset_flags;
    i2s->conf.val &= ~conf_reset_flags;
}

inline void reset_i2s_dma(i2s_dev_t* i2s) FL_NO_EXCEPT {
    i2s->lc_conf.in_rst = 1;
    i2s->lc_conf.in_rst = 0;
    i2s->lc_conf.out_rst = 1;
    i2s->lc_conf.out_rst = 0;
}

inline void reset_i2s_fifo(i2s_dev_t* i2s) FL_NO_EXCEPT {
    i2s->conf.rx_fifo_reset = 1;
    i2s->conf.rx_fifo_reset = 0;
    i2s->conf.tx_fifo_reset = 1;
    i2s->conf.tx_fifo_reset = 0;
}

// FastLED#3526 Phase 2b step B — DMA-done ISR handler.
//
// Runs in ISR context. `arg` is the peripheral instance. On `out_eof`
// (end-of-frame — DMA finished draining the descriptor), clear the
// interrupt, mark the peripheral not-busy, and fire the registered
// completion callback (which routes to the channel-engine trampoline).
FL_IRAM void i2s_dma_isr_trampoline(void* arg) FL_NO_EXCEPT;

}  // anonymous namespace

//=============================================================================
// Ctor / dtor
//=============================================================================

I2sPeripheralEsp32DevEsp::I2sPeripheralEsp32DevEsp() FL_NO_EXCEPT
    : mConfig(),
      mInitialized(false),
      mBusy(false),
      mCallback(nullptr),
      mCallbackUserCtx(nullptr),
      mDescriptors(nullptr),
      mDescriptorCapacity(0),
      mIsrHandle(nullptr),
      mLaneRoutedPin{{-1, -1, -1, -1, -1, -1, -1, -1,
                      -1, -1, -1, -1, -1, -1, -1, -1,
                      -1, -1, -1, -1, -1, -1, -1, -1}} {}

I2sPeripheralEsp32DevEsp::~I2sPeripheralEsp32DevEsp() {
    if (mInitialized) {
        deinitialize();
    }
}

//=============================================================================
// Lifecycle
//=============================================================================

bool I2sPeripheralEsp32DevEsp::initialize(
    const I2sEsp32DevPeripheralConfig &cfg) FL_NO_EXCEPT {
    if (mInitialized) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: already initialized");
        return false;
    }
    mConfig = cfg;
    const int i2s_device = static_cast<int>(mConfig.mI2sPort);

    // FastLED#3576 Phase 1 — cross-driver ownership: I2S0 is contended
    // between the second clockless bank and the clocked-SPI driver
    // (`I2sSpiPeripheralEsp`). First claim wins; released in
    // deinitialize(). The per-instance `mInitialized` flag alone can't
    // see the other driver class.
    if (!i2sPortClaim(i2s_device, kI2sClocklessOwner)) {
        return false;
    }

    // Power-gate + reset the peripheral via the IDF-version-safe wrapper
    // (routes to `periph_module_enable(PERIPH_I2S{n}_MODULE)` on IDF
    // 4.x/5.x, `i2s_ll_enable_bus_clock` + `i2s_ll_reset_register` on
    // IDF 6.x+). Not a Yves helper — lives in `i2s_periph_compat.h`.
    fl::fl_i2s_periph_enable(i2s_device);

    // Select I2S0 vs I2S1 SoC register block. The modern engine uses
    // I2S1 by default (mConfig.mI2sPort == 1); support I2S0 for tests
    // that construct the peripheral against port 0.
    i2s_dev_t* const i2s = (i2s_device == 0) ? &I2S0 : &I2S1;

    // Reset everything before configuring. Order matters: peripheral
    // reset first (clears any prior state), then DMA and FIFO subsystem
    // reset. Bit-identical to Yves's `i2s_init` prelude.
    reset_i2s_registers(i2s);
    reset_i2s_dma(i2s);
    reset_i2s_fifo(i2s);

    // Main configuration — Yves baseline (known-boot-clean).
    i2s->conf.tx_msb_right = 1;
    i2s->conf.tx_mono = 0;
    i2s->conf.tx_short_sync = 0;
    i2s->conf.tx_msb_shift = 0;
    i2s->conf.tx_right_first = 1;
    i2s->conf.tx_slave_mod = 0;

    // Enable LCD/parallel mode. `lcd_tx_wrx2_en = 0` selects 16- or
    // 32-bit parallel output width (vs the 8-bit-with-2x-write mode).
    i2s->conf2.val = 0;
    i2s->conf2.lcd_en = 1;
    i2s->conf2.lcd_tx_wrx2_en = 0;
    i2s->conf2.lcd_tx_sdx2_en = 0;

    // Sample-rate configuration — `tx_bits_mod = 32` (Yves's proven
    // value): one 32-bit sample per pixel clock, with DATA_OUT(n)
    // presenting sample bit n+8. The engine feeds this format via
    // `wave8I2s1ExpandTo32Samples()` (one u32 per pulse, lanes at bits
    // 8..23) — see FastLED#3569 for the full mapping derivation.
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 32;
    i2s->sample_rate_conf.tx_bck_div_num = 1;

    // Main clock: 80 MHz APB / (N + B/A) = pixel clock. Variable-rate
    // per `mConfig.mPixelClockHz` — supports wave8 at 8 MHz (WS2812
    // 800 kHz × 8 pulses/bit), 3.2 MHz (WS2811 400 kHz × 8), 2.4 MHz
    // wave3, or any chipset-specific rate the engine picks. Falls back
    // to 8 MHz if `mPixelClockHz` is zero.
    int div_N, div_A, div_B;
    solveI2sClockDivider(mConfig.mPixelClockHz != 0 ? mConfig.mPixelClockHz : 8000000u,
                          &div_N, &div_A, &div_B);
    i2s->clkm_conf.val = 0;
    i2s->clkm_conf.clka_en = 0;
    i2s->clkm_conf.clkm_div_a = div_A;
    i2s->clkm_conf.clkm_div_b = div_B;
    i2s->clkm_conf.clkm_div_num = div_N;

    // FIFO in DMA-serviced mode — Yves baseline (32-bit single-channel)
    // restored after `= 1` triggered spurious DMA interrupts.
    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 3;
    i2s->fifo_conf.tx_data_num = 32;
    i2s->fifo_conf.dscr_en = 1;

    // Bypass the on-chip PCM formatting logic (we're delivering
    // already-encoded pulse bytes; no PCM sample-rate conversion).
    //
    // `tx_stop_en = 1` (deviation from Yves's 0): auto-stop the TX unit
    // when the FIFO drains. Yves streamed a circular descriptor ring
    // that fed zeros forever, so his TX never underran; our one-shot
    // chain ends, and with tx_stop_en=0 the TX unit keeps clocking
    // stale FIFO contents onto every routed DATA_OUT pin after the
    // frame — spurious pulses during the WS28xx latch window. The last
    // real sample of a wave8 frame ends low, so the auto-stopped line
    // idles low as required. (Stopping via `tx_start = 0` in the EOF
    // ISR instead would truncate the wire tail: OUT_EOF fires when the
    // DMA drains the descriptor into the FIFO, up to 64 samples before
    // they reach the pins.)
    i2s->conf1.val = 0;
    // tx_stop_en stays 0 (Yves baseline): bench-tested `= 1` produced
    // ZERO wire output — the FIFO is empty at the instant `tx_start`
    // is set (DMA prefetch hasn't landed yet), so the auto-stop latches
    // before the first sample ever ships. Post-frame underrun clocking
    // is handled by stopping TX from the completion path instead.
    i2s->conf1.tx_stop_en = 0;
    i2s->conf1.tx_pcm_bypass = 1;

    // Mono channel mode routing everything to the right channel path
    // (paired with `tx_msb_right = 1` above).
    i2s->conf_chan.val = 0;
    i2s->conf_chan.tx_chan_mod = 1;

    i2s->timing.val = 0;

    // FastLED#3569 interrupt hygiene — keep ALL I2S interrupt sources
    // masked and any stale raw flags cleared BEFORE `esp_intr_alloc`
    // (which enables the CPU interrupt immediately). `transmit()` arms
    // `out_eof` after the descriptor chain is set up; nothing should be
    // able to fire between here and the first kick.
    i2s->int_ena.val = 0;
    i2s->int_clr.val = i2s->int_raw.val;

    const int interrupt_source =
        (i2s_device == 0) ? ETS_I2S0_INTR_SOURCE : ETS_I2S1_INTR_SOURCE;
    // FastLED#3568 BUG #4 fix — flags = 0 matches Yves. IRAM-safe ISR
    // requires all called code to be in IRAM; our trampoline calls
    // finishTransmitFromIsr which dispatches through a function pointer
    // that may target flash-resident code, so `ESP_INTR_FLAG_IRAM` was
    // over-committing safety we don't uphold. Yves's driver runs in
    // normal-cache-required mode.
    esp_err_t err = esp_intr_alloc(interrupt_source, 0,
                                   &i2s_dma_isr_trampoline, this, &mIsrHandle);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: esp_intr_alloc failed err=%s", static_cast<int>(err));
        i2sPortRelease(i2s_device, kI2sClocklessOwner);
        return false;
    }

    mInitialized = true;
    mBusy = false;
    return true;
}

void I2sPeripheralEsp32DevEsp::deinitialize() FL_NO_EXCEPT {
    if (!mInitialized) {
        return;
    }
    const int i2s_device = static_cast<int>(mConfig.mI2sPort);
    i2s_dev_t* const i2s = (i2s_device == 0) ? &I2S0 : &I2S1;

    // Stop the peripheral first so DMA doesn't drift into the next
    // caller's buffer while we tear down. Then disable + free the ISR
    // handle. Then free the descriptor.
    if (mBusy) {
        reset_i2s_registers(i2s);
        i2s->conf.rx_start = 0;
        i2s->conf.tx_start = 0;
    }
    i2s->int_ena.val = 0;
    if (mIsrHandle) {
        (void)esp_intr_disable(mIsrHandle);
        (void)esp_intr_free(mIsrHandle);
        mIsrHandle = nullptr;
    }
    if (mDescriptors) {
        heap_caps_free(mDescriptors);
        mDescriptors = nullptr;
        mDescriptorCapacity = 0;
    }
    i2sPortRelease(i2s_device, kI2sClocklessOwner);
    mInitialized = false;
    mBusy = false;
}

bool I2sPeripheralEsp32DevEsp::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer management
//=============================================================================

u8 *I2sPeripheralEsp32DevEsp::allocateBuffer(size_t size_bytes) FL_NO_EXCEPT {
    if (size_bytes == 0) {
        return nullptr;
    }
    // DMA-capable DRAM. `MALLOC_CAP_DMA` returns 4-byte-aligned
    // blocks by default, which is enough for the I2S FIFO's 32-bit
    // read granularity once Stage 5 turns on the DMA descriptor path.
    void *p = heap_caps_malloc(size_bytes, MALLOC_CAP_DMA);
    return static_cast<u8 *>(p);
}

void I2sPeripheralEsp32DevEsp::freeBuffer(u8 *buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool I2sPeripheralEsp32DevEsp::transmit(const u8 *buffer,
                                        size_t size_bytes) FL_NO_EXCEPT {
    if (!mInitialized || buffer == nullptr || size_bytes == 0) {
        return false;
    }
    if (mBusy) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: transmit while busy");
        return false;
    }

    const int i2s_device = static_cast<int>(mConfig.mI2sPort);
    i2s_dev_t* const i2s = (i2s_device == 0) ? &I2S0 : &I2S1;

    // FastLED#3569 — build a linear descriptor chain over the caller's
    // DMA-capable buffer. Each lldesc reaches at most 4092 bytes
    // (12-bit length field); a 32-bit-sample wave8 frame is
    // `bytes_per_lane * 256` bytes, so real frames need several links.
    // Chunk at 4088 (largest 4-byte multiple ≤ 4092) so every link
    // starts on a 32-bit sample boundary. `eof = 1` only on the LAST
    // link so `out_eof` fires exactly once per frame; the chain ends
    // with `stqe_next = nullptr` so the DMA fetch stops after the
    // final link drains.
    constexpr size_t kChunk = 4088;
    const size_t desc_count = (size_bytes + kChunk - 1) / kChunk;
    if (desc_count > mDescriptorCapacity) {
        lldesc_t* grown = static_cast<lldesc_t*>(
            heap_caps_malloc(desc_count * sizeof(lldesc_t), MALLOC_CAP_DMA));
        if (!grown) {
            FL_WARN_F("I2sPeripheralEsp32DevEsp: descriptor chain alloc failed (%s links)",
                      static_cast<int>(desc_count));
            return false;
        }
        if (mDescriptors) {
            heap_caps_free(mDescriptors);
        }
        mDescriptors = grown;
        mDescriptorCapacity = desc_count;
    }
    size_t remaining = size_bytes;
    const u8* chunk_ptr = buffer;
    for (size_t i = 0; i < desc_count; ++i) {
        lldesc_t& d = mDescriptors[i];
        const size_t chunk_len = remaining > kChunk ? kChunk : remaining;
        d.buf = const_cast<u8*>(chunk_ptr);
        d.size = chunk_len;
        d.length = chunk_len;
        d.owner = 1;
        d.sosf = (i == 0) ? 1 : 0;
        d.eof = (i == desc_count - 1) ? 1 : 0;
        d.offset = 0;
        d.empty = 0;
        d.qe.stqe_next = (i == desc_count - 1) ? nullptr : &mDescriptors[i + 1];
        chunk_ptr += chunk_len;
        remaining -= chunk_len;
    }

    // Register kick order (deliberate deviation from Yves's i2s_start):
    //   reset → BURST → int_clr → int_ena(eof|dscr_err) →
    //   esp_intr_enable → out_link.addr/start → conf.tx_start
    //
    // Two ordering bugs in the previous (Yves-shaped) sequence:
    //  1. `int_clr` ran AFTER `out_link.start` — a frame small enough to
    //     drain entirely into the 64-word TX FIFO raised OUT_EOF in that
    //     window and the clear wiped it, so the ISR never fired and the
    //     frame hung until waitTransmitDone() timed out.
    //  2. `int_ena.val = 0` ran after `int_ena.out_dscr_err = 1`,
    //     silently disarming the descriptor-error interrupt every frame.
    // Arming interrupts before out_link.start is safe: the reset +
    // int_clr just above guarantee no stale raw flags can fire early.
    // It also means a failed esp_intr_enable() returns before ANY DMA
    // state is armed (the old order left the out-link fetching from the
    // scratch buffer on that error path).
    reset_i2s_registers(i2s);
    i2s->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
    i2s->int_clr.val = i2s->int_raw.val;
    i2s->int_ena.val = 0;
    i2s->int_ena.out_eof = 1;
    i2s->int_ena.out_dscr_err = 1;

    esp_err_t err = esp_intr_enable(mIsrHandle);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: esp_intr_enable failed err=%s", static_cast<int>(err));
        i2s->int_ena.val = 0;
        return false;
    }
    i2s->out_link.addr = reinterpret_cast<u32>(&mDescriptors[0]); // ok reinterpret cast - DMA hardware register expects the physical address of the lldesc as a u32
    i2s->out_link.start = 1;

    mBusy = true;
    i2s->conf.tx_start = 1;
    return true;
}

bool I2sPeripheralEsp32DevEsp::waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT {
    // Spin-wait for the ISR to clear `mBusy`. Elapsed-time computation
    // uses the FreeRTOS-standard wraparound-safe pattern
    // `(TickType_t)(now - start)` so a 32-bit tick-counter wrap doesn't
    // trip a spurious immediate timeout or an unbounded wait after ~49
    // days of uptime.
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    while (mBusy) {
        if (static_cast<TickType_t>(xTaskGetTickCount() - start) >= timeout_ticks) {
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}

bool I2sPeripheralEsp32DevEsp::isBusy() const FL_NO_EXCEPT { return mBusy; }

//=============================================================================
// Callback registration
//=============================================================================

bool I2sPeripheralEsp32DevEsp::registerTransmitCallback(
    I2sEsp32DevTxDoneCallback cb, void *user_ctx) FL_NO_EXCEPT {
    mCallback = cb;
    mCallbackUserCtx = user_ctx;
    return true;
}

//=============================================================================
// Lane -> GPIO routing (Phase 2b step C)
//=============================================================================

bool I2sPeripheralEsp32DevEsp::routeLanePin(u8 lane, i32 gpio_pin) FL_NO_EXCEPT {
    if (!mInitialized) {
        return false;
    }
    if (lane >= kMaxI2sLanes) {
        return false;
    }
    const int i2s_device = static_cast<int>(mConfig.mI2sPort);

    // Compute the SoC signal index for this lane. Signal indices are
    // contiguous within each I2S block (I2S{n}O_DATA_OUT0_IDX ..
    // I2S{n}O_DATA_OUT23_IDX) — one add per lane covers the range.
    //
    // I2S0 quirk (FastLED#3576 Phase 1 bench): in the same 32-bit mono
    // LCD config, I2S0 presents the emitted half-word on DATA_OUT8..23
    // (one byte higher than I2S1's DATA_OUT0..15) — lane n therefore
    // routes signal OUT(8+n) on port 0 and OUT(n) on port 1.
    const int base_signal = (i2s_device == 0)
        ? static_cast<int>(I2S0O_DATA_OUT0_IDX) + 8
        : static_cast<int>(I2S1O_DATA_OUT0_IDX);

    if (gpio_pin < 0) {
        // Negative pin = clear routing. Detach the previously-routed pin
        // (if any) by routing SIG_GPIO_OUT_IDX in its place, so the pin
        // stops driving I2S data and returns to plain-GPIO control.
        const i32 prev = mLaneRoutedPin[lane];
        if (prev >= 0) {
            esp_rom_gpio_connect_out_signal(static_cast<u32>(prev),
                                             SIG_GPIO_OUT_IDX,
                                             false, false);
            mLaneRoutedPin[lane] = -1;
        }
        return true;
    }

    // Select the pad's GPIO function first — pins that boot into an
    // alternate function (e.g. SD_CMD on GPIO11 in some board configs)
    // must be switched to plain GPIO before the matrix routing takes
    // effect. `esp_rom_gpio_pad_select_gpio` is the IDF v5-safe API.
    esp_rom_gpio_pad_select_gpio(static_cast<u8>(gpio_pin));

    // Put the pin into output mode. `gpio_set_direction` accepts a
    // `gpio_num_t` enum; the raw int cast is safe for the classic
    // ESP32's 0..39 pin range.
    esp_err_t err = gpio_set_direction(static_cast<gpio_num_t>(gpio_pin),
                                        GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: gpio_set_direction failed pin=%s err=%s",
                  static_cast<int>(gpio_pin), static_cast<int>(err));
        return false;
    }

    // Route the DATA_OUT{lane} signal through the GPIO matrix to the
    // target pin. Args: gpio_num, signal_idx, out_inv, oen_inv. We honor
    // the per-strip invert mask from the peripheral config so chipsets
    // that want polarity inversion get it "for free" via the matrix.
    const bool invert = (mConfig.mInvertMask & (1u << lane)) != 0;
    esp_rom_gpio_connect_out_signal(static_cast<u32>(gpio_pin),
                                     static_cast<u32>(base_signal + lane),
                                     invert, false);
    mLaneRoutedPin[lane] = gpio_pin;
    return true;
}

//=============================================================================
// Introspection
//=============================================================================

const I2sEsp32DevPeripheralConfig &
I2sPeripheralEsp32DevEsp::getConfig() const FL_NO_EXCEPT {
    return mConfig;
}

//=============================================================================
// ISR trampoline
//=============================================================================

namespace {

FL_IRAM void i2s_dma_isr_trampoline(void* arg) FL_NO_EXCEPT {
    auto* self = static_cast<I2sPeripheralEsp32DevEsp*>(arg);
    if (!self) {
        return;
    }
    // Read port via the IRAM-safe accessor — do NOT go through
    // `getConfig()` which may not be inlined into IRAM under -Og debug
    // builds. `i2sPortForIsr()` reads the u8 member field directly.
    const int i2s_device = static_cast<int>(self->i2sPortForIsr());
    i2s_dev_t* const i2s = (i2s_device == 0) ? &I2S0 : &I2S1;

    // FastLED#3568 — disable ALL I2S1 interrupts BEFORE anything else
    // to prevent ISR flooding if the DMA state (null stqe_next + wrong
    // FIFO mode combo) is firing dscr_err or out_eof repeatedly. The
    // next `transmit()` re-arms `int_ena.out_eof = 1` and
    // `int_ena.out_dscr_err = 1` after setting up the new descriptor.
    const u32 int_state = i2s->int_st.val;
    i2s->int_ena.val = 0;
    i2s->int_clr.val = int_state;

    // Only fire the completion callback on real end-of-frame. Ignore
    // errors (dscr_err) for now — engine will notice via
    // `waitTransmitDone` timeout.
    if (int_state & I2S_OUT_EOF_INT_ST_M) {
        self->finishTransmitFromIsr();
    }
}

}  // anonymous namespace

// FastLED#3526 Phase 2b step B — ISR-context completion. Declared with a
// public linker name so the trampoline (in the anonymous namespace) can
// call it. Marked FL_IRAM so it stays resident during flash reads.
FL_IRAM void I2sPeripheralEsp32DevEsp::finishTransmitFromIsr() FL_NO_EXCEPT {
    mBusy = false;
    if (mCallback != nullptr) {
        (*mCallback)(mCallbackUserCtx);
    }
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
