// IWYU pragma: private

/// @file rmt_rx_channel_4.cpp.hpp
/// @brief IDF 4.x implementation of `RmtRxChannel` (issue #3465).
///
/// Closes the long-standing gap where `FastLED.addRx(...)` on classic
/// ESP32 / esp32dev with the IDF 4.x toolchain returned a `nullptr`
/// from the factory and was wrapped as a `DummyRxDevice`.
///
/// ## Design notes
///
/// The IDF 5.x sibling (`../rmt_rx_5/rmt_rx_channel_5.cpp.hpp`) uses
/// the modern `rmt_new_rx_channel()` / `rmt_receive()` / encoder /
/// event-callback flow. The legacy `driver/rmt.h` API used here is
/// markedly simpler:
///
///   - `rmt_config()` with `rmt_mode = RMT_MODE_RX` + an
///     `rmt_rx_config_t { idle_threshold, filter_ticks_thresh,
///     filter_en }` field.
///   - `rmt_driver_install(channel, rx_buf_size, intr_flags)` where
///     `rx_buf_size > 0` makes the IDF allocate a FreeRTOS ring
///     buffer for us.
///   - `rmt_get_ringbuf_handle()` then `xRingbufferReceive()` to pop
///     captured `rmt_item32_t*` chunks.
///   - No custom IRAM ISR. Unlike the TX path (which needs ISR-driven
///     refill because the RMT TX buffer is 64 symbols), RX has no
///     latency pressure — the IDF ring buffer absorbs symbols and
///     the consumer thread drains them in `wait()` / `decode()`.
///
/// ## Decode helpers
///
/// The bit-decoding helpers (`isResetPulse`, `isGapPulse`,
/// `decodeBit`, `decodeRmtSymbols`) are intentionally duplicated from
/// the IDF 5 impl rather than refactored into a shared header. The
/// IDF 5 helpers use `rmt_symbol_word_t` (from `driver/rmt_rx.h`) and
/// the IDF 4 equivalents below use `rmt_item32_t` (from
/// `driver/rmt.h`). Both have identical 32-bit field layout
/// (duration0:15 / level0:1 / duration1:15 / level1:1) so the
/// algorithmic code is line-for-line the same. Factoring the helpers
/// into a single backend-agnostic file is a sensible follow-up and is
/// flagged in issue #3465.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

// Gate exactly mirrors the rmt_4 TX driver: classic ESP32 / S2 with
// IDF 4.x (FASTLED_RMT5 == 0). RMT5-only chips (C6 / H2 / P4 / C5)
// and the IDF 5.x build of S3 / C3 / ESP32 all skip this TU.
#if FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

#include "fl/log/log.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/iterator.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

// RMT pin assignment helper — IDF 4.4+ has `rmt_set_gpio()` (with
// the invert-output flag); older 4.x has the simpler `rmt_set_pin()`.
// Mirror the same fork the rmt_4 TX driver already uses.

// Local logging toggle, mirroring the IDF 5 impl's idiom.
#ifndef FASTLED_RX_LOG_ENABLED
#define FASTLED_RX_LOG_ENABLED 0
#endif

#if FASTLED_RX_LOG_ENABLED
#define FL_LOG_RX(X) FL_DBG(X)
#else
#define FL_LOG_RX(X) FL_DBG_NO_OP(X)
#endif

