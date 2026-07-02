// IWYU pragma: private

/// @file i2s_rx_sampler.cpp.hpp
/// @brief Impl of the classic-ESP32 I2S-RX 1-bit oversampling capture
///        backend (FastLED#3576 Phase 3). See i2s_rx_sampler.h.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s_rx/i2s_rx_sampler.h"

#include "platforms/esp/32/drivers/i2s/i2s_periph_compat.h"
#include "platforms/esp/32/drivers/i2s/i2s_port_claim.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_rom_gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

namespace {

// Sample clock. 16 MHz → 62.5 ns resolution; comfortably inside the
// ±150 ns 4-phase decode windows and only 2 MB/s of DMA traffic.
// TRUE sample rate = clkm = 80 MHz / clkm_div_num — bench-derived: the
// RX serial sampler clocks SD at the clkm rate directly; rx_bck_div
// only shapes the (unused) BCK output. 80/5 = 16 MHz → 62.5 ns per
// sample. Rates above ~16 MHz (N < 5) capture only zeros from real
// pads — the input path tops out, so N = 5 is the practical maximum.
constexpr u32 kSampleRateHz = 16000000u;
constexpr u32 kNsPerSample = 1000000000u / kSampleRateHz; // 62 (62.5 truncated; see flushCurrentRun)

// DMA ring: 16 descriptors × 1024 bytes = 16 KB — at 2 MB/s that is
// 8 ms of buffering, enough to ride out task-scheduling stalls in the
// draining wait() loop (bench: 8 KB showed mid-frame sample gaps).
constexpr size_t kRxDescCount = 16;
constexpr size_t kRxDescBytes = 1024;

// Owner tag for the cross-driver port-claim registry.
constexpr const char *kI2sRxOwner = "I2S_RX";

// Hard cap on stored edges (level runs): 8000 packed runs = 32 KB ≈
// 165 WS2812 LEDs — sized against classic ESP32's free heap (bench:
// unbounded storage OOM'd at 500 LEDs). Still 16× the RMT backend's
// one-shot 10-LED ceiling.
constexpr size_t kMaxEdges = 8000;

/// @brief One captured level run, packed: bit0 = level, bits1..31 =
/// duration in ns (caps at ~2.1 s, far beyond any LED frame element).
struct SampleRun {
    u32 packed;
    u32 duration_ns() const FL_NO_EXCEPT { return packed >> 1; }
    u8 level() const FL_NO_EXCEPT { return static_cast<u8>(packed & 1u); }
    static SampleRun make(u32 dur_ns, u8 lvl) FL_NO_EXCEPT {
        return SampleRun{(dur_ns << 1) | (lvl & 1u)};
    }
};

} // anonymous namespace

// Bench instrumentation (FastLED#3576 Phase 3 bring-up): peekMem-able
// counters — external linkage so the symbol lands in the ELF map.
// [0]=descriptors drained, [1]=words processed, [2]=one-bits counted,
// [3]=first nonzero word value, [4]=runs stored, [5]=begin() progress
// marker, [6]=timeouts, [7]=finishes.
volatile u32 g_i2s_rx_debug[8] = {0, 0, 0, 0, 0, 0, 0, 0};

namespace {

class I2sRxSampler final : public RxDevice {
  public:
    explicit I2sRxSampler(int pin) FL_NO_EXCEPT : mPin(pin) {}

    ~I2sRxSampler() override { teardown(); }

