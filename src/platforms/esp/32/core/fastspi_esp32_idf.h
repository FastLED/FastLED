#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"

// ESP32 Hardware SPI implementation using native ESP-IDF driver
// This file uses driver/spi_master.h for pure ESP-IDF builds without Arduino framework

#include "crgb.h"
#include "fastspi_types.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/cstring.h"  // For fl::memset
#include "fl/log/log.h"  // For FL_WARN macro

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/spi_master.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "driver/gpio.h"
// IWYU pragma: end_keep
#include "esp_heap_caps.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/spi_bus.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
FL_EXTERN_C_END

// ---------------------------------------------------------------------------
// writeByte() below used to issue one spi_device_polling_transmit() call PER
// BYTE - a real hardware transaction, with its own fixed per-transaction
// overhead, for every single byte written. FASTLED_ESP32_SPI_BULK_TRANSFER
// has no effect on this for clocked-SPI chipsets (APA102, SK9822, WS2801,
// LPD8806, P9813, ...) that write via the select()/writeByte()/writeWord()/
// release() sequence directly (e.g. APA102Controller::showPixelsDefault() in
// fl/chipsets/apa102.h) rather than through the generic writePixels<>()/
// writePixelsBulk() path a few hundred lines below - that bulk path is
// unaffected by this change and still writes one polling transaction per
// pixel-block as before.
//
// Measured on a 128-LED/column ESP32-C5 APA102 "POV" (persistence-of-vision)
// display, calling FastLED.show() once per column: one polling_transmit()
// per byte limited this to ~7fps even with FASTLED_ESP32_SPI_BULK_TRANSFER
// enabled, since that display's chipset never took the bulk path at all.
//
// Fix: ESP32SPIOutput now buffers an entire select()...release() window
// (whatever sequence of writeByte()/writeWord() calls happens in between)
// into one of several DMA-capable RAM buffers, and release() fires it as a
// single spi_device_queue_trans() call - one real DMA transaction per frame
// instead of one blocking transaction per byte. Buffers are ping-ponged
// (FL_ESP32_SPI_NUM_BUFFERS deep, default 2): select() advances to the
// next buffer and waits only if THAT buffer's own prior transaction hasn't
// completed yet, so the CPU can fill buffer N+1 while buffer N's DMA
// transfer is still running in hardware - removing fill time from the
// critical path once buffering is deep enough to cover it. On the same
// hardware this raised the display well above its original ~7fps (bound by
// the LED strip's own serial protocol timing rather than the SPI driver
// once per-byte transaction overhead was removed).
//
// spi_device_acquire_bus()/spi_device_release_bus() (previously held across
// select()/release() to keep each frame's individual polling_transmit()
// calls from being interleaved with another device sharing the bus) are no
// longer needed: there is now only ever one spi_device_queue_trans() call
// per frame, and the SPI driver already serializes transactions against a
// single device handle on its own.
// ---------------------------------------------------------------------------

// Max bytes buffered per select()...release() window before being sent as
// one DMA transaction. Must cover whatever the heaviest caller writes in a
// single frame - for APA102/SK9822, that is 4 start-frame bytes + 4
// bytes/LED + ~4 bytes per 32 LEDs of end-frame clock-out. Default covers
// roughly 1000 LEDs; override before #include <FastLED.h> if you need more.
// A frame that exceeds this is truncated (and warned about once per
// occurrence - see release()), not silently dropped without any indication.
#ifndef FL_ESP32_SPI_DMA_BUFFER_BYTES
#define FL_ESP32_SPI_DMA_BUFFER_BYTES 4096
#endif

// How many buffers (= how many frames/transactions deep) to pre-allocate.
// Default of 2 matches a simple double-buffered design; override before
// #include <FastLED.h> to go deeper (trades DMA-capable RAM - this many
// buffers times FL_ESP32_SPI_DMA_BUFFER_BYTES - for more tolerance of
// scheduling delays between fills, e.g. under a busy WiFi/BLE radio task).
#ifndef FL_ESP32_SPI_NUM_BUFFERS
#define FL_ESP32_SPI_NUM_BUFFERS 2
#endif

