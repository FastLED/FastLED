/// @file parlio_rx_sampler.cpp.hpp
/// @brief PARLIO-RX 1-bit oversampling capture backend (FastLED#3586).

// IWYU pragma: private

#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#include "platforms/esp/32/drivers/parlio_rx/parlio_rx_sampler.h"
#include "fl/stl/shared_ptr.h"

#include "soc/soc_caps.h"  // IWYU pragma: keep

#if defined(SOC_PARLIO_SUPPORTED) && SOC_PARLIO_SUPPORTED && \
    defined(SOC_PARLIO_RX_UNITS_PER_GROUP) && SOC_PARLIO_RX_UNITS_PER_GROUP > 0

#include "fl/log/log.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
// IWYU pragma: begin_keep
#include "fl/stl/vector.h"
// IWYU pragma: end_keep

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/parlio_rx.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

namespace {

/// Sample clock. 16 MHz -> 62.5 ns per sample, matching the classic-ESP32
/// I2S-RX oversampler so the duration arithmetic below is identical.
constexpr u32 kSampleHz = 16000000;

/// Whole-ns part of one sample period; the remaining 0.5 ns is added back
/// as (samples / 2) to stay in integer math.
constexpr u32 kNsPerSample = 62;

/// Capture buffer. 32 KB = 262144 samples = ~16.4 ms at 16 MHz. Sized so
/// that even when sampling opens partway through a frame — the DMA starts
/// at begin(), the transmit happens some milliseconds later — the window
/// still spans a whole subsequent 100-LED WS2812 frame (~3 ms) plus its
/// trailing reset gap. PARLIO requires a receive size that is a multiple
/// of 4 bytes.
constexpr size_t kCaptureBytes = 32768;

/// Minimum level runs for a capture to count as a real frame rather than
/// the tail of one already in flight when sampling started. A 100-LED
/// frame is ~4800 runs; anything under this is a fragment.
constexpr size_t kMinFrameRuns = 64;

/// Hard cap on stored level runs, mirroring the I2S-RX backend.
constexpr size_t kMaxEdges = 8000;

/// One captured level run, packed: bit0 = level, bits1..31 = duration ns.
struct SampleRun {
    u32 packed;
    u32 duration_ns() const FL_NO_EXCEPT { return packed >> 1; }
    u8 level() const FL_NO_EXCEPT { return static_cast<u8>(packed & 1u); }
    static SampleRun make(u32 dur_ns, u8 lvl) FL_NO_EXCEPT {
        return SampleRun{(dur_ns << 1) | (lvl & 1u)};
    }
};

class ParlioRxSampler final : public RxDevice {
  public:
    explicit ParlioRxSampler(int pin) FL_NO_EXCEPT : mPin(pin) {}

    ~ParlioRxSampler() override { teardown(); }

    bool begin(const RxConfig &config) FL_NO_EXCEPT override {
        mSignalRangeMaxNs = config.signal_range_max_ns;
        mRuns.clear();
        mFinished = false;
        mSawFirstEdge = false;
        mCurLevel = 0;
        mCurRunSamples = 0;

        if (!allocBuffer()) {
            return false;
        }

        if (mUnit == nullptr && !createUnit()) {
            teardown();
            return false;
        }

        // Queue the capture. The unit is already enabled and the soft
        // delimiter running, so sampling begins immediately; wait()
        // blocks on completion.
        parlio_receive_config_t recv_cfg = {};
        recv_cfg.delimiter = mDelimiter;
        recv_cfg.flags.partial_rx_en = 0;
        recv_cfg.flags.indirect_mount = 0;

        esp_err_t err =
            parlio_rx_unit_receive(mUnit, mBuffer, kCaptureBytes, &recv_cfg);
        if (err != ESP_OK) {
            FL_WARN_F("ParlioRxSampler: receive queue failed: %s",
                      esp_err_to_name(err));
            return false;
        }

        mArmed = true;
        return true;
    }

    bool finished() const FL_NO_EXCEPT override { return mFinished; }

    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override {
        if (!mArmed) {
            return RxWaitResult::TIMEOUT;
        }
        mArmed = false;

        // The DMA transfer completes when the whole buffer is filled,
        // which at 16 MHz is a fixed ~4.1 ms regardless of the signal.
        // Bound the wait by the caller's timeout so a dead line cannot
        // hang the bench.
        esp_err_t err = parlio_rx_unit_wait_all_done(
            mUnit, static_cast<int>(timeout_ms));
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            FL_WARN_F("ParlioRxSampler: wait failed: %s",
                      esp_err_to_name(err));
            return RxWaitResult::TIMEOUT;
        }

        processSamples(mBuffer, kCaptureBytes);
        flushCurrentRun();