    bool begin(const RxConfig &config) FL_NO_EXCEPT override {
        mSignalRangeMaxNs = config.signal_range_max_ns;
        mRuns.clear();
        mFinished = false;
        mSawFirstEdge = false;
        mCurLevel = 0;
        mCurRunSamples = 0;
        mSubNs = 0;

        // I2S0 is shared with the second clockless TX bank and the
        // clocked-SPI driver — claim it for the capture window.
        if (!i2sPortClaim(0, kI2sRxOwner)) {
            return false;
        }

        fl::fl_i2s_periph_enable(0);
        i2s_dev_t *i2s = &I2S0;

        // Full reset (same sequence the TX peripheral uses).
        const u32 lc_reset =
            I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
        i2s->lc_conf.val |= lc_reset;
        i2s->lc_conf.val &= ~lc_reset;
        const u32 conf_reset = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M |
                               I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
        i2s->conf.val |= conf_reset;
        i2s->conf.val &= ~conf_reset;

        // Standard serial master RX. No LCD, no camera. Philips shift
        // ON — bench-derived: with msb_shift=0 the RX unit stored only
        // zeros; the IDF classic RX driver's proven config uses the
        // Philips frame (one-BCK WS shift) and real pin data flows.
        i2s->conf.val = 0;
        i2s->conf.rx_slave_mod = 0;
        i2s->conf.rx_msb_shift = 1;
        i2s->conf.rx_short_sync = 0;
        i2s->conf.rx_mono = 0;
        i2s->conf.rx_msb_right = 0;
        i2s->conf.rx_right_first = 0;
        i2s->conf2.val = 0;

        // 16-bit channel slots; BCK = clkm / rx_bck_div. Target
        // BCK = kSampleRateHz: 80 MHz / 2 = 40 MHz (integer), /2 = 20 MHz.
        i2s->sample_rate_conf.val = 0;
        i2s->sample_rate_conf.rx_bits_mod = 16;
        i2s->sample_rate_conf.rx_bck_div_num = 2;
        i2s->clkm_conf.val = 0;
        i2s->clkm_conf.clka_en = 0;
        i2s->clkm_conf.clkm_div_num = 5;   // 80/5 = 16 MHz (integer)
        i2s->clkm_conf.clkm_div_b = 0;
        i2s->clkm_conf.clkm_div_a = 1;

        // FIFO: 16-bit DUAL channel so every BCK-sampled bit is stored
        // (single-channel modes drop the other channel's slots, which
        // would punch holes in the sample stream).
        i2s->fifo_conf.val = 0;
        i2s->fifo_conf.rx_fifo_mod_force_en = 1;
        i2s->fifo_conf.rx_fifo_mod = 0;
        i2s->fifo_conf.rx_data_num = 32;
        i2s->fifo_conf.dscr_en = 1;

        i2s->conf_chan.val = 0;
        i2s->conf_chan.rx_chan_mod = 0;

        i2s->conf1.val = 0;
        i2s->conf1.rx_pcm_bypass = 1;

        // The PDM demodulator sits in front of the serial RX path on
        // classic ESP32 and its reset default is NOT all-clear — with
        // PDM-RX engaged the plain serial samples never reach the FIFO
        // (bench-observed as an all-zero capture). Force PDM off.
        i2s->pdm_conf.rx_pdm_en = 0;
        i2s->pdm_conf.tx_pdm_en = 0;
        i2s->pdm_conf.pcm2pdm_conv_en = 0;
        i2s->pdm_conf.pdm2pcm_conv_en = 0;

        i2s->timing.val = 0;
        i2s->int_ena.val = 0;
        i2s->int_clr.val = i2s->int_raw.val;

        if (!allocRing()) {
            i2sPortRelease(0, kI2sRxOwner);
            return false;
        }

        // Route the pin into the serial data-in signal. Classic-ESP32
        // quirk: SD-in rides the DATA_IN15 matrix signal.
        esp_rom_gpio_pad_select_gpio(static_cast<u8>(mPin));
        (void)gpio_set_direction(static_cast<gpio_num_t>(mPin), GPIO_MODE_INPUT);
        esp_rom_gpio_connect_in_signal(static_cast<u32>(mPin),
                                       I2S0I_DATA_IN15_IDX, false);

        // Arm the RX DMA and start sampling.
        i2s->rx_eof_num = kRxDescBytes / 4; // words per in_suc_eof
        i2s->in_link.addr = reinterpret_cast<u32>(&mDescs[0]); // ok reinterpret cast - DMA register takes lldesc address
        i2s->in_link.start = 1;
        i2s->conf.rx_start = 1;

        mDrainIdx = 0;
        mArmed = true;
        g_i2s_rx_debug[5] = 0xB16B00B5; // begin() completed marker
        return true;
    }