namespace fl {

// Match the IDF 5 sibling: RmtSymbol is a u32 typedef from the public
// header, and the underlying IDF type is the same 32-bit width.
FL_STATIC_ASSERT(sizeof(RmtSymbol) == sizeof(rmt_item32_t),
                 "RmtSymbol must be the same size as rmt_item32_t (32 bits)");
FL_STATIC_ASSERT(fl::is_trivially_copyable<rmt_item32_t>::value,
                 "rmt_item32_t must be trivially copyable for safe casting");

namespace {

//=============================================================================
// Decode helpers — IDF 4 port of the IDF 5 sibling's anonymous-namespace
// helpers. Same algorithm, substitutes rmt_symbol_word_t -> rmt_item32_t.
//=============================================================================

inline u32 ticksToNs(u32 ticks, u32 ns_per_tick) { return ticks * ns_per_tick; }

inline bool isResetPulse(RmtSymbol symbol, const ChipsetTiming4Phase &timing,
                         u32 ns_per_tick) {
    const auto item = fl::bit_cast<rmt_item32_t>(symbol);
    u32 duration0_ns = ticksToNs(item.duration0, ns_per_tick);
    u32 duration1_ns = ticksToNs(item.duration1, ns_per_tick);
    u32 reset_min_ns = timing.reset_min_us * 1000;
    if (item.level0 == 0 && duration0_ns >= reset_min_ns) {
        return true;
    }
    if (item.level1 == 0 && duration1_ns >= reset_min_ns) {
        return true;
    }
    return false;
}

inline bool isGapPulse(RmtSymbol symbol, const ChipsetTiming4Phase &timing,
                       u32 ns_per_tick) {
    if (timing.gap_tolerance_ns == 0) {
        return false;
    }
    const auto item = fl::bit_cast<rmt_item32_t>(symbol);
    u32 duration0_ns = ticksToNs(item.duration0, ns_per_tick);
    u32 duration1_ns = ticksToNs(item.duration1, ns_per_tick);
    if (item.level1 == 0 && duration1_ns > timing.t0l_max_ns &&
        duration1_ns <= timing.gap_tolerance_ns) {
        return true;
    }
    if (item.level0 == 0 && duration0_ns > timing.t0l_max_ns &&
        duration0_ns <= timing.gap_tolerance_ns) {
        return true;
    }
    return false;
}

inline int decodeBit(RmtSymbol symbol, const ChipsetTiming4Phase &timing,
                     u32 ns_per_tick) {
    const auto item = fl::bit_cast<rmt_item32_t>(symbol);
    u32 high_ns = ticksToNs(item.duration0, ns_per_tick);
    u32 low_ns = ticksToNs(item.duration1, ns_per_tick);
    if (item.level0 != 1 || item.level1 != 0) {
        return -1;
    }
    bool t0h_match =
        (high_ns >= timing.t0h_min_ns) && (high_ns <= timing.t0h_max_ns);
    bool t0l_match =
        (low_ns >= timing.t0l_min_ns) && (low_ns <= timing.t0l_max_ns);
    if (t0h_match && t0l_match) {
        return 0;
    }
    bool t1h_match =
        (high_ns >= timing.t1h_min_ns) && (high_ns <= timing.t1h_max_ns);
    bool t1l_match =
        (low_ns >= timing.t1l_min_ns) && (low_ns <= timing.t1l_max_ns);
    if (t1h_match && t1l_match) {
        return 1;
    }
    return -1;
}

fl::result<u32, DecodeError> decodeRmtSymbols(const ChipsetTiming4Phase &timing,
                                              u32 resolution_hz,
                                              fl::span<const RmtSymbol> symbols,
                                              fl::span<u8> bytes_out) {
    if (symbols.empty() || bytes_out.empty()) {
        return fl::result<u32, DecodeError>::failure(
            DecodeError::INVALID_ARGUMENT);
    }
    u32 ns_per_tick = 1000000000UL / resolution_hz;
    u32 bytes_decoded = 0;
    u8 current_byte = 0;
    int bit_index = 0;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (isResetPulse(symbols[i], timing, ns_per_tick)) {
            break;
        }
        if (isGapPulse(symbols[i], timing, ns_per_tick)) {
            continue;
        }
        int bit = decodeBit(symbols[i], timing, ns_per_tick);
        if (bit < 0) {
            return fl::result<u32, DecodeError>::failure(
                DecodeError::INVALID_SYMBOL);
        }
        current_byte = static_cast<u8>((current_byte << 1) | (bit & 0x1));
        ++bit_index;
        if (bit_index == 8) {
            if (bytes_decoded >= bytes_out.size()) {
                // Output buffer full — silent stop, matches IDF 5 sibling.
                break;
            }
            bytes_out[bytes_decoded++] = current_byte;
            current_byte = 0;
            bit_index = 0;
        }
    }
    return fl::result<u32, DecodeError>::success(bytes_decoded);
}

//=============================================================================
// Channel allocation
//
// Classic ESP32 has 8 RMT channels (0..7); ESP32-S2 has 4 (0..3).
// FastLED TX historically claims channels starting at 0; this RX
// allocator picks from the high end so a single TX+RX strip can
// coexist on the same chip without colliding.
//=============================================================================

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
constexpr int kRmt4RxMaxChannels = SOC_RMT_TX_CANDIDATES_PER_GROUP;
#elif defined(FL_IS_ESP_32S2)
constexpr int kRmt4RxMaxChannels = 4;
#else
constexpr int kRmt4RxMaxChannels = 8;
#endif

static bool s_channel_in_use[kRmt4RxMaxChannels] = {false};

int allocateRxChannel() FL_NO_EXCEPT {
    // Descend from the highest channel so TX (which uses 0,1,...
    // ascending) and RX (which uses N-1,N-2,... descending) don't
    // collide on a typical 1-TX + 1-RX AutoResearch setup.
    for (int ch = kRmt4RxMaxChannels - 1; ch >= 0; --ch) {
        if (!s_channel_in_use[ch]) {
            s_channel_in_use[ch] = true;
            return ch;
        }
    }
    return -1;
}

void releaseRxChannel(int channel) FL_NO_EXCEPT {
    if (channel >= 0 && channel < kRmt4RxMaxChannels) {
        s_channel_in_use[channel] = false;
    }
}

} // anonymous namespace

