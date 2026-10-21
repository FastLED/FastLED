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
///   - TX and RX share the SDK ISR. RX capture is bounded by one
///     hardware memory block; a larger software ring buffer cannot
///     extend that classic-ESP32 hardware limit.
///
/// ## Decode helpers
///
/// The SDK can pair LOW/HIGH phases instead of HIGH/LOW. Convert symbols
/// to edges and use the tested, SDK-independent RMT4 decoder so leading
/// idle and packet-end truncation cannot shift or discard data bits.

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
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_4/rmt_rx_decode_4.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "driver/rmt.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "hal/gpio_ll.h"
#endif
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
// Convert captured hardware ticks into nanoseconds.
//=============================================================================

inline u32 ticksToNs(u32 ticks, u32 ns_per_tick) FL_NO_EXCEPT { return ticks * ns_per_tick; }

//=============================================================================
// Channel allocation
//
// Classic ESP32 has 8 RMT channels (0..7); ESP32-S2 has 4 (0..3).
// FastLED TX historically claims channels starting at 0; this RX
// allocator picks from the high end so a single TX+RX strip can
// coexist on the same chip without colliding.
//=============================================================================

#if defined(SOC_RMT_CHANNELS_PER_GROUP)
constexpr int kRmt4RxMaxChannels = SOC_RMT_CHANNELS_PER_GROUP;
#elif defined(FL_IS_ESP_32S2)
constexpr int kRmt4RxMaxChannels = 4;
#else
constexpr int kRmt4RxMaxChannels = 8;
#endif

static bool s_channel_in_use[kRmt4RxMaxChannels] = {false};

#if defined(SOC_RMT_TX_CANDIDATES_PER_GROUP) && \
    defined(SOC_RMT_RX_CANDIDATES_PER_GROUP) && \
    SOC_RMT_TX_CANDIDATES_PER_GROUP + SOC_RMT_RX_CANDIDATES_PER_GROUP == SOC_RMT_CHANNELS_PER_GROUP
constexpr int kRmt4RxFirstChannel = SOC_RMT_TX_CANDIDATES_PER_GROUP;
#else
constexpr int kRmt4RxFirstChannel = 0;
#endif

int allocateRxChannel() FL_NO_EXCEPT {
    // Reserve the physical block as well as the logical RX channel. A TX
    // allocation can span several consecutive channel memory blocks.
    for (int ch = kRmt4RxMaxChannels - 1; ch >= kRmt4RxFirstChannel; --ch) {
        if (!s_channel_in_use[ch] &&
            RmtMemoryManager4::instance().tryReserveBlocks(ch, 1)) {
            s_channel_in_use[ch] = true;
            return ch;
        }
    }
    return -1;
}