    bool finished() const FL_NO_EXCEPT override { return mFinished; }

    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override {
        if (!mArmed) {
            return RxWaitResult::TIMEOUT;
        }
        const i64 start_us = esp_timer_get_time();
        const i64 timeout_us = static_cast<i64>(timeout_ms) * 1000;
        while (true) {
            drainReady();
            if (mFinished) {
                stopCapture();
                return RxWaitResult::SUCCESS;
            }
            if (esp_timer_get_time() - start_us >= timeout_us) {
                // Timed out — keep whatever was captured (a frame with
                // no trailing idle gap still decodes).
                stopCapture();
                flushCurrentRun();
                return mRuns.empty() ? RxWaitResult::TIMEOUT
                                     : RxWaitResult::SUCCESS;
            }
            taskYIELD();
        }
    }

    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) FL_NO_EXCEPT override {
        if (mRuns.empty() || out.empty()) {
            return fl::result<u32, DecodeError>::failure(
                DecodeError::INVALID_ARGUMENT);
        }
        // 4-phase pair decode over the level runs — same window
        // semantics as the RMT / GPIO-ISR backends (HIGH run then LOW
        // run per bit, MSB first, reset run ends the frame).
        const u32 reset_min_ns = timing.reset_min_us * 1000;
        size_t errors = 0;
        u32 bytes_decoded = 0;
        u8 current = 0;
        int bit_index = 0;
        size_t i = 0;
        // Skip anything before the first HIGH run.
        while (i < mRuns.size() && mRuns[i].level() == 0) {
            ++i;
        }
        while (i + 1 < mRuns.size()) {
            const SampleRun &hi = mRuns[i];
            const SampleRun &lo = mRuns[i + 1];
            if (hi.level() != 1 || lo.level() != 0) {
                ++errors;
                ++i;
                continue;
            }
            
            int bit = -1;
            if (hi.duration_ns() >= timing.t0h_min_ns &&
                hi.duration_ns() <= timing.t0h_max_ns &&
                (lo.duration_ns() >= timing.t0l_min_ns || lo.duration_ns() >= reset_min_ns)) {
                bit = 0;
            } else if (hi.duration_ns() >= timing.t1h_min_ns &&
                       hi.duration_ns() <= timing.t1h_max_ns &&
                       (lo.duration_ns() >= timing.t1l_min_ns || lo.duration_ns() >= reset_min_ns)) {
                bit = 1;
            }
            if (bit < 0) {
                ++errors;
                i += 2;
                continue;
            }
            current = static_cast<u8>((current << 1) | bit);
            if (++bit_index == 8) {
                if (bytes_decoded >= out.size()) {
                    return fl::result<u32, DecodeError>::failure(
                        DecodeError::BUFFER_OVERFLOW);
                }
                out[bytes_decoded++] = current;
                current = 0;
                bit_index = 0;
            }
            if (lo.duration_ns() >= reset_min_ns) {
                break; // end of frame
            }
            i += 2;
        }
        if (bytes_decoded == 0 || errors * 10 > bytes_decoded * 8) {
            return fl::result<u32, DecodeError>::failure(
                DecodeError::HIGH_ERROR_RATE);
        }
        return fl::result<u32, DecodeError>::success(bytes_decoded);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) FL_NO_EXCEPT override {
        if (out.empty() || offset >= mRuns.size()) {
            return 0;
        }
        size_t n = mRuns.size() - offset;
        if (n > out.size()) {
            n = out.size();
        }
        for (size_t i = 0; i < n; ++i) {
            const auto &run = mRuns[offset + i];
            out[i] = EdgeTime(run.level() != 0, run.duration_ns());
        }
        return n;
    }

    const char *name() const FL_NO_EXCEPT override { return "I2S_RX"; }

    int getPin() const FL_NO_EXCEPT override { return mPin; }

    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override {
        mRuns.clear();
        for (const auto &e : edges) {
            mRuns.push_back(SampleRun::make(e.ns, static_cast<u8>(e.high ? 1 : 0)));
        }
        mFinished = true;
        return true;
    }

  private:
    bool allocRing() FL_NO_EXCEPT {
        if (mRingMem == nullptr) {
            mRingMem = static_cast<u8 *>(heap_caps_malloc(
                kRxDescCount * kRxDescBytes, MALLOC_CAP_DMA));
            mDescs = static_cast<lldesc_t *>(heap_caps_malloc(
                kRxDescCount * sizeof(lldesc_t), MALLOC_CAP_DMA));
            if (mRingMem == nullptr || mDescs == nullptr) {
                FL_WARN_F("I2sRxSampler: DMA ring alloc failed");
                freeRing();
                return false;
            }
        }
        for (size_t i = 0; i < kRxDescCount; ++i) {
            lldesc_t &d = mDescs[i];
            d.buf = mRingMem + i * kRxDescBytes;
            d.size = kRxDescBytes;
            d.length = 0;
            d.owner = 1;
            d.sosf = 0;
            d.eof = 0;
            d.offset = 0;
            d.empty = 0;
            d.qe.stqe_next = &mDescs[(i + 1) % kRxDescCount];
        }
        return true;
    }

    void freeRing() FL_NO_EXCEPT {
        if (mRingMem) {
            heap_caps_free(mRingMem);
            mRingMem = nullptr;
        }
        if (mDescs) {
            heap_caps_free(mDescs);
            mDescs = nullptr;
        }
    }

    void stopCapture() FL_NO_EXCEPT {
        if (!mArmed) {
            return;
        }
        i2s_dev_t *i2s = &I2S0;
        i2s->conf.rx_start = 0;
        i2s->in_link.stop = 1;
        mArmed = false;
        i2sPortRelease(0, kI2sRxOwner);
    }

    void teardown() FL_NO_EXCEPT {
        stopCapture();
        freeRing();
    }