// Uncomment (or -D) to have release() track how long spi_device_queue_trans()
// itself takes vs. the whole select()-to-release() window, printing a summary
// every FL_ESP32_SPI_DMA_TIMING_PERIOD frames. Lets you tell apart "DMA
// transfer is slow" (bus/hardware contention) from "something outside the
// transfer call is slow" (task scheduling / other work stealing CPU time
// between frames).
#ifndef FL_ESP32_SPI_DMA_TIMING_PERIOD
#define FL_ESP32_SPI_DMA_TIMING_PERIOD 640
#endif

// Where FL_ESP32_SPI_DMA_DEBUG/_TIMING output goes - both are opt-in
// and off by default, so this has no effect until one of them is defined.
// Defaults to a no-op rather than Serial directly: this header is compiled
// into the ESP32 lazy-Serial0 unity build (src/platforms/esp/32), where any
// direct `Serial.` reference would be weakened by the linker along with
// every other reference in that translation unit and could resolve to null
// at runtime (see ci/tests/test_esp32_lazy_serial0.py). Override this
// before #include <FastLED.h> - from your own sketch, not from a file in
// this unity build - to actually see DMA_DEBUG/DMA_TIMING output, e.g.
// `#define FL_ESP32_SPI_DMA_LOG(...) Serial.printf(__VA_ARGS__)`.
#ifndef FL_ESP32_SPI_DMA_LOG
#define FL_ESP32_SPI_DMA_LOG(...) ((void)0)
#endif

namespace fl {

#if !ESP_IDF_VERSION_4_OR_HIGHER
#define SPI2_HOST HSPI_HOST
#define SPI3_HOST VSPI_HOST
#endif

// Determine default SPI host based on chip variant
#if defined(FL_IS_ESP_32S2) || defined(FL_IS_ESP_32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
    #define FASTLED_ESP32_DEFAULT_SPI_HOST SPI2_HOST
#else
    // ESP32 classic - use SPI2_HOST (aka VSPI)
    #define FASTLED_ESP32_DEFAULT_SPI_HOST SPI2_HOST
#endif

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_SPEED>
class ESP32SPIOutput {
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;
    Selectable* mPSelect;
    bool mInitialized;

    // Ping-pong DMA buffer ring - see the file-level comment above for why.
    static const int NUM_BUFFERS = FL_ESP32_SPI_NUM_BUFFERS;
    uint8_t* mBuffer[NUM_BUFFERS] = {};
    size_t mBufferLen = 0;              // fill position in the buffer CURRENTLY being written (between select() and release())
    int mCurrentBuffer = 0;             // which of mBuffer[] is being written - cycles 0..NUM_BUFFERS-1, see select()
    spi_transaction_t mTrans[NUM_BUFFERS] = {};  // one persistent transaction descriptor per buffer - must outlive the queued transaction
    bool mPending[NUM_BUFFERS] = {};    // does mBuffer[i] have a transaction still queued/in-flight?
    bool mOverflowDropped = false;      // true if writeByte() had to drop a byte since the last select() - see writeByte()/release()

#ifdef FL_ESP32_SPI_DMA_TIMING
    uint32_t mSelectMicros = 0;
    uint32_t mFrameCount = 0;
    uint64_t mWaitMicrosSum = 0;
    uint32_t mWaitMicrosMax = 0;
    uint64_t mQueueMicrosSum = 0;
    uint32_t mQueueMicrosMax = 0;
    uint64_t mWallMicrosSum = 0;
    uint32_t mWallMicrosMax = 0;
#endif

    // Drains every outstanding queued transaction, matching each one back to
    // the buffer it actually belongs to by comparing the descriptor pointer
    // ESP-IDF hands back against &mTrans[i] - spi_device_get_trans_result()
    // returns transactions in actual hardware-completion order, which is NOT
    // necessarily the same as buffer index order (it only matches index
    // order within one uninterrupted ping-pong cycle - it breaks at the
    // wrap-around point, buffer NUM_BUFFERS-1 back to buffer 0, which
    // select() itself never has to worry about since it only ever waits on
    // the one slot it's about to reuse, but any caller scanning every index
    // in ascending order - as an earlier version of this function did - can
    // wait for buffer 1's completion while mistakenly crediting it to buffer
    // 0, freeing buffer 0's memory while its own transaction is still
    // genuinely in flight. Also used by writePixelsBulk() below, which must
    // not mix a polling transmit with any still-queued transaction.
    void drainAllPending() FL_NO_EXCEPT {
        if (mSPIHandle == nullptr) {
            return;
        }
        int pendingCount = 0;
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (mPending[i]) pendingCount++;
        }
        for (int n = 0; n < pendingCount; n++) {
            spi_transaction_t* rtrans = nullptr;
            spi_device_get_trans_result(mSPIHandle, &rtrans, portMAX_DELAY);
            for (int i = 0; i < NUM_BUFFERS; i++) {
                if (&mTrans[i] == rtrans) {
                    mPending[i] = false;
                    break;
                }
            }
        }
    }