void releaseRxChannel(int channel) FL_NO_EXCEPT {
    if (channel >= 0 && channel < kRmt4RxMaxChannels) {
        if (s_channel_in_use[channel]) {
            RmtMemoryManager4::instance().releaseBlocks(channel, 1);
        }
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
          mCaptureOverflow(false),
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
        const size_t kMinRxSymbols = RmtMemoryManager4::instance().wordsPerBlock();
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
        mCaptureOverflow = false;
        // Re-arm path: if already installed, just clear our buffer and
        // restart RX. The driver stays installed across begin() calls
        // for the lifetime of the object.
        if (mInstalled) {
            rmt_rx_stop(static_cast<rmt_channel_t>(mChannel));
            drainRingbuf();
            mInternalBuffer.clear();
            mRunning = false;
            esp_err_t err = rmt_rx_start(static_cast<rmt_channel_t>(mChannel),
                                         /*rx_idx_rst=*/true);
            if (err != ESP_OK) {
                FL_WARN("[RMT4 RX] rmt_rx_start re-arm failed, err=" << err);
                return false;
            }
            mRunning = true;
            return true;
        }

        // Construction may have preceded another channel releasing memory.
        // Never start hardware unless its RX allocation is accounted for.
        if (!mMemoryRegistered) {
            size_t reserved = 0;
            if (!RmtMemoryManager4::instance().tryAllocateRx(
                    mMemoryChannelId, RmtMemoryManager4::instance().wordsPerBlock(), reserved)) {
                return false;
            }
            mMemoryRegistered = true;
        }

        // First-time init.
        if (mChannel < 0) {
            mChannel = allocateRxChannel();
            if (mChannel < 0) {
                FL_WARN("[RMT4 RX] no free RMT channel available");
                return false;
            }
        }

        // Resolve the optional clock and zero-valued buffer defaults.
        mResolutionHz = config.hz.value_or(40000000u); // Default: 40 MHz
        if (mResolutionHz == 0) {
            mResolutionHz = 40000000u;
        }
        // Classic RMT RX cannot stream beyond its configured hardware block.
        // AutoResearch's shared decode buffer can advertise hundreds of
        // thousands of symbols; allocating that as an SDK ring buffer would
        // exhaust the classic ESP32 heap without extending hardware capture.
        const size_t requested = config.buffer_size != 0 ? config.buffer_size : 1024;
        const size_t capacity = RmtMemoryManager4::instance().wordsPerBlock();
        mBufferSize = requested < capacity ? requested : capacity;

        // RMT clock divider so the RMT tick rate matches the requested
        // resolution. The APB clock is 80 MHz on classic ESP32 / S2.
        // Example: 40 MHz target -> divider = 2 -> 25 ns per tick.
        constexpr u32 kApbHz = 80000000u;
        u32 clk_div = kApbHz / mResolutionHz;
        if (clk_div == 0)
            clk_div = 1;
        if (clk_div > 255)
            clk_div = 255;
        mResolutionHz = kApbHz / clk_div;

        rmt_config_t cfg = {};
        cfg.rmt_mode = RMT_MODE_RX;
        cfg.channel = static_cast<rmt_channel_t>(mChannel);
        cfg.gpio_num = mPin;
        cfg.clk_div = static_cast<u8>(clk_div);
        cfg.mem_block_num = 1;
        // RX-mode config: how long without an edge before "done", and
        // optional noise-filter threshold. The numbers below mirror
        // the IDF 5 impl's behaviour for WS2812B-family capture.
        const u64 idle_ticks = static_cast<u64>(config.signal_range_max_ns) *
                               mResolutionHz / 1000000000ULL;
        cfg.rx_config.idle_threshold = static_cast<u16>(
            idle_ticks == 0 ? 1 : (idle_ticks > 32767 ? 32767 : idle_ticks));
        // The IDF4 glitch filter counts APB cycles, not divided RX ticks.
        const u64 filter_ticks = static_cast<u64>(config.signal_range_min_ns) *
                                 kApbHz / 1000000000ULL;
        cfg.rx_config.filter_ticks_thresh = static_cast<u8>(
            filter_ticks > 255 ? 255 : filter_ticks);
        cfg.rx_config.filter_en = filter_ticks != 0;

        esp_err_t err = rmt_config(&cfg);
        if (err != ESP_OK) {
            FL_WARN("[RMT4 RX] rmt_config failed on channel " << mChannel << " pin " << (static_cast<int>(mPin)) << ", err=" << err);
            teardown();
            return false;
        }

        // Install the IDF driver and let it own the RX ring buffer. The
        // rx_buf_size argument is in BYTES. We size for the requested
        // symbol count (each rmt_item32_t is 4 bytes).
        // Leave room for the ring-buffer item header and a full hardware frame.
        size_t rx_buf_bytes = 2 * (capacity + 4) * sizeof(rmt_item32_t);
        err = rmt_driver_install(static_cast<rmt_channel_t>(mChannel),
                                 rx_buf_bytes, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
        if (err != ESP_OK) {
            FL_WARN("[RMT4 RX] rmt_driver_install failed on channel " << mChannel << ", err=" << err);
            teardown();
            return false;
        }
        mInstalled = true;

        // Cache the ring buffer handle so wait() can pull from it.
        err = rmt_get_ringbuf_handle(static_cast<rmt_channel_t>(mChannel),
                                     &mRingbufHandle);
        if (err != ESP_OK || mRingbufHandle == nullptr) {
            FL_WARN("[RMT4 RX] rmt_get_ringbuf_handle failed on channel " << mChannel << ", err=" << err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            teardown();
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
            FL_WARN("[RMT4 RX] rmt_set_gpio/pin failed on channel " << mChannel << " pin " << (static_cast<int>(mPin)) << ", err=" << err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            teardown();
            return false;
        }

        if (config.io_loop_back) {
            // rmt_config(RX) makes the pin input-only. Keep its output enabled
            // without gpio_set_direction(), which resets the output matrix.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
            gpio_ll_output_enable(&GPIO, mPin);
#else
            err = gpio_set_direction(mPin, GPIO_MODE_INPUT_OUTPUT);
            if (err != ESP_OK) {
                rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
                mInstalled = false;
                teardown();
                return false;
            }
#endif
        }

        // Arm the receiver.
        err = rmt_rx_start(static_cast<rmt_channel_t>(mChannel),
                           /*rx_idx_rst=*/true);
        if (err != ESP_OK) {
            FL_WARN("[RMT4 RX] rmt_rx_start failed on channel " << mChannel << ", err=" << err);
            rmt_driver_uninstall(static_cast<rmt_channel_t>(mChannel));
            mInstalled = false;
            teardown();
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
            if (raw != nullptr) {
                size_t count = item_size_bytes / sizeof(rmt_item32_t);
                appendSymbols(raw, count);
                // IDF4 also queues empty idle packets. Every received item
                // must be returned, including those with a zero-byte payload.
                vRingbufferReturnItem(mRingbufHandle, static_cast<void *>(raw));
                if (count > 0) {
                    // Each non-empty SDK RX item is a complete capture ending
                    // at idle or hardware capacity, rather than a stream chunk.
                    rmt_rx_stop(static_cast<rmt_channel_t>(mChannel));
                    mRunning = false;
                    mCaptureOverflow = mInternalBuffer.size() >= mBufferSize;
                    return mCaptureOverflow
                               ? RxWaitResult::BUFFER_OVERFLOW : RxWaitResult::SUCCESS;
                }
            }
            TickType_t now = xTaskGetTickCount();
            if ((now - start) >= total_ticks) {
                rmt_rx_stop(static_cast<rmt_channel_t>(mChannel));
                mRunning = false;
                drainRingbuf();
                if (mInternalBuffer.size() >= mBufferSize) {
                    mCaptureOverflow = true;
                    return RxWaitResult::BUFFER_OVERFLOW;
                }
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
        if (mCaptureOverflow) {
            return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
        fl::vector<EdgeTime> edges;
        edges.resize(mInternalBuffer.size() * 2);
        const size_t count = getRawEdgeTimes(edges, 0);
        return detail::decodeRmt4Edges(
            timing, fl::span<const EdgeTime>(edges.data(), count), out);
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

    int getPin() const FL_NO_EXCEPT override { return static_cast<i32>(mPin); }

    u32 getResolutionHz() const FL_NO_EXCEPT override { return mResolutionHz; }

    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override {
        mCaptureOverflow = false;
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
    fl::span<const RmtSymbol> getReceivedSymbols() const FL_NO_EXCEPT override {
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

    void drainRingbuf() FL_NO_EXCEPT {
        if (mRingbufHandle == nullptr) {
            return;
        }
        for (;;) {
            size_t item_size_bytes = 0;
            rmt_item32_t *raw = static_cast<rmt_item32_t *>(
                xRingbufferReceive(mRingbufHandle, &item_size_bytes, 0));
            if (raw == nullptr) {
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
    bool mCaptureOverflow;
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
        FL_WARN("[RMT4 RX] create() refused: pin " << pin << " is negative");
        return nullptr;
    }
    return fl::make_shared<RmtRxChannelImpl>(pin);
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM &&
       // !FASTLED_RMT5
#endif // FL_IS_ESP32
