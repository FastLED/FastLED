// IWYU pragma: private

// FastLED#3512 Phase 5: this file is now part of the i2s unity build.
// The earlier UNITY_BUILD_EXCLUDE marker cited a `driver/gpio.h` collision
// against the restored classic Yves driver; on re-investigation the
// actual `ADC: CONFLICT!` issue was `driver/i2s.h` in the Stage 2
// (#3476) impl. This Stage 4 file drops `driver/i2s.h` entirely (see
// the docblock below) so the conflict no longer applies. The stub
// `transmit()` currently does no DMA — Phase 2 replaces it with real
// register + DMA + ISR machinery reused from the classic driver.

/// @file i2s_peripheral_esp32dev_esp.cpp.hpp
/// @brief Real-hardware `II2sPeripheralEsp32Dev` impl — Stage 4 rewrite
///        avoids IDF's `driver/i2s.h` to escape the `driver_ng` vs
///        legacy-ADC conflict (#3474).
///
/// ## Why not `driver/i2s.h`
///
/// Stage 2 (#3476) used the legacy `driver/i2s.h`. Wiring that TU
/// into the classic-ESP32 unity build tripped a hard `ADC:
/// CONFLICT! driver_ng is not allowed to be used with the legacy
/// driver` panic at IDF init time — the include drags in the
/// `driver_ng` framework, and something in the link (arduino-esp32
/// 3.3.7 core, or FastLED's `pin_esp32_native_impl.hpp` on the same
/// codepath) had already registered the legacy ADC driver.
///
/// Stage 4 dodges the ecosystem conflict by dropping the
/// `driver/i2s.h` include entirely. Only very-low-level headers
/// remain:
///
///   - `esp_heap_caps.h` — DMA-capable buffer alloc.
///   - `driver/gpio.h` — GPIO enable (no ADC interaction).
///
/// These are the same low-level headers that AutoResearch and every
/// other classic-ESP32 driver in the tree use without triggering the
/// conflict. The peripheral wraps this narrow surface so the engine
/// can drive it exactly the way the mock does.
///
/// ## Scope of the Stage 4 v1 impl
///
/// Sufficient to satisfy "peripheral wired end-to-end" and pass the
/// engine tests on hardware:
///
/// - `initialize()` accepts the config, stashes it, sets the
///   initialized flag.
/// - `allocateBuffer()` / `freeBuffer()` back onto DMA-capable heap.
/// - `transmit()` accepts the byte buffer and fires the registered
///   callback synchronously (mock-equivalent semantics — the engine
///   walks through BUSY → poll → READY the same way).
/// - `isBusy()` / `waitTransmitDone()` reflect the ready state.
///
/// The full DMA descriptor chain, ISR install, register-level
/// clock/pin setup on `I2S1`, and the real WS2812 waveform emit are
/// Stage 5. The peripheral surface is stable across that upgrade —
/// only this file's bodies change.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.h"
#include "platforms/esp/32/drivers/i2s/i2s_periph_compat.h"  // fl_i2s_periph_enable — IDF-version-safe power gate

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

// FastLED#3526 Phase 2b — modern peripheral owns I2S1 for the duration of
// its `initialize()`/`deinitialize()` window. This static-storage flag is
// the ownership handshake between concurrent construction attempts (e.g.,
// test doubles + a real peripheral in the same TU). Yves classic driver
// tracks its own separate flag (`gInitializedI2sInitialized`) — during the
// #3526 rollout only one path is bound at any time (Yves via
// `-DFASTLED_ESP32_I2S=1` opt-in, modern via `Bus::FLEX_IO, 0` after Phase
// 2d), so there is no cross-driver race here. Yves is deleted at Phase 2e
// and this flag becomes the sole record.
bool g_i2s1_hardware_claimed_by_modern = false;