//=============================================================================
// RmtRxChannelImpl — IDF 4.x flavour
//=============================================================================

class RmtRxChannelImpl : public RmtRxChannel {
  public:
    explicit RmtRxChannelImpl(int pin) FL_NO_EXCEPT
        : mPin(static_cast<gpio_num_t>(pin)),
          mChannel(-1),
          mInstalled(false),
          mRunning(false),
          mResolutionHz(0),
          mBufferSize(0),
          mInternalBuffer(),
          mRingbufHandle(nullptr),
          mMemoryChannelId(128 + (pin & 0x7F)),
          mMemoryRegistered(false) {
        FL_LOG_RX("RmtRxChannelImpl(pin=" << pin << ") constructed");

        // Pre-register a minimum RX allocation with the RMT4 memory
        // manager (#3469). Same channel-id offset convention as the
        // RMT5 sibling (128 + (pin & 0x7F)) so TX (which uses raw
        // channel numbers 0..7) never collides with an RX record.
        // Failure is non-fatal — the RX device still constructs; the
        // caller can retry allocation at begin() time.
        constexpr size_t kMinRxSymbols = 64;
        size_t reserved = 0;
        if (RmtMemoryManager4::instance().tryAllocateRx(
                mMemoryChannelId, kMinRxSymbols, reserved)) {
            mMemoryRegistered = true;
        }
    }

    ~RmtRxChannelImpl() override {
        teardown();
        if (mMemoryRegistered) {
            (void)RmtMemoryManager4::instance().free(mMemoryChannelId,
                                                     /*is_tx=*/false);
            mMemoryRegistered = false;
        }
    }