    /// Drain descriptors the DMA has handed back (owner flipped to 0).
    void drainReady() FL_NO_EXCEPT {
        for (size_t scanned = 0; scanned < kRxDescCount; ++scanned) {
            lldesc_t &d = mDescs[mDrainIdx];
            if (d.owner != 0) {
                return; // next fill not ready yet
            }
            const size_t len = d.length ? d.length : kRxDescBytes;
            g_i2s_rx_debug[0]++;
            // lldesc buf is volatile u8* — the DMA finished writing this
            // descriptor (owner flipped), so the snapshot read is safe.
            processSamples(const_cast<const u8 *>(d.buf), len);
            // Recycle the descriptor for the circular DMA.
            d.length = 0;
            d.owner = 1;
            mDrainIdx = (mDrainIdx + 1) % kRxDescCount;
            if (mFinished) {
                return;
            }
        }
    }

    /// Run-length decode a chunk of raw sample words into level runs.
    ///
    /// Hot path: at 16 Msps a per-bit loop cannot keep up with the DMA
    /// stream (bench: mid-frame sample gaps past ~50 LEDs). CLZ-based
    /// run scanning handles a whole 32-bit word in a few iterations.
    /// Bit order (bench-derived): MSB-first through the full word with
    /// the high half sampled first — i.e. bit31 is the earliest sample,
    /// so the raw little-endian word IS the stream.
    void processSamples(const u8 *buf, size_t len_bytes) FL_NO_EXCEPT {
        const u32 *words = reinterpret_cast<const u32 *>(buf); // ok reinterpret cast - DMA buffer is 4-byte aligned
        const size_t nwords = len_bytes / 4;
        for (size_t w = 0; w < nwords && !mFinished; ++w) {
            const u32 word = words[w];
            g_i2s_rx_debug[1]++;
            if (word != 0) {
                if (g_i2s_rx_debug[3] == 0) {
                    g_i2s_rx_debug[3] = word;
                }
                mSawFirstEdge = true;
            }

            const u32 solid = mCurLevel ? 0xFFFFFFFFu : 0x00000000u;
            if (word == solid) {
                // Fast path: whole word continues the current run.
                mCurRunSamples += 32;
            } else {
                int pos = 0;
                while (pos < 32) {
                    const u32 shifted = word << pos;
                    // Bits equal to the current level become 0; the
                    // first set bit marks the transition. Cap by the
                    // remaining-bit count so shifted-in zeros at the
                    // bottom can't extend a run.
                    const u32 diff = mCurLevel ? ~shifted : shifted;
                    const int remaining = 32 - pos;
                    int run = (diff == 0) ? remaining : __builtin_clz(diff);
                    if (run > remaining) {
                        run = remaining;
                    }
                    mCurRunSamples += static_cast<u32>(run);
                    pos += run;
                    if (pos < 32) {
                        flushCurrentRun();
                        mCurLevel = static_cast<u8>(mCurLevel ? 0 : 1);
                    }
                }
            }

            // Frame-end detection once per word (2 µs granularity):
            // after real signal, a LOW run past the idle threshold ends
            // the capture.
            if (mSawFirstEdge && mCurLevel == 0 &&
                static_cast<u64>(mCurRunSamples) * kNsPerSample >
                    mSignalRangeMaxNs) {
                flushCurrentRun();
                mFinished = true;
            }
            if (mRuns.size() >= kMaxEdges) {
                mFinished = true;
            }
        }
    }

    void flushCurrentRun() FL_NO_EXCEPT {
        if (mCurRunSamples == 0) {
            return;
        }
        // Skip the leading idle-low period before the first HIGH.
        if (!mSawFirstEdge && mCurLevel == 0) {
            mCurRunSamples = 0;
            return;
        }
        // 62.5 ns per sample without floating point: ×62 plus one
        // extra ns every 2 samples.
        const u32 dur =
            mCurRunSamples * kNsPerSample + (mCurRunSamples / 2);
        mRuns.push_back(SampleRun::make(dur, mCurLevel));
        g_i2s_rx_debug[4] = static_cast<u32>(mRuns.size());
        mCurRunSamples = 0;
    }

    int mPin;
    u32 mSignalRangeMaxNs = 300000;
    bool mArmed = false;
    bool mFinished = false;
    bool mSawFirstEdge = false;
    u8 mCurLevel = 0;
    u32 mCurRunSamples = 0;
    u32 mSubNs = 0;
    size_t mDrainIdx = 0;
    u8 *mRingMem = nullptr;
    lldesc_t *mDescs = nullptr;
    fl::vector<SampleRun> mRuns;
};

} // anonymous namespace

fl::shared_ptr<RxDevice> createI2sRxSampler(int pin) FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32DEV)
    return fl::make_shared<I2sRxSampler>(pin);
#else
    (void)pin;
    return fl::shared_ptr<RxDevice>();
#endif
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