// Wave8 pixel clock: 80 MHz APB / (10 + 0/1) = 8 MHz — 125 ns per pulse.
// Fixed rate regardless of chipset (per the meta-issue directive: modern
// driver is a general clockless like PARLIO, not per-chipset like Yves).
// Wave3-eligible chipsets will re-program N/A/B in a follow-up when the
// engine plumbs per-channel timing selection into the peripheral.
constexpr int kWave8ClockDividerN = 10;
constexpr int kWave8ClockDividerA = 1;
constexpr int kWave8ClockDividerB = 0;

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
      mDescriptor(nullptr),
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
    // FastLED#3526 Phase 2b: modern-driver ownership handshake — refuse
    // if another modern peripheral instance is already bound. The Yves
    // classic driver tracks its own separate flag; during rollout only
    // one binding path is active at a time (Yves via
    // `-DFASTLED_ESP32_I2S=1`, modern via `Bus::FLEX_IO, 0` after Phase
    // 2d), so we do not check Yves's flag here — that would drag the
    // Yves TU into modern's link footprint, violating the meta-issue's
    // "No calls into `i2s_esp32dev.cpp.hpp`" directive.
    if (g_i2s1_hardware_claimed_by_modern) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: I2S1 already claimed by another modern instance");
        return false;
    }
    mConfig = cfg;
    const int i2s_device = static_cast<int>(mConfig.mI2sPort);

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

    // Main configuration — parallel-out mode with LSB-first byte order
    // in each 32-bit sample. `tx_right_first = 1` puts channel-right
    // data first, matching how wave8 encoded bytes are laid out in the
    // DMA buffer. Same bit-map Yves proved works for classic ESP32 I2S
    // parallel output.
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

    // Sample-rate configuration. `tx_bits_mod = 32` puts the peripheral
    // in 32-bit-parallel mode (16 lanes × 2 bytes packed per word).
    // `tx_bck_div_num = 1` is the divider on the sample clock feeding
    // the bit clock — kept at 1 for wave8's 8 MHz rate.
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 32;
    i2s->sample_rate_conf.tx_bck_div_num = 1;

    // Main clock: 80 MHz APB / (N + B/A) = wave8's 8 MHz pixel clock.
    // Fixed-rate — no per-chipset clock math. Wave3 mode will re-program
    // these when per-channel timing plumbs through (follow-up).
    i2s->clkm_conf.val = 0;
    i2s->clkm_conf.clka_en = 0;
    i2s->clkm_conf.clkm_div_a = kWave8ClockDividerA;
    i2s->clkm_conf.clkm_div_b = kWave8ClockDividerB;
    i2s->clkm_conf.clkm_div_num = kWave8ClockDividerN;

    // FIFO in DMA-serviced mode. `tx_fifo_mod = 3` selects the 32-bit
    // single-channel data path (each DMA word is one output sample).
    // `dscr_en = 1` routes the FIFO through the DMA descriptor chain.
    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 3;
    i2s->fifo_conf.tx_data_num = 32;
    i2s->fifo_conf.dscr_en = 1;

    // Bypass the on-chip PCM formatting logic (we're delivering
    // already-encoded pulse bytes; no PCM sample-rate conversion).
    i2s->conf1.val = 0;
    i2s->conf1.tx_stop_en = 0;
    i2s->conf1.tx_pcm_bypass = 1;

    // Mono channel mode routing everything to the right channel path
    // (paired with `tx_msb_right = 1` above).
    i2s->conf_chan.val = 0;
    i2s->conf_chan.tx_chan_mod = 1;

    i2s->timing.val = 0;

    // FastLED#3526 Phase 2b step B — allocate one DMA-capable descriptor
    // for single-shot transmit. Streaming (multi-descriptor ring) is a
    // follow-up when the engine can produce buffers larger than one
    // descriptor's 4 KB reach.
    mDescriptor = static_cast<lldesc_t*>(heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA));
    if (!mDescriptor) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: descriptor alloc failed");
        return false;
    }
    mDescriptor->owner = 1;
    mDescriptor->sosf = 1;
    mDescriptor->eof = 1;
    mDescriptor->offset = 0;
    mDescriptor->empty = 0;
    mDescriptor->buf = nullptr;
    mDescriptor->size = 0;
    mDescriptor->length = 0;
    mDescriptor->qe.stqe_next = nullptr;

    // Enable OUT_EOF interrupt bit so the DMA-done ISR fires at the end
    // of each descriptor. `int_ena.out_eof = 1` on the peripheral, then
    // install our ISR handler via `esp_intr_alloc`.
    i2s->int_ena.val = 0;
    i2s->int_ena.out_eof = 1;

    const int interrupt_source =
        (i2s_device == 0) ? ETS_I2S0_INTR_SOURCE : ETS_I2S1_INTR_SOURCE;
    esp_err_t err = esp_intr_alloc(interrupt_source, ESP_INTR_FLAG_IRAM,
                                   &i2s_dma_isr_trampoline, this, &mIsrHandle);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: esp_intr_alloc failed err=%s", static_cast<int>(err));
        heap_caps_free(mDescriptor);
        mDescriptor = nullptr;
        return false;
    }

    g_i2s1_hardware_claimed_by_modern = true;
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
    if (mDescriptor) {
        heap_caps_free(mDescriptor);
        mDescriptor = nullptr;
    }
    g_i2s1_hardware_claimed_by_modern = false;
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
    // Descriptor field `length`/`size` are 12-bit — the DMA can carry
    // 4092 bytes max per descriptor (12-bit field caps at 4095, but 4092
    // is the practical alignment-safe ceiling used across ESP32 DMA
    // drivers). Anything larger needs a chain — streaming refill from
    // the ISR is a Phase 2b step D follow-up.
    if (size_bytes > 4092) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: buffer too large for single descriptor (%s > 4092)", static_cast<int>(size_bytes));
        return false;
    }

    const int i2s_device = static_cast<int>(mConfig.mI2sPort);
    i2s_dev_t* const i2s = (i2s_device == 0) ? &I2S0 : &I2S1;

    // Point the descriptor at the caller's DMA-capable buffer. Ownership
    // stays with the DMA engine until it finishes (owner=1). `sosf`+`eof`
    // = single-shot, one-descriptor frame. `qe.stqe_next = nullptr` ends
    // the chain (no streaming refill in this step).
    mDescriptor->buf = const_cast<u8*>(buffer);
    mDescriptor->size = size_bytes;
    mDescriptor->length = size_bytes;
    mDescriptor->owner = 1;
    mDescriptor->sosf = 1;
    mDescriptor->eof = 1;
    mDescriptor->offset = 0;
    mDescriptor->empty = 0;
    mDescriptor->qe.stqe_next = nullptr;

    // Reset the peripheral so no stale DMA state carries over from a
    // prior kick, then arm the output link with our descriptor address
    // and start the DMA. The ISR will fire when the descriptor drains.
    reset_i2s_registers(i2s);
    i2s->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
    i2s->int_clr.val = i2s->int_raw.val;
    i2s->out_link.addr = reinterpret_cast<u32>(mDescriptor); // ok reinterpret cast - DMA hardware register expects the physical address of the lldesc as a u32
    i2s->out_link.start = 1;

    esp_err_t err = esp_intr_enable(mIsrHandle);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: esp_intr_enable failed err=%s", static_cast<int>(err));
        return false;
    }

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
    const int base_signal = (i2s_device == 0)
        ? static_cast<int>(I2S0O_DATA_OUT0_IDX)
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

    // Only respond to OUT_EOF. Other flags (out_dscr_err, etc.) are not
    // enabled in `int_ena` so shouldn't fire, but guard defensively.
    if (i2s->int_st.out_eof) {
        i2s->int_clr.val = i2s->int_raw.val;
        i2s->conf.tx_start = 0;

        // Fire user callback via the peripheral's registered slot.
        // Callback is registered via `registerTransmitCallback` before
        // any transmit is issued; the engine's trampoline lives in
        // channel_engine_i2s_esp32dev and just flips `mTransmitCompleted`.
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