    // Drains every outstanding transaction (see drainAllPending() above) and
    // frees every allocated buffer. Safe to call unconditionally - whether
    // called after a partial allocation failure (mSPIHandle is still
    // nullptr, so drainAllPending() is a no-op and this just frees whatever
    // succeeded) or from the destructor (drains for real). Leaves mBuffer[]
    // all nullptr.
    void freeBuffers() FL_NO_EXCEPT {
        drainAllPending();
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (mBuffer[i] != nullptr) {
                heap_caps_free(mBuffer[i]);
                mBuffer[i] = nullptr;
            }
        }
    }

public:
    // Verify that the pins are valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "Invalid data pin specified");
    FL_STATIC_ASSERT(FastPin<CLOCK_PIN>::validpin(), "Invalid clock pin specified");

    ESP32SPIOutput() FL_NO_EXCEPT
        : mSPIHandle(nullptr)
        , mHost(defaultHost())
        , mPSelect(nullptr)
        , mInitialized(false)
    {}

    ESP32SPIOutput(Selectable* pSelect) FL_NO_EXCEPT
        : mSPIHandle(nullptr)
        , mHost(defaultHost())
        , mPSelect(pSelect)
        , mInitialized(false)
    {}

    ~ESP32SPIOutput() {
        // Unconditional - covers both a fully-initialized instance and one
        // left with allocated-but-unused buffers after a failed init()
        // (which returns before setting mInitialized, so the old version of
        // this destructor never freed them).
        freeBuffers();
        if (mInitialized) {
            if (mSPIHandle) {
                spi_bus_remove_device(mSPIHandle);
                mSPIHandle = nullptr;
            }
            spi_bus_free(mHost);
            mInitialized = false;
        }
    }

    void setSelect(Selectable* pSelect) FL_NO_EXCEPT {
        mPSelect = pSelect;
    }

    void setBus(Esp32SpiBus bus) FL_NO_EXCEPT {
        switch (bus) {
            case Esp32SpiBus::HOST2:
                mHost = SPI2_HOST;
                break;
            case Esp32SpiBus::HOST3:
#if SOC_SPI_PERIPH_NUM > 2
                mHost = SPI3_HOST;
#else
                FL_WARN("SPI3 is unavailable on this ESP32 target; using the default SPI host");
                mHost = defaultHost();
#endif
                break;
            case Esp32SpiBus::AUTO:
                mHost = defaultHost();
                break;
        }
    }

    static spi_host_device_t defaultHost() FL_NO_EXCEPT {
#ifdef FASTLED_ESP32_SPI_BUS
        return static_cast<spi_host_device_t>(FASTLED_ESP32_SPI_BUS);
#else
        return FASTLED_ESP32_DEFAULT_SPI_HOST;
#endif
    }

    void init() FL_NO_EXCEPT {
        if (mInitialized) {
            return;
        }

        // Local, not a member: the only state any OTHER method needs to
        // check is mInitialized (false until every step below - allocation,
        // bus init, device add - has succeeded). An earlier version of this
        // code kept this as a member (mAllocFailed) and had writeByte() key
        // off it instead of mInitialized - which missed that the bus-init
        // and device-add failure paths below also null out mBuffer[]
        // without ever touching that flag, so writeByte() would still pass
        // its guard and write through a null pointer. Making this local
        // removes any chance of that mismatch recurring.
        bool allocFailed = false;
        for (int i = 0; i < NUM_BUFFERS; i++) {
            mBuffer[i] = (uint8_t*) heap_caps_malloc(FL_ESP32_SPI_DMA_BUFFER_BYTES, MALLOC_CAP_DMA);
            if (mBuffer[i] == nullptr) {
                allocFailed = true;
            }
        }
        if (allocFailed) {
            // Loud and impossible to miss - bail out here rather than
            // continuing into spi_bus_initialize()/spi_bus_add_device().
            // mInitialized stays false, so writeByte()/select()/release()/
            // writePixelsBulk() all correctly no-op until a later init()
            // call succeeds.
            FL_WARN("ESP32SPIOutput: failed to allocate one or more of "
                    << NUM_BUFFERS << " DMA buffers ("
                    << FL_ESP32_SPI_DMA_BUFFER_BYTES << " bytes each, "
                    << (NUM_BUFFERS * FL_ESP32_SPI_DMA_BUFFER_BYTES)
                    << " bytes total) - LED output disabled for this boot.");
            freeBuffers();
            return;
        }

        // Configure SPI bus
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DATA_PIN;
        bus_config.miso_io_num = -1;  // Not used for LED strips
        bus_config.sclk_io_num = CLOCK_PIN;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = FL_ESP32_SPI_DMA_BUFFER_BYTES;
        bus_config.flags = SPICOMMON_BUSFLAG_MASTER;

        // Initialize bus with auto DMA
#if ESP_IDF_VERSION_4_OR_HIGHER
        const int dma_channel = SPI_DMA_CH_AUTO;
#else
        // IDF 3.3 has no automatic allocation. HSPI and VSPI use distinct
        // DMA channels with the same numeric IDs as their host constants.
        const int dma_channel = static_cast<i32>(mHost);
#endif
        esp_err_t ret = spi_bus_initialize(mHost, &bus_config, dma_channel);
        if (ret == ESP_ERR_INVALID_STATE) {
            // Some other device already initialized this bus (e.g. a second
            // strip sharing the clock pin) - fine, attach to it, but that
            // first initializer is also the one whose DATA_PIN/CLOCK_PIN
            // actually won: if this instance's pins differ, its output
            // silently goes to the WRONG pins. Not something this class can
            // detect on its own (the bus config that "won" isn't exposed
            // back to us), so just make the shared-bus assumption visible
            // rather than silent.
            FL_WARN("SPI bus already initialized by another device - "
                    "attaching to the existing bus. If this strip's "
                    "DATA_PIN/CLOCK_PIN differ from whichever controller "
                    "initialized the bus first, its output will go to the "
                    "wrong pins.");
        } else if (ret != ESP_OK) {
            FL_WARN("SPI bus init failed: " << ret);
            freeBuffers();
            return;
        }

        // Configure device
        spi_device_interface_config_t dev_config = {};
        dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
        dev_config.clock_speed_hz = SPI_SPEED;
        dev_config.spics_io_num = -1;  // No CS pin for LED strips
        // NUM_BUFFERS, not 1: double/multi-buffering needs room for one
        // transaction that's still in flight (or done but not yet collected)
        // PLUS one freshly queued behind it - spi_device_queue_trans() would
        // block once the queue is full, defeating the point of queuing
        // asynchronously.
        dev_config.queue_size = NUM_BUFFERS;
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only

        // Add device to bus
        ret = spi_bus_add_device(mHost, &dev_config, &mSPIHandle);
        if (ret != ESP_OK) {
            FL_WARN("SPI add device failed: " << ret);
            freeBuffers();
            return;
        }

        mInitialized = true;
    }

    static void stop() FL_NO_EXCEPT {}
    static void wait() FL_NO_EXCEPT {}
    static void waitFully() FL_NO_EXCEPT {}

    void writeByteNoWait(u8 b) FL_NO_EXCEPT __attribute__((always_inline)) {
        writeByte(b);
    }

    void writeBytePostWait(u8 b) FL_NO_EXCEPT __attribute__((always_inline)) {
        writeByte(b);
        wait();
    }

    void writeWord(u16 w) FL_NO_EXCEPT __attribute__((always_inline)) {
        writeByte(static_cast<u8>(w >> 8));
        writeByte(static_cast<u8>(w & 0xFF));
    }

    // Buffers a byte into the current frame - actual SPI write happens in
    // release(). mInitialized guard: this is the one flag that's false in
    // every case where mBuffer[] might not be safely writable - a failed
    // allocation, a failed spi_bus_initialize()/spi_bus_add_device(), or
    // init() simply never having been called - all of them leave
    // mInitialized false. Without this guard, a chipset controller calling
    // writeByte()/writeWord() unconditionally every frame regardless of
    // whether init() ever succeeded would otherwise hit a null-pointer
    // write. If the frame is bigger than FL_ESP32_SPI_DMA_BUFFER_BYTES,
    // bytes past that are dropped (the alternative - growing the buffer or
    // splitting into extra transactions mid-frame - isn't attempted here)
    // but release() warns once per occurrence rather than truncating
    // silently.
    void writeByte(u8 b) FL_NO_EXCEPT __attribute__((always_inline)) {
        if (mInitialized && mBufferLen < FL_ESP32_SPI_DMA_BUFFER_BYTES) {
            mBuffer[mCurrentBuffer][mBufferLen++] = b;
        } else if (mInitialized) {
            mOverflowDropped = true;
        }
    }

    // Still a single immediate polling transmit against mSPIHandle, used
    // only by the generic writePixels<>() bulk path below
    // (FASTLED_ESP32_SPI_BULK_TRANSFER), not by chipsets that write via
    // select()/writeByte()/writeWord()/release() directly. ESP-IDF does not
    // support a polling transmit while any queued (spi_device_queue_trans())
    // transaction against the same device is still outstanding, so this
    // drains every pending ring-buffer transaction first - select() only
    // drains the ONE slot it's about to reuse, which isn't enough here.
    void writePixelsBulk(const CRGB* pixels, size_t n) FL_NO_EXCEPT {
        if (!mInitialized || !mSPIHandle || n == 0) {
            return;
        }

        drainAllPending();

        const u8* data = fl::bit_cast<const u8*>(pixels);
        size_t n_bytes = n * 3;

        spi_transaction_t t = {};
        t.length = n_bytes * 8;  // Convert to bits
        t.tx_buffer = data;

        spi_device_polling_transmit(mSPIHandle, &t);
    }

    // Begin a new frame: advances to the NEXT buffer in the ring (wrapping
    // after NUM_BUFFERS) and, if THAT buffer still has a transaction
    // outstanding from NUM_BUFFERS-1 frames ago, waits for it to finish
    // before letting new writeByte() calls overwrite it. By the time we get
    // back around to a given buffer, its transfer has usually already
    // completed during the other buffers' fill+transmit time, so this wait
    // is normally short - deeper NUM_BUFFERS means more real time passes,
    // and so more margin, before a given slot is needed again.
    void select() FL_NO_EXCEPT {
        if (!mInitialized) {
            return;
        }
        mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
#ifdef FL_ESP32_SPI_DMA_TIMING
        uint32_t waitT0 = micros();
#endif
        if (mPending[mCurrentBuffer]) {
            spi_transaction_t* rtrans;
            spi_device_get_trans_result(mSPIHandle, &rtrans, portMAX_DELAY);
            mPending[mCurrentBuffer] = false;
        }
#ifdef FL_ESP32_SPI_DMA_TIMING
        uint32_t waitT1 = micros();
        uint32_t waitUs = waitT1 - waitT0;
        mWaitMicrosSum += waitUs;
        if (waitUs > mWaitMicrosMax) mWaitMicrosMax = waitUs;
        mSelectMicros = waitT1;
#endif
        mBufferLen = 0;
        if (mPSelect != nullptr) {
            mPSelect->select();
        }
    }

    // End of frame: queue everything buffered since select() as a single
    // async DMA transaction and return immediately - does NOT wait for it to
    // finish (that wait, for THIS buffer, happens at the top of its next
    // select() call instead - see above).
    void release() FL_NO_EXCEPT {
        if (mOverflowDropped) {
            FL_WARN("ESP32SPIOutput: frame exceeded FL_ESP32_SPI_DMA_BUFFER_BYTES ("
                    << FL_ESP32_SPI_DMA_BUFFER_BYTES << " bytes) - truncated. "
                    "Increase FL_ESP32_SPI_DMA_BUFFER_BYTES to fit your strip length.");
            mOverflowDropped = false;
        }
        if (mInitialized && mSPIHandle != nullptr && mBufferLen > 0) {
            spi_transaction_t& trans = mTrans[mCurrentBuffer];
            trans = {};
            trans.length = mBufferLen * 8;
            trans.tx_buffer = mBuffer[mCurrentBuffer];
            trans.rx_buffer = nullptr;
#ifdef FL_ESP32_SPI_DMA_TIMING
            uint32_t t0 = micros();
            esp_err_t err = spi_device_queue_trans(mSPIHandle, &trans, portMAX_DELAY);
            uint32_t t1 = micros();
            uint32_t queueUs = t1 - t0;
            uint32_t wallUs = t1 - mSelectMicros;
            mPending[mCurrentBuffer] = (err == ESP_OK);
            mFrameCount++;
            mQueueMicrosSum += queueUs;
            if (queueUs > mQueueMicrosMax) mQueueMicrosMax = queueUs;
            mWallMicrosSum += wallUs;
            if (wallUs > mWallMicrosMax) mWallMicrosMax = wallUs;
            if (mFrameCount >= FL_ESP32_SPI_DMA_TIMING_PERIOD) {
                FL_ESP32_SPI_DMA_LOG("[ESP32SPIOutput] timing over %u frames: wait avg=%uus max=%uus | queue avg=%uus max=%uus | wall (select-to-release, i.e. per-frame period) avg=%uus max=%uus\n",
                    (unsigned)mFrameCount,
                    (unsigned)(mWaitMicrosSum / mFrameCount), (unsigned)mWaitMicrosMax,
                    (unsigned)(mQueueMicrosSum / mFrameCount), (unsigned)mQueueMicrosMax,
                    (unsigned)(mWallMicrosSum / mFrameCount), (unsigned)mWallMicrosMax);
                mFrameCount = 0;
                mWaitMicrosSum = 0;
                mWaitMicrosMax = 0;
                mQueueMicrosSum = 0;
                mQueueMicrosMax = 0;
                mWallMicrosSum = 0;
                mWallMicrosMax = 0;
            }
#else
            esp_err_t err = spi_device_queue_trans(mSPIHandle, &trans, portMAX_DELAY);
            mPending[mCurrentBuffer] = (err == ESP_OK);
#endif
#ifdef FL_ESP32_SPI_DMA_DEBUG
            static size_t s_lastLoggedLen = (size_t)-1;
            static int s_framesLogged = 0;
            if (mBufferLen != s_lastLoggedLen) {
                if (s_framesLogged < 20) {
                    FL_ESP32_SPI_DMA_LOG("[ESP32SPIOutput] release(): buffer=%d bufferLen=%u bits=%u\n",
                        mCurrentBuffer, (unsigned)mBufferLen, (unsigned)trans.length);
                    s_framesLogged++;
                }
                s_lastLoggedLen = mBufferLen;
            }
#endif
            mBufferLen = 0;
        }
        if (mPSelect != nullptr) {
            mPSelect->release();
        }
    }

    void endTransaction() FL_NO_EXCEPT {
        waitFully();
        release();
    }

    void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
        select();
        writeBytesValueRaw(value, len);
        release();
    }

    void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
        while (len--) {
            writeByte(value);
        }
    }

    template <class D>
    void writeBytes(FASTLED_REGISTER u8* data, int len) FL_NO_EXCEPT {
        select();
        u8* end = data + len;
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len, this);
        release();
    }

    void writeBytes(FASTLED_REGISTER u8* data, int len) FL_NO_EXCEPT {
        writeBytes<DATA_NOP>(data, len);
    }

    static void finalizeTransmission() FL_NO_EXCEPT {}

    template <u8 BIT>
    inline void writeBit(u8 b) FL_NO_EXCEPT {
        // Test bit BIT in value b, send 0xFF if set, 0x00 if clear
        writeByte((b & (1 << BIT)) ? 0xFF : 0x00);
    }

    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    FL_NO_INLINE void writePixels(PixelController<RGB_ORDER> pixels, void* context) FL_NO_EXCEPT {
        #if FASTLED_ESP32_SPI_BULK_TRANSFER
        select();
        int len = pixels.mLen;
        CRGB data_block[FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE] = {0};
        size_t data_block_index = 0;

        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            if (data_block_index >= FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE) {
                writePixelsBulk(data_block, data_block_index);
                data_block_index = 0;
            }
            CRGB rgb(
                D::adjust(pixels.loadAndScale0()),
                D::adjust(pixels.loadAndScale1()),
                D::adjust(pixels.loadAndScale2())
            );
            data_block[data_block_index++] = rgb;
            pixels.advanceData();
            pixels.stepDithering();
        }
        if (data_block_index > 0) {
            writePixelsBulk(data_block, data_block_index);
            data_block_index = 0;
        }
        D::postBlock(len, context);
        release();
        #else
        select();
        int len = pixels.mLen;
        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            writeByte(D::adjust(pixels.loadAndScale0()));
            writeByte(D::adjust(pixels.loadAndScale1()));
            writeByte(D::adjust(pixels.loadAndScale2()));
            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len, context);
        release();
        #endif
    }
};

}  // namespace fl

FL_DISABLE_WARNING_POP