        return mRuns.empty() ? RxWaitResult::TIMEOUT : RxWaitResult::SUCCESS;
    }

    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) FL_NO_EXCEPT override {
        if (mRuns.empty() || out.empty()) {
            return fl::result<u32, DecodeError>::failure(
                DecodeError::INVALID_ARGUMENT);
        }
        // 4-phase pair decode over the level runs — same window semantics
        // as the RMT / GPIO-ISR / I2S-RX backends (HIGH run then LOW run
        // per bit, MSB first, reset run ends the frame).
        const u32 reset_min_ns = timing.reset_min_us * 1000;
        size_t errors = 0;
        u32 bytes_decoded = 0;
        u8 current = 0;
        int bit_index = 0;
        size_t i = 0;
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
                (lo.duration_ns() >= timing.t0l_min_ns ||
                 lo.duration_ns() >= reset_min_ns)) {
                bit = 0;
            } else if (hi.duration_ns() >= timing.t1h_min_ns &&
                       hi.duration_ns() <= timing.t1h_max_ns &&
                       (lo.duration_ns() >= timing.t1l_min_ns ||
                        lo.duration_ns() >= reset_min_ns)) {
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

    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) FL_NO_EXCEPT override {
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

    const char *name() const FL_NO_EXCEPT override { return "PARLIO_RX"; }

    int getPin() const FL_NO_EXCEPT override { return mPin; }

    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override {
        mRuns.clear();
        for (const auto &e : edges) {
            mRuns.push_back(
                SampleRun::make(e.ns, static_cast<u8>(e.high ? 1 : 0)));
        }
        mFinished = true;
        return true;
    }

  private:
    bool allocBuffer() FL_NO_EXCEPT {
        if (mBuffer != nullptr) {
            return true;
        }
        mBuffer = static_cast<u8 *>(
            heap_caps_calloc(1, kCaptureBytes, MALLOC_CAP_DMA));
        if (mBuffer == nullptr) {
            FL_WARN_F("ParlioRxSampler: DMA buffer alloc failed (%s bytes)",
                      static_cast<int>(kCaptureBytes));
            return false;
        }
        return true;
    }

    bool createUnit() FL_NO_EXCEPT {
        parlio_rx_unit_config_t unit_cfg = {};
        unit_cfg.trans_queue_depth = 2;
        unit_cfg.max_recv_size = kCaptureBytes;
        unit_cfg.data_width = 1;  // single data line: the RX pin
        unit_cfg.clk_src = PARLIO_CLK_SRC_DEFAULT;
        unit_cfg.exp_clk_freq_hz = kSampleHz;
        unit_cfg.clk_in_gpio_num = GPIO_NUM_NC;  // internal clock
        unit_cfg.clk_out_gpio_num = GPIO_NUM_NC;
        unit_cfg.valid_gpio_num = GPIO_NUM_NC;   // soft delimiter instead
        unit_cfg.data_gpio_nums[0] = static_cast<gpio_num_t>(mPin);
        for (size_t i = 1; i < SOC_PARLIO_RX_UNIT_MAX_DATA_WIDTH; ++i) {
            unit_cfg.data_gpio_nums[i] = GPIO_NUM_NC;
        }

        esp_err_t err = parlio_new_rx_unit(&unit_cfg, &mUnit);
        if (err != ESP_OK) {
            FL_WARN_F("ParlioRxSampler: rx unit create failed: %s",
                      esp_err_to_name(err));
            mUnit = nullptr;
            return false;
        }

        // Software delimiter: capture is gated by start/stop calls rather
        // than an external valid signal, so no extra pin is needed.
        parlio_rx_soft_delimiter_config_t delim_cfg = {};
        delim_cfg.sample_edge = PARLIO_SAMPLE_EDGE_POS;
        delim_cfg.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
        delim_cfg.eof_data_len = kCaptureBytes;
        delim_cfg.timeout_ticks = 0;

        err = parlio_new_rx_soft_delimiter(&delim_cfg, &mDelimiter);
        if (err != ESP_OK) {
            FL_WARN_F("ParlioRxSampler: soft delimiter create failed: %s",
                      esp_err_to_name(err));
            teardown();
            return false;
        }

        err = parlio_rx_unit_enable(mUnit, true);
        if (err != ESP_OK) {
            FL_WARN_F("ParlioRxSampler: rx unit enable failed: %s",
                      esp_err_to_name(err));
            teardown();
            return false;
        }

        err = parlio_rx_soft_delimiter_start_stop(mUnit, mDelimiter, true);
        if (err != ESP_OK) {
            FL_WARN_F("ParlioRxSampler: soft delimiter start failed: %s",
                      esp_err_to_name(err));
            teardown();
            return false;
        }

        return true;
    }

    void teardown() FL_NO_EXCEPT {
        if (mUnit != nullptr) {
            if (mDelimiter != nullptr) {
                parlio_rx_soft_delimiter_start_stop(mUnit, mDelimiter, false);
            }
            parlio_rx_unit_disable(mUnit);
        }
        if (mDelimiter != nullptr) {
            parlio_del_rx_delimiter(mDelimiter);
            mDelimiter = nullptr;
        }
        if (mUnit != nullptr) {
            parlio_del_rx_unit(mUnit);
            mUnit = nullptr;
        }
        if (mBuffer != nullptr) {
            heap_caps_free(mBuffer);
            mBuffer = nullptr;
        }
    }

    /// Run-length-decode the 1-bit sample stream into level runs.
    /// Peripheral-agnostic — identical to the classic-ESP32 I2S-RX
    /// oversampler's CLZ run-scan (#3576 Phase 3).
    void processSamples(const u8 *buf, size_t len_bytes) FL_NO_EXCEPT {
        // Walk BYTES in DMA order and, within each byte, bits MSB-first.
        // PARLIO is configured with PARLIO_BIT_PACK_ORDER_MSB, so the
        // earliest sample of a byte is bit 7. Scanning bytes directly
        // (rather than reinterpreting as u32) keeps this independent of
        // word endianness: on a little-endian core a u32 load would put
        // the earliest-sampled byte in the LOW bits, which reverses the
        // time order a leading-zero scan depends on.
        for (size_t b = 0; b < len_bytes && !mFinished; ++b) {
            const u8 byte = buf[b];
            if (byte != 0) {
                mSawFirstEdge = true;
            }

            const u8 solid = mCurLevel ? 0xFFu : 0x00u;
            if (byte == solid) {
                mCurRunSamples += 8;  // whole byte continues the run
            } else {
                int pos = 0;
                while (pos < 8) {
                    // Bits equal to the current level become 0; the first
                    // set bit marks the transition. Shift the byte up so
                    // bit 7 (earliest sample) sits at bit 31 for the
                    // leading-zero count.
                    const u32 shifted =
                        (static_cast<u32>(byte) << (24 + pos)) & 0xFFFFFFFFu;
                    const u32 diff = mCurLevel ? ~shifted : shifted;
                    const int remaining = 8 - pos;
                    int run = (diff == 0) ? remaining
                                          : static_cast<int>(__builtin_clz(diff));
                    if (run > remaining) {
                        run = remaining;
                    }
                    mCurRunSamples += static_cast<u32>(run);
                    pos += run;
                    if (pos < 8) {
                        flushCurrentRun();
                        mCurLevel = static_cast<u8>(mCurLevel ? 0 : 1);
                    }
                }
            }

            // Frame-end detection: after real signal, a LOW run past the
            // idle threshold ends the capture.
            if (mSawFirstEdge && mCurLevel == 0 &&
                static_cast<u64>(mCurRunSamples) * kNsPerSample >
                    mSignalRangeMaxNs) {
                if (mRuns.size() >= kMinFrameRuns) {
                    flushCurrentRun();
                    mFinished = true;
                } else {
                    // Fragment of a frame that was already transmitting
                    // when the DMA window opened. Discard it and resync on
                    // the next frame rather than reporting a short capture.
                    mRuns.clear();
                    mCurRunSamples = 0;
                    mSawFirstEdge = false;
                }
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
        // 62.5 ns per sample without floating point.
        const u32 dur = mCurRunSamples * kNsPerSample + (mCurRunSamples / 2);
        mRuns.push_back(SampleRun::make(dur, mCurLevel));
        mCurRunSamples = 0;
    }

    int mPin;
    u32 mSignalRangeMaxNs = 100000;
    bool mArmed = false;
    bool mFinished = false;
    bool mSawFirstEdge = false;
    u8 mCurLevel = 0;
    u32 mCurRunSamples = 0;
    u8 *mBuffer = nullptr;
    parlio_rx_unit_handle_t mUnit = nullptr;
    parlio_rx_delimiter_handle_t mDelimiter = nullptr;
    fl::vector<SampleRun> mRuns;
};

} // anonymous namespace

fl::shared_ptr<RxDevice> createParlioRxSampler(int pin) FL_NO_EXCEPT {
    return fl::make_shared<ParlioRxSampler>(pin);
}

} // namespace fl

#else // no PARLIO RX unit on this SoC

namespace fl {
fl::shared_ptr<RxDevice> createParlioRxSampler(int pin) FL_NO_EXCEPT {
    (void)pin;
    return nullptr;
}
} // namespace fl

#endif // SOC_PARLIO_SUPPORTED && SOC_PARLIO_RX_UNITS_PER_GROUP

#endif // FL_IS_ESP32