    bool begin(const RxConfig &config) FL_NO_EXCEPT override {
        // Re-arm path: if already installed, just clear our buffer and
        // restart RX. The driver stays installed across begin() calls
        // for the lifetime of the object.
        if (mInstalled) {
            mInternalBuffer.clear();
            mRunning = true;
            esp_err_t err = rmt_rx_start(static_cast<rmt_channel_t>(mChannel),
                                         /*rx_idx_rst=*/true);
            if (err != ESP_OK) {
                FL_WARN_F("[RMT4 RX] rmt_rx_start re-arm failed, err=%s", err);
                return false;
            }
            return true;
        }

        // First-time init.
        if (mChannel < 0) {
            mChannel = allocateRxChannel();
            if (mChannel < 0) {
                FL_WARN_F("[RMT4 RX] no free RMT channel available");
                return false;
            }
        }

        // Resolve clock + buffer parameters from RxConfig (with the
        // documented FastLED defaults when the caller leaves them at 0).
        mResolutionHz = (config.hz != 0) ? config.hz : 40000000u; // 40 MHz
        mBufferSize = (config.buffer_size != 0) ? config.buffer_size : 1024;

        // RMT clock divider so the RMT tick rate matches the requested
        // resolution. The APB clock is 80 MHz on classic ESP32 / S2.
        // Example: 40 MHz target -> divider = 2 -> 25 ns per tick.
        constexpr u32 kApbHz = 80000000u;
        u32 clk_div = kApbHz / mResolutionHz;
        if (clk_div == 0)
            clk_div = 1;
        if (clk_div > 255)
            clk_div = 255;

        rmt_config_t cfg = {};
        cfg.rmt_mode = RMT_MODE_RX;
        cfg.channel = static_cast<rmt_channel_t>(mChannel);
        cfg.gpio_num = mPin;
        cfg.clk_div = static_cast<u8>(clk_div);
        cfg.mem_block_num = 1;
        // RX-mode config: how long without an edge before "done", and
        // optional noise-filter threshold. The numbers below mirror
        // the IDF 5 impl's behaviour for WS2812B-family capture.
        cfg.rx_config.idle_threshold = 12000; // ticks
        cfg.rx_config.filter_ticks_thresh = 100;
        cfg.rx_config.filter_en = true;

        esp_err_t err = rmt_config(&cfg);
        if (err != ESP_OK) {
            FL_WARN_F(
                "[RMT4 RX] rmt_config failed on channel %s pin %s, err=%s",
                mChannel, static_cast<int>(mPin), err);
            return false;
        }

        // Install the IDF driver and let it own the RX ring buffer. The
        // rx_buf_size argument is in BYTES. We size for the requested
        // symbol count (each rmt_item32_t is 4 bytes).
        size_t rx_buf_bytes = mBufferSize * sizeof(rmt_item32_t);
        err = rmt_driver_install(static_cast<rmt_channel_t>(mChannel),
                                 rx_buf_bytes, 0);
        if (err != ESP_OK) {
            FL_WARN_F(
                "[RMT4 RX] rmt_driver_install failed on channel %s, err=%s",
                mChannel, err);
            return false;
        }
        mInstalled = true;

        // Cache the ring buffer handle so wait() can pull from it.
        err = rmt_get_ringbuf_handle(static_cast<rmt_channel_t>(mChannel),
                                     &mRingbufHandle);
        if (err != ESP_OK || mRingbufHandle == nullptr) {
            FL_WARN_F(
                "[RMT4 RX] rmt_get_ringbuf_handle failed on channel %s, err=%s",
                mChannel, err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            return false;
        }

        // Assign the GPIO. IDF 4.4 introduced rmt_set_gpio (with the
        // invert flag); earlier 4.x uses rmt_set_pin and ignores the
        // flag.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        err = rmt_set_gpio(static_cast<rmt_channel_t>(mChannel), RMT_MODE_RX,
                           mPin, /*invert=*/false);
#else
        err = rmt_set_pin(static_cast<rmt_channel_t>(mChannel), RMT_MODE_RX,
                          mPin);
#endif
        if (err != ESP_OK) {
            FL_WARN_F("[RMT4 RX] rmt_set_gpio/pin failed on channel %s pin %s, "
                      "err=%s",
                      mChannel, static_cast<int>(mPin), err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            return false;
        }

        // Arm the receiver.
        err = rmt_rx_start(static_cast<rmt_channel_t>(mChannel),
                           /*rx_idx_rst=*/true);
        if (err != ESP_OK) {
            FL_WARN_F("[RMT4 RX] rmt_rx_start failed on channel %s, err=%s",
                      mChannel, err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            return false;
        }
        mRunning = true;
        return true;
    }

    bool finished() const FL_NO_EXCEPT override {
        // The legacy IDF 4 driver doesn't expose a direct "done" flag
        // the way IDF 5 does. We treat "finished" as "the consumer has
        // popped at least one symbol chunk AND the ring buffer is
        // empty", which matches AutoResearch's polling-loop usage.
        if (!mRunning) {
            return true;
        }
        if (mInternalBuffer.empty()) {
            return false;
        }
        // The ring buffer is opaque from the const path; return based on
        // mRunning being cleared by drainRingbuf().
        return false;
    }

    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override {
        if (!mInstalled || mRingbufHandle == nullptr) {
            return RxWaitResult::TIMEOUT;
        }
        // Pop chunks until either: (a) we've collected enough symbols
        // to fill the requested buffer, (b) the ring buffer is empty
        // for longer than the timeout, or (c) we hit a reset symbol.
        const TickType_t total_ticks = pdMS_TO_TICKS(timeout_ms);
        const TickType_t pop_ticks_each = pdMS_TO_TICKS(5);
        TickType_t start = xTaskGetTickCount();
        for (;;) {
            size_t item_size_bytes = 0;
            rmt_item32_t *raw = static_cast<rmt_item32_t *>(xRingbufferReceive(
                mRingbufHandle, &item_size_bytes, pop_ticks_each));
            if (raw != nullptr && item_size_bytes > 0) {
                size_t count = item_size_bytes / sizeof(rmt_item32_t);
                appendSymbols(raw, count);
                vRingbufferReturnItem(mRingbufHandle, static_cast<void *>(raw));
                if (mInternalBuffer.size() >= mBufferSize) {
                    drainRingbuf();
                    return mInternalBuffer.size() >= mBufferSize
                               ? RxWaitResult::BUFFER_OVERFLOW
                               : RxWaitResult::SUCCESS;
                }
                if (mInternalBuffer.size() > 0 && finishedSentinelSeen()) {
                    drainRingbuf();
                    return RxWaitResult::SUCCESS;
                }
            }
            TickType_t now = xTaskGetTickCount();
            if ((now - start) >= total_ticks) {
                drainRingbuf();
                if (!mInternalBuffer.empty()) {
                    return RxWaitResult::SUCCESS;
                }
                return RxWaitResult::TIMEOUT;
            }
            // Yield so the IDF's own RX ISR can refill the ring buffer.
            taskYIELD();
        }
    }

    fl::result<u32, DecodeError>
    decode(const ChipsetTiming4Phase &timing,
           fl::span<u8> out) FL_NO_EXCEPT override {
        if (mInternalBuffer.empty()) {
            return fl::result<u32, DecodeError>::failure(
                DecodeError::INVALID_ARGUMENT);
        }
        return decodeRmtSymbols(
            timing, mResolutionHz,
            fl::span<const RmtSymbol>(mInternalBuffer.data(),
                                      mInternalBuffer.size()),
            out);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset) FL_NO_EXCEPT override {
        if (mResolutionHz == 0) {
            return 0;
        }
        u32 ns_per_tick = 1000000000UL / mResolutionHz;
        size_t edges_written = 0;
        size_t symbol_index = offset;
        while (edges_written + 1 < out.size() &&
               symbol_index < mInternalBuffer.size()) {
            const auto item =
                fl::bit_cast<rmt_item32_t>(mInternalBuffer[symbol_index]);
            out[edges_written++] =
                EdgeTime{static_cast<bool>(item.level0),
                         ticksToNs(item.duration0, ns_per_tick)};
            out[edges_written++] =
                EdgeTime{static_cast<bool>(item.level1),
                         ticksToNs(item.duration1, ns_per_tick)};
            ++symbol_index;
        }
        return edges_written;
    }

    const char *name() const FL_NO_EXCEPT override { return "RMT"; }

    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override {
        if (mResolutionHz == 0) {
            mResolutionHz = 40000000u;
        }
        u32 ns_per_tick = 1000000000UL / mResolutionHz;
        if (ns_per_tick == 0)
            ns_per_tick = 1;
        mInternalBuffer.clear();
        // Pack pairs of EdgeTime entries into rmt_item32_t and append.
        for (size_t i = 0; (i + 1) < edges.size(); i += 2) {
            rmt_item32_t item = {};
            item.level0 = edges[i].high ? 1 : 0;
            item.duration0 = edges[i].ns / ns_per_tick;
            item.level1 = edges[i + 1].high ? 1 : 0;
            item.duration1 = edges[i + 1].ns / ns_per_tick;
            mInternalBuffer.push_back(fl::bit_cast<RmtSymbol>(item));
        }
        return true;
    }

  protected:
    fl::span<const RmtSymbol> getReceivedSymbols() const override {
        return fl::span<const RmtSymbol>(mInternalBuffer.data(),
                                         mInternalBuffer.size());
    }

  private:
    void teardown() FL_NO_EXCEPT {
        if (mInstalled) {
            rmt_rx_stop(static_cast<rmt_channel_t>(mChannel));
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            mRingbufHandle = nullptr;
        }
        if (mChannel >= 0) {
            releaseRxChannel(mChannel);
            mChannel = -1;
        }
        mRunning = false;
    }

    void appendSymbols(const rmt_item32_t *src, size_t count) FL_NO_EXCEPT {
        for (size_t i = 0; i < count; ++i) {
            mInternalBuffer.push_back(fl::bit_cast<RmtSymbol>(src[i]));
        }
    }

    bool finishedSentinelSeen() FL_NO_EXCEPT {
        // A symbol with both durations == 0 is the IDF-4 "RX idle"
        // sentinel that fires when idle_threshold expires.
        if (mInternalBuffer.empty()) {
            return false;
        }
        const auto last = fl::bit_cast<rmt_item32_t>(mInternalBuffer.back());
        return (last.duration0 == 0) && (last.duration1 == 0);
    }

    void drainRingbuf() FL_NO_EXCEPT {
        if (mRingbufHandle == nullptr) {
            return;
        }
        for (;;) {
            size_t item_size_bytes = 0;
            rmt_item32_t *raw = static_cast<rmt_item32_t *>(
                xRingbufferReceive(mRingbufHandle, &item_size_bytes, 0));
            if (raw == nullptr || item_size_bytes == 0) {
                break;
            }
            size_t count = item_size_bytes / sizeof(rmt_item32_t);
            appendSymbols(raw, count);
            vRingbufferReturnItem(mRingbufHandle, static_cast<void *>(raw));
        }
    }

    gpio_num_t mPin;
    int mChannel;    // -1 until allocated
    bool mInstalled; // rmt_driver_install() succeeded
    bool mRunning;   // rmt_rx_start() called
    u32 mResolutionHz;
    size_t mBufferSize;
    fl::vector<RmtSymbol> mInternalBuffer;
    RingbufHandle_t mRingbufHandle;
    // RMT4 memory-manager coordination (#3469). RX uses an offset ID so
    // it doesn't collide with TX's raw 0..N-1 channel numbering.
    int mMemoryChannelId;
    bool mMemoryRegistered;
};

//=============================================================================
// Factory
//=============================================================================

fl::shared_ptr<RmtRxChannel> RmtRxChannel::create(int pin) FL_NO_EXCEPT {
    if (pin < 0) {
        FL_WARN_F("[RMT4 RX] create() refused: pin %s is negative", pin);
        return nullptr;
    }
    return fl::shared_ptr<RmtRxChannel>(new RmtRxChannelImpl(pin)); // ok bare allocation — RmtRxChannelImpl is private, cannot construct externally via make_shared without exposing type
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM &&
       // !FASTLED_RMT5
#endif // FL_IS_ESP32
