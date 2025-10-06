# RMT5 Low-Level Double Buffer Implementation Strategy

## Executive Summary

This document outlines a comprehensive strategy for implementing RMT4's double-buffer ping-pong mechanism in RMT5 by using ESP-IDF's low-level RMT TX API directly, bypassing the high-level `led_strip` API. This approach provides:

1. **Double buffering** - Eliminates Wi-Fi interference through ping-pong buffer refills
2. **Worker pooling** - Supports N > K strips (where K = hardware channel count)
3. **Multiple flicker-mitigation strategies** - Double-buffer, high-priority ISR, or one-shot encoding
4. **Memory efficiency** - Detachable buffers eliminate allocation churn

## Problem Statement

### Current RMT5 Limitations

**Fire-and-Forget Transmission** (via `led_strip` API):
- Single buffer with no manual refill control
- No equivalent to RMT4's `fillNext()` interrupt-driven refill
- Buffer underruns during Wi-Fi interference cause flickering
- One-to-one controller-to-channel mapping limits scalability

**Hardware Channel Constraints**:
- **ESP32**: 8 RMT TX channels maximum
- **ESP32-S2/S3**: 4 RMT TX channels maximum
- **ESP32-C3/C6/H2**: 2 RMT TX channels maximum

**Wi-Fi Interference Problem**:
- Wi-Fi interrupts run at high priority (typically level 4)
- RMT interrupts run at default priority (level 3)
- Wi-Fi can delay RMT buffer refills by 50-120µs
- WS2812B timing tolerance is only ±150ns
- Result: Buffer underruns and visible flickering

### RMT4's Advantages

**Double-Buffered Architecture**:
- 2 × 64 words = 128 words per channel
- Interrupt fires at 50% mark to refill next half
- Continuous streaming prevents underruns
- Manual control via `fillNext()` in interrupt handler

**Worker Pool for Scalability**:
- K workers (hardware channels) serve N controllers
- Controllers don't own channels - they borrow workers temporarily
- Automatic recycling enables unlimited strips (N > K)

## RMT4 Double Buffer Mechanism

### Memory Layout

```
Channel 0: [Block 0: 64 words] [Block 1: 64 words]
           ├─── Half 0 ───┤├─── Half 1 ───┤

Transmission sequence:
1. Fill both halves initially
2. Start transmission from Half 0
3. Interrupt fires at 50% (32 words sent)
4. ISR refills Half 0 while Half 1 transmits
5. Interrupt fires again at next 50% mark
6. ISR refills Half 1 while Half 0 transmits
7. Repeat until all pixel data sent
```

### Key Implementation (RMT4)

```cpp
// Initial fill (idf4_rmt_impl.cpp:554-556)
fillNext(false);  // Fill Half 0
fillNext(false);  // Fill Half 1

// Interrupt handler (idf4_rmt_impl.cpp:850-857)
if (intr_st & BIT(tx_next_bit)) {
    portENTER_CRITICAL_ISR(&rmt_spinlock);
    pController->fillNext(true);  // Refill completed half
    RMT.int_clr.val |= BIT(tx_next_bit);
    portEXIT_CRITICAL_ISR(&rmt_spinlock);
}

// Buffer refill logic (idf4_rmt_impl.cpp:877-942)
void ESP32RMTController::fillNext(bool check_time) {
    for (int i = 0; i < PULSES_PER_FILL / 8; i++) {
        if (mCur < mSize) {
            convert_byte_to_rmt(mPixelData[mCur], zero_val, one_val, pItem);
            pItem += 8;
            mCur++;
        } else {
            pItem->val = 0;  // End marker
            pItem++;
        }
    }

    // Flip to other half
    mWhichHalf = (mWhichHalf + 1) % 2;
    if (mWhichHalf == 0) pItem = mRMT_mem_start;
    mRMT_mem_ptr = pItem;
}
```

## ESP-IDF v5 Low-Level RMT API

### Key Components

1. **RMT Channel** (`rmt_channel_handle_t`)
   - Direct hardware channel allocation
   - Configurable memory blocks, resolution, DMA

2. **RMT Encoder** (`rmt_encoder_handle_t`)
   - Converts pixel data to RMT symbols
   - Can be invoked incrementally for ping-pong

3. **Transaction Callbacks** (`rmt_tx_event_callbacks_t`)
   - `on_trans_done` - Transmission complete
   - Runs in ISR context

4. **Direct Memory Access** (`RMTMEM.chan[channel]`)
   - Available in IDF v5
   - Allows manual buffer manipulation like RMT4

### Relevant APIs

```cpp
// Channel creation
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *config,
                               rmt_channel_handle_t *ret_chan);

// Encoder creation (custom encoder needed)
esp_err_t rmt_new_encoder(const rmt_encoder_config_t *config,
                           rmt_encoder_handle_t *ret_encoder);

// Enable channel and transmit
esp_err_t rmt_enable(rmt_channel_handle_t channel);
esp_err_t rmt_transmit(rmt_channel_handle_t channel, ...);

// Register callbacks
esp_err_t rmt_tx_register_event_callbacks(rmt_channel_handle_t channel,
                                           const rmt_tx_event_callbacks_t *cbs,
                                           void *user_data);
```

## Low-Level Double Buffer Implementation

### Custom WS2812 Encoder

The encoder converts pixel bytes to RMT symbols on-demand, supporting incremental encoding for double buffering.

```cpp
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;

    // WS2812 timing symbols (pre-calculated)
    rmt_symbol_word_t ws2812_zero;
    rmt_symbol_word_t ws2812_one;
    rmt_symbol_word_t ws2812_reset;

    // State for incremental encoding
    const uint8_t *pixel_data;
    uint32_t total_bytes;
    uint32_t current_byte;
    uint8_t state;
} ws2812_encoder_t;

static size_t IRAM_ATTR ws2812_encoder_encode(
    rmt_encoder_t *encoder,
    rmt_channel_handle_t channel,
    const void *primary_data, size_t data_size,
    rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    while (ws2812_encoder->current_byte < ws2812_encoder->total_bytes) {
        uint8_t byte = ws2812_encoder->pixel_data[ws2812_encoder->current_byte];

        // Convert byte to 8 RMT symbols via bytes_encoder
        // ... (encoding implementation)

        ws2812_encoder->current_byte++;
        encoded_symbols += 8;

        if (encoded_symbols >= PULSES_PER_FILL) {
            state = RMT_ENCODING_MEM_FULL;
            break;
        }
    }

    if (ws2812_encoder->current_byte >= ws2812_encoder->total_bytes) {
        state = RMT_ENCODING_COMPLETE;
    }

    *ret_state = state;
    return encoded_symbols;
}
```

### Hybrid Approach: Built-in Encoder + Custom ISR

This provides the best balance of maintainability and control:

```cpp
class RmtController5Hybrid {
    rmt_encoder_handle_t mBytesEncoder;
    intr_handle_t mIntrHandle;
    volatile rmt_item32_t *mRMT_mem_ptr;

    void init() {
        // Create channel
        rmt_new_tx_channel(&tx_config, &mChannel);

        // Create bytes encoder (handles bit-to-symbol conversion)
        rmt_bytes_encoder_config_t encoder_config = {
            .bit0 = mZero,
            .bit1 = mOne,
            .flags.msb_first = 1
        };
        rmt_new_bytes_encoder(&encoder_config, &mBytesEncoder);

        // Register CUSTOM interrupt handler (not high-level callback)
        uint32_t channel_id = get_channel_id(mChannel);
        rmt_ll_enable_tx_thres_interrupt(&RMT, channel_id, true);
        rmt_ll_set_tx_thres(&RMT, channel_id, PULSES_PER_FILL);

        esp_intr_alloc(ETS_RMT_INTR_SOURCE,
                       ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                       &RmtController5Hybrid::globalISR, this, &mIntrHandle);
    }

    static void IRAM_ATTR globalISR(void *arg) {
        RmtController5Hybrid *self = (RmtController5Hybrid*)arg;
        uint32_t intr_st = RMT.int_st.val;
        uint32_t channel_id = get_channel_id(self->mChannel);

        if (intr_st & BIT(channel_id + 8)) {  // Threshold interrupt
            self->fillNextHalf();
            RMT.int_clr.val |= BIT(channel_id + 8);
        }

        if (intr_st & BIT(channel_id)) {  // Done interrupt
            self->onComplete();
            RMT.int_clr.val |= BIT(channel_id);
        }
    }
};
```

## Worker Pool Architecture

### Design Overview

To support N > K strips (where K = hardware channel count), we implement a worker pool that recycles RMT channels across multiple controllers.

**Architecture**:
```
┌─────────────────────────────────────────────────────┐
│                  RmtWorkerPool                      │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐   │
│  │  Worker 0  │  │  Worker 1  │  │  Worker K  │   │
│  │ (Channel 0)│  │ (Channel 1)│  │ (Channel K)│   │
│  └────────────┘  └────────────┘  └────────────┘   │
│         ↑              ↑              ↑            │
│         └──────────────┴──────────────┘            │
│              Assignment & Recycling                │
└─────────────────────────────────────────────────────┘
         ↑              ↑              ↑
         │              │              │
    ┌────────┐    ┌────────┐    ┌────────┐
    │Strip 0 │    │Strip 1 │... │Strip N │    (N > K)
    │ (Pin 5)│    │ (Pin 6)│    │(Pin 10)│
    └────────┘    └────────┘    └────────┘
```

**Key Principles**:
1. **Persistent Workers** - Own hardware channels and double-buffer state
2. **Ephemeral Controllers** - Own pixel data, borrow workers during transmission
3. **Pool Coordinator** - Manages worker assignment and recycling

### Worker Implementation

```cpp
namespace fl {

class RmtWorker {
public:
    // Worker lifecycle
    bool initialize(uint8_t worker_id);
    bool isAvailable() const { return mAvailable; }

    // Configuration and transmission
    bool configure(int pin, int t1, int t2, int t3, uint32_t reset_ns);
    void transmit(const uint8_t* pixel_data, size_t num_bytes);
    void waitForCompletion();

private:
    // Hardware resources
    rmt_channel_handle_t mChannel;
    rmt_encoder_handle_t mEncoder;
    uint8_t mChannelId;

    // Current configuration
    gpio_num_t mCurrentPin;
    int mT1, mT2, mT3;
    uint32_t mResetNs;

    // Double buffer state (like RMT4)
    volatile uint32_t mCur;
    volatile uint8_t mWhichHalf;
    volatile rmt_item32_t *mRMT_mem_start;
    volatile rmt_item32_t *mRMT_mem_ptr;

    // Pre-calculated symbols
    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // State tracking
    volatile bool mAvailable;
    volatile bool mTransmitting;
    const uint8_t* mPixelData;  // POINTER ONLY - not owned
    size_t mNumBytes;

    // Double buffer refill (like RMT4)
    void IRAM_ATTR fillNextHalf();

    // Static ISR handlers
    static void IRAM_ATTR globalISR(void *arg);
};

class RmtWorkerPool {
public:
    static RmtWorkerPool& getInstance();

    // Worker acquisition/release
    RmtWorker* acquireWorker();  // May block if N > K
    void releaseWorker(RmtWorker* worker);

private:
    fl::vector<RmtWorker*> mWorkers;  // K workers
    portMUX_TYPE mSpinlock;

    RmtWorker* findAvailableWorker();
};

} // namespace fl
```

### Lightweight Controller Interface

Controllers become lightweight - they own pixel data but borrow workers:

```cpp
class RmtController5LowLevel {
public:
    RmtController5LowLevel(int DATA_PIN, int T1, int T2, int T3, int RESET_US);

    void loadPixelData(PixelIterator &pixels);
    void showPixels();

private:
    // Configuration (not hardware resources!)
    gpio_num_t mPin;
    int mT1, mT2, mT3;
    uint32_t mResetNs;

    // Pixel data buffer (owned by controller)
    uint8_t *mPixelData;
    size_t mPixelDataSize;

    // Current worker assignment (temporary)
    RmtWorker* mCurrentWorker;

    void waitForPreviousTransmission();
};
```

## Worker Transfer Mechanics

### Two-Phase Waiting Model

**Phase 1 - Worker Acquisition** (in `endShowLeds()`):
```cpp
void RmtController5::showPixels() {
    // Acquire worker (may block with polling if N > K)
    RmtWorker* worker = RmtWorkerPool::getInstance().acquireWorker();

    // Configure and start transmission
    worker->configure(mPin, mT1, mT2, mT3, mResetNs);
    worker->transmit(mPixelData, mPixelDataSize);

    // Store for completion waiting
    mCurrentWorker = worker;

    // Return immediately after transmission STARTS
}
```

**Phase 2 - Transmission Completion** (in next frame's `loadPixelData()`):
```cpp
void RmtController5::loadPixelData(PixelIterator &pixels) {
    // Wait for previous transmission before overwriting buffer
    waitForPreviousTransmission();

    // Now safe to load new pixel data
    // ... (copy pixel data to mPixelData)
}

void RmtController5::waitForPreviousTransmission() {
    if (mCurrentWorker) {
        mCurrentWorker->waitForCompletion();
        RmtWorkerPool::getInstance().releaseWorker(mCurrentWorker);
        mCurrentWorker = nullptr;
    }
}
```

### Worker Acquisition with Polling

```cpp
RmtWorker* RmtWorkerPool::acquireWorker() {
    portENTER_CRITICAL(&mSpinlock);
    RmtWorker* worker = findAvailableWorker();
    portEXIT_CRITICAL(&mSpinlock);

    if (worker) return worker;  // Immediate return if available

    // No workers available - poll until one frees up
    while (true) {
        delayMicroseconds(100);

        portENTER_CRITICAL(&mSpinlock);
        worker = findAvailableWorker();
        portEXIT_CRITICAL(&mSpinlock);

        if (worker) return worker;

        // Yield periodically to FreeRTOS
        static uint32_t pollCount = 0;
        if (++pollCount % 50 == 0) {  // Every 5ms
            yield();
        }
    }
}
```

### Execution Timeline

**Scenario**: ESP32-S3 (K=4 workers), 6 strips (N=6), 300 LEDs each, ~9ms transmission time

```
Frame 0:
  t=0ms:   Controllers 0-3 acquire workers immediately → transmit → return
  t=0ms:   Controller 4 blocks in acquireWorker() (all workers busy)
  t=9ms:   Worker 0 completes → Controller 4 acquires → transmit → return
  t=9ms:   Controller 5 acquires Worker 1 → transmit → return
  t=18ms:  All transmissions complete

Frame 1:
  t=18ms:  All controllers call waitForCompletion()
  t=18ms:  All workers done → controllers load new data → cycle repeats
```

**Benefits**:
- ✅ Each controller waits only for itself (fair distribution)
- ✅ `endShowLeds()` returns after transmission STARTS
- ✅ Next frame waits for previous transmission to COMPLETE
- ✅ Preserves async semantics when N ≤ K

## Integration with FastLED Framework

### FastLED's Synchronization Model

FastLED provides built-in synchronization hooks that eliminate the need for complex coordination logic:

```cpp
// User code - simple global show()
void loop() {
    // Update pixel data
    fill_rainbow(leds1, NUM_LEDS1, gHue, 7);
    fill_rainbow(leds2, NUM_LEDS2, gHue + 128, 7);

    // Single global show() call
    FastLED.show();  // Coordinates all controllers automatically
}
```

**What happens inside `FastLED.show()`**:

1. **`onBeforeShow()`** called for each controller
   - Controllers prepare pixel data buffers
   - No blocking, no transmission yet

2. **`onEndShow()`** called for each controller
   - **This is where worker assignment happens**
   - Controller acquires worker (may block if N > K)
   - Starts transmission
   - Returns once transmission STARTS (not completes)

3. Next frame's `onBeforeShow()` waits for previous transmission to complete

### Worker Assignment in `onEndShow()`

```cpp
class RmtController5LowLevel {
public:
    void onBeforeShow() {
        // Wait for previous transmission to complete
        if (mCurrentWorker) {
            mCurrentWorker->waitForCompletion();
            RmtWorkerPool::getInstance().releaseWorker(mCurrentWorker);
            mCurrentWorker = nullptr;
        }

        // Pixel data already loaded by FastLED framework
        // Nothing else to do here
    }

    void onEndShow() {
        // Acquire worker (BLOCKS if N > K and all workers busy)
        RmtWorker* worker = RmtWorkerPool::getInstance().acquireWorker();

        // Configure worker for this controller
        worker->configure(mPin, mT1, mT2, mT3, mResetNs);

        // Start transmission (double-buffered, async)
        worker->transmit(mPixelData, mPixelDataSize);

        // Store worker reference for next frame
        mCurrentWorker = worker;

        // Return immediately after transmission STARTS
        // (not after it completes)
    }

private:
    RmtWorker* mCurrentWorker;
};
```

### Execution Flow

**ESP32-S3 (K=4 workers), 6 strips (N=6), 300 LEDs each, ~9ms transmission time**

```
User calls FastLED.show():

Phase 1 - onBeforeShow() for all controllers:
  t=0ms: Controller 0 onBeforeShow() → wait for previous TX → done
  t=0ms: Controller 1 onBeforeShow() → wait for previous TX → done
  t=0ms: Controller 2 onBeforeShow() → wait for previous TX → done
  t=0ms: Controller 3 onBeforeShow() → wait for previous TX → done
  t=0ms: Controller 4 onBeforeShow() → wait for previous TX → done
  t=0ms: Controller 5 onBeforeShow() → wait for previous TX → done

Phase 2 - onEndShow() for all controllers:
  t=0ms: Controller 0 onEndShow() → acquireWorker() → Worker 0 → transmit() → return
  t=0ms: Controller 1 onEndShow() → acquireWorker() → Worker 1 → transmit() → return
  t=0ms: Controller 2 onEndShow() → acquireWorker() → Worker 2 → transmit() → return
  t=0ms: Controller 3 onEndShow() → acquireWorker() → Worker 3 → transmit() → return
  t=0ms: Controller 4 onEndShow() → acquireWorker() [BLOCKS - all workers busy]
  t=9ms: Worker 0 completes → Controller 4 acquires Worker 0 → transmit() → return
  t=9ms: Controller 5 onEndShow() → acquireWorker() → Worker 1 → transmit() → return

FastLED.show() returns at t=9ms (when last controller starts TX)
All transmissions complete at t=18ms
```

### Shortest-First Scheduling (Optional Optimization)

Controllers are called in registration order by default. For shortest-first scheduling, register controllers in order of strip length:

```cpp
// During setup - register shortest strips first
CRGB leds_short1[50];
CRGB leds_short2[50];
CRGB leds_medium[100];
CRGB leds_long1[300];
CRGB leds_long2[300];
CRGB leds_long3[300];

void setup() {
    // Register in shortest-first order
    FastLED.addLeds<WS2812, PIN1>(leds_short1, 50);   // 50 LEDs
    FastLED.addLeds<WS2812, PIN2>(leds_short2, 50);   // 50 LEDs
    FastLED.addLeds<WS2812, PIN3>(leds_medium, 100);  // 100 LEDs
    FastLED.addLeds<WS2812, PIN4>(leds_long1, 300);   // 300 LEDs
    FastLED.addLeds<WS2812, PIN5>(leds_long2, 300);   // 300 LEDs
    FastLED.addLeds<WS2812, PIN6>(leds_long3, 300);   // 300 LEDs
}

// FastLED.show() will call onEndShow() in this order
```

**Benefits**:
- ✅ Fast strips complete quickly (1.5ms, not 9ms)
- ✅ Slow strips only wait 1.5ms for workers (not 9ms)
- ✅ Better perceived responsiveness
- ✅ No code changes needed - just registration order

**Timeline with shortest-first registration**:
```
t=0ms:   Controller 0 (50 LEDs) → Worker 0 (1.5ms)
t=0ms:   Controller 1 (50 LEDs) → Worker 1 (1.5ms)
t=0ms:   Controller 2 (100 LEDs) → Worker 2 (3ms)
t=0ms:   Controller 3 (300 LEDs) → Worker 3 (9ms)
t=1.5ms: Controller 4 (300 LEDs) → Worker 0 (9ms) [waited 1.5ms]
t=1.5ms: Controller 5 (300 LEDs) → Worker 1 (9ms) [waited 1.5ms]
t=10.5ms: All complete
```

### Key Differences from RMT4

**RMT4** (no `onBeforeShow`/`onEndShow`):
- Required complex batch coordination
- Last controller blocked until ALL complete
- Uneven load distribution

**RMT5** (with `onBeforeShow`/`onEndShow`):
- ✅ Simple per-controller blocking
- ✅ Fair load distribution
- ✅ No batch coordination needed
- ✅ Built-in synchronization via FastLED framework

## Critical Implementation Details

### 1. LED Strip API Buffer Problem

**🚨 MAJOR BLOCKER** for worker pooling with high-level API.

The ESP-IDF `led_strip` API embeds the pixel buffer inside the RMT object:

```cpp
// From esp-idf/components/led_strip/src/led_strip_rmt_dev.c
typedef struct {
    led_strip_t base;
    rmt_channel_handle_t rmt_chan;
    uint8_t *pixel_buf;                    // THE PROBLEM: Embedded buffer
    bool pixel_buf_allocated_internally;
} led_strip_rmt_obj;
```

**Why This Breaks Worker Recycling**:
- Worker reconfiguration requires deleting/recreating `led_strip` objects
- Each delete/create = 900-byte malloc/free (for 300 LED strip)
- Result: Heap fragmentation, allocation churn, potential failures

**Problems**:
- ❌ Massive allocation churn (900 bytes per worker switch)
- ❌ Heap fragmentation over time
- ❌ Performance overhead (~50-100µs malloc/free)
- ❌ Unpredictable timing

**Solution: Detachable Buffers**

Workers store only **pointers** to controller-owned buffers:

```cpp
// Controller owns buffer
class RmtController5LowLevel {
private:
    uint8_t *mPixelData;  // Owned buffer (allocated once)
public:
    const uint8_t* getPixelData() const { return mPixelData; }
};

// Worker stores pointer only
class RmtWorker {
private:
    const uint8_t* mPixelData;  // POINTER - not owned

public:
    void transmit(const uint8_t* pixel_data, size_t num_bytes) {
        mPixelData = pixel_data;  // Just store pointer - NO ALLOCATION
        fillNextHalf();  // Reads from controller's buffer
        tx_start();
    }
};
```

**Memory Savings**:
```
N=6 controllers, K=4 workers, 300 LEDs each = 900 bytes/buffer

High-Level API (led_strip):
  Controllers: 6 × 900 = 5.4KB
  Workers: 4 × 900 = 3.6KB
  Total: 9.0KB + allocation churn

Low-Level API (detached buffers):
  Controllers: 6 × 900 = 5.4KB
  Workers: 0 bytes (pointers only)
  Total: 5.4KB (fixed)

Savings: 3.6KB + eliminated allocation churn
```

### 2. Memory Block Configuration

**Problem**: ESP-IDF v5 `rmt_tx_channel_config_t` doesn't have `mem_block_num` like v4.

**Solution**: Use `mem_block_symbols` to allocate double the memory:

```cpp
rmt_tx_channel_config_t tx_config = {
    .gpio_num = pin,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10000000,  // 10MHz
    .mem_block_symbols = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL,  // 128 words
    .trans_queue_depth = 1,
};
```

### 3. Threshold Interrupt Setup

**Problem**: ESP-IDF v5 callbacks don't have threshold events.

**Solutions**:

**Option A**: Use encoder's `RMT_ENCODING_MEM_FULL` state
```cpp
// In encoder's encode() function
if (encoded_symbols >= PULSES_PER_FILL) {
    *ret_state = RMT_ENCODING_MEM_FULL;
    // Encoder called again for next chunk
}
```

**Option B**: Direct register access (recommended for full control)
```cpp
// Set up threshold interrupt manually
rmt_ll_enable_tx_thres_interrupt(&RMT, channel_id, true);
rmt_ll_set_tx_thres(&RMT, channel_id, PULSES_PER_FILL);

// Register custom ISR
esp_intr_alloc(ETS_RMT_INTR_SOURCE,
               ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
               custom_rmt_isr_handler, this, &mIntrHandle);
```

### 4. Direct Memory Access

**Issue**: Need to extract channel ID from opaque `rmt_channel_handle_t`.

**Solution**: Peek inside internal structure:

```cpp
// Internal structure (from esp-idf)
typedef struct rmt_tx_channel_t {
    rmt_channel_t base;
    uint32_t channel_id;
    // ... other fields
} rmt_tx_channel_t;

// Extract channel ID
uint32_t get_channel_id(rmt_channel_handle_t handle) {
    rmt_tx_channel_t *tx_chan = (rmt_tx_channel_t*)handle;
    return tx_chan->channel_id;
}

// Access RMT memory
uint32_t channel_id = get_channel_id(mChannel);
mRMT_mem_start = &(RMTMEM.chan[channel_id].data32[0]);
```

**Warning**: Relies on internal structures, may break in future IDF versions.

### 5. High Interrupt Priority

For maximum Wi-Fi immunity, use high interrupt priority to prevent Wi-Fi interrupts from delaying RMT buffer refills.

**Platform-Specific Constraints**:

#### Xtensa Architecture (ESP32, ESP32-S3)

**C Function Limit**: Xtensa cores can only call C functions from interrupt levels **≤ 3**.

**Level 3 (Maximum for direct C)**:
```cpp
// Level 3 - direct C function call (no trampoline needed)
esp_intr_alloc(ETS_RMT_INTR_SOURCE,
               ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
               custom_rmt_isr_handler, this, &mIntrHandle);
```

**Levels 4-5 (Requires Assembly Trampoline)**:

For interrupt levels > 3, Xtensa requires assembly trampolines to handle stack frame management before calling C code.

```cpp
// Level 4/5 - requires assembly trampoline
esp_intr_alloc(ETS_RMT_INTR_SOURCE,
               ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL4,
               trampoline_to_c_handler,  // Assembly shim
               this, &mIntrHandle);

// Assembly trampoline (from src/platforms/esp/32/interrupts/)
extern "C" void trampoline_to_c_handler(void* arg) {
    // Assembly code to save/restore stack frame
    // Then calls actual C handler
    actual_c_handler(arg);
}
```

**Experimental Support**: The project has experimental interrupt infrastructure in `src/platforms/esp/32/interrupts/` that provides trampolines for Xtensa Level 4/5 interrupts.

See: `src/platforms/esp/32/interrupts/esp32_s3.hpp:316-368`

#### RISC-V Architecture (ESP32-C3, ESP32-C6, ESP32-H2)

**No Trampoline Needed**: RISC-V can call C functions directly from **any interrupt level (1-7)** without assembly trampolines.

```cpp
// Level 4 or 5 - direct C function call (no special handling)
esp_intr_alloc(ETS_RMT_INTR_SOURCE,
               ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL4,
               custom_rmt_isr_handler, this, &mIntrHandle);
```

**Recommendation for RISC-V**: Use Level 4 or 5 for maximum Wi-Fi immunity.

#### Priority Level Recommendations

| Platform | Wi-Fi Priority | Recommended RMT Priority | Trampoline Required? |
|----------|----------------|-------------------------|---------------------|
| ESP32 (Xtensa LX6) | 4 | **3** (safe default) | No |
| ESP32 (Xtensa LX6) | 4 | **4** (experimental) | Yes (assembly) |
| ESP32-S3 (Xtensa LX7) | 4 | **3** (safe default) | No |
| ESP32-S3 (Xtensa LX7) | 4 | **4** (experimental) | Yes (assembly) |
| ESP32-C3 (RISC-V) | 4 | **4 or 5** | No |
| ESP32-C6 (RISC-V) | 4 | **4 or 5** | No |

**Stretch Goal**: For Xtensa platforms, integrate experimental interrupt trampolines from `src/platforms/esp/32/interrupts/` to enable Level 4/5 interrupts with C handlers.

## Alternative: One-Shot Encoder

### Strategy

Pre-encode entire LED strip to RMT symbols **before** transmission starts to completely eliminate flicker.

### How It Works

```cpp
class RmtWorkerOneShot {
private:
    rmt_item32_t* mEncodedSymbols;
    size_t mEncodedSize;

public:
    void transmit(const uint8_t* pixel_data, size_t num_bytes) {
        // Calculate required symbols
        const size_t num_symbols = num_bytes * 8;

        // Allocate or reuse symbol buffer
        if (mEncodedSize < num_symbols) {
            free(mEncodedSymbols);
            mEncodedSymbols = (rmt_item32_t*)malloc(num_symbols * sizeof(rmt_item32_t));
            mEncodedSize = num_symbols;
        }

        // PRE-ENCODE all pixel data
        rmt_item32_t* out = mEncodedSymbols;
        for (size_t i = 0; i < num_bytes; i++) {
            convert_byte_to_rmt(pixel_data[i], mZero.val, mOne.val, out);
            out += 8;
        }

        // ONE-SHOT transmission (no interrupts needed)
        rmt_write_items(mChannel, mEncodedSymbols, num_symbols, false);
    }
};
```

### Memory Cost

**Example**: 300 LED strip, RGB (3 bytes/pixel)

```
Pixel Data: 300 × 3 = 900 bytes
RMT Symbols: 900 × 8 bits × 4 bytes/symbol = 28,800 bytes = 28.8KB

Memory overhead: 32× larger than pixel data
```

### Comparison Table

| Approach | Memory (300 LEDs) | Flicker Risk | Complexity | Notes |
|----------|-------------------|--------------|------------|-------|
| High-Level API (led_strip) | 900 bytes | High | Low | No double buffer |
| Double Buffer (RMT4-style) | 900 bytes + 256 bytes | Low | Medium | Level 3 ISR |
| One-Shot Encoder | 900 bytes + 28.8KB | **Zero** | Low | No ISR needed |
| High-Priority ISR (Xtensa) | 900 bytes + 256 bytes | **Zero** | High | Level 4 requires trampoline |
| High-Priority ISR (RISC-V) | 900 bytes + 256 bytes | **Zero** | Medium | Level 4/5 direct C call |

### When to Use One-Shot

**✅ Use when**:
- Small to medium LED counts (<500 LEDs)
- Abundant RAM available (ESP32: 320KB, ESP32-S3: 512KB)
- Absolute zero-flicker requirement
- Simplicity preferred over memory efficiency

**❌ Avoid when**:
- Large LED counts (>500 LEDs, >50KB per strip)
- Low memory devices
- Multiple long strips (memory multiplies by N)

### Hybrid Approach

Best of both worlds - use one-shot for short strips, double-buffer for long:

```cpp
class RmtWorker {
private:
    static constexpr size_t ONE_SHOT_THRESHOLD = 200;  // LEDs

public:
    void transmit(const uint8_t* pixel_data, size_t num_bytes) {
        const size_t num_leds = num_bytes / 3;

        if (num_leds <= ONE_SHOT_THRESHOLD) {
            transmitOneShot(pixel_data, num_bytes);
        } else {
            transmitDoubleBuffer(pixel_data, num_bytes);
        }
    }
};
```

**Memory Example** (ESP32-S3, 6 strips: `[50, 50, 100, 300, 300, 300]` LEDs):

```
Pure One-Shot: 105.6KB (20% of 512KB SRAM)
Hybrid (threshold=150): 21.9KB (4% of SRAM)

Savings: 79% reduction
```

## Worker Reconfiguration

### Reconfiguration Logic

```cpp
bool RmtWorker::configure(int pin, int t1, int t2, int t3, uint32_t reset_ns) {
    // Check if reconfiguration needed
    if (mCurrentPin == pin && mT1 == t1 && mT2 == t2 && mT3 == t3) {
        return true;  // Already configured
    }

    // Wait for active transmission
    if (mTransmitting) waitForCompletion();

    // Update pin assignment
    mCurrentPin = gpio_num_t(pin);
    esp_err_t ret = rmt_set_gpio(mChannel, RMT_MODE_TX, mCurrentPin, false);
    if (ret != ESP_OK) return false;

    // Update timing parameters
    mT1 = t1; mT2 = t2; mT3 = t3; mResetNs = reset_ns;

    // Recalculate RMT symbols
    const uint32_t RMT_RESOLUTION = 10000000;
    const uint32_t TICKS_PER_NS = RMT_RESOLUTION / 1000000000;

    mZero.level0 = 1;
    mZero.duration0 = t1 * TICKS_PER_NS;
    mZero.level1 = 0;
    mZero.duration1 = (t2 + t3) * TICKS_PER_NS;

    mOne.level0 = 1;
    mOne.duration0 = (t1 + t2) * TICKS_PER_NS;
    mOne.level1 = 0;
    mOne.duration1 = t3 * TICKS_PER_NS;

    // NO buffer reallocation - workers don't own buffers!
    return true;
}
```

**Reconfiguration Cost**:
- Pin reassignment: ~1-5µs (GPIO matrix update)
- Symbol recalculation: ~0.5µs (arithmetic only)
- **Total: ~2-6µs** (negligible compared to 9ms transmission time)

## Testing Strategy

### QEMU Test Framework

```bash
# Install QEMU for ESP32 emulation
uv run ci/install-qemu.py

# Run tests on different architectures
uv run test.py --qemu esp32s3  # Xtensa LX7
uv run test.py --qemu esp32c3  # RISC-V
```

### Test Scenarios

1. **Baseline Test**: Verify double buffering works without Wi-Fi
2. **Wi-Fi Stress Test**: Enable AP mode + web server, measure flicker reduction
3. **Timing Validation**: Oscilloscope verification of WS2812 timing compliance
4. **Interrupt Latency**: Measure ISR response time under load
5. **Comparison Test**: RMT4 (ESP32) vs RMT5 (ESP32-S3) side-by-side
6. **Worker Pool Test**: Verify N > K strips (e.g., 8 strips on ESP32-C3, K=2)
7. **Reconfiguration Test**: Measure worker switching overhead
8. **Memory Usage**: Validate no leaks during worker recycling
9. **One-Shot Test**: Verify zero-flicker on short strips
10. **Scheduling Test**: Verify shortest-first ordering

### Platform Coverage

| Platform | Architecture | RMT Channels | Test Priority |
|----------|-------------|--------------|---------------|
| ESP32-S3 | Xtensa LX7 | 4 | High |
| ESP32-C3 | RISC-V | 2 | High |
| ESP32 | Xtensa LX6 | 8 | Reference |
| ESP32-C6 | RISC-V | 2 | Medium |

## Full Feature Set Summary

**Implemented Capabilities**:
1. ✅ **Double buffering** - Wi-Fi interference immunity via ping-pong
2. ✅ **Worker pooling** - Support unlimited strips (N > K)
3. ✅ **FastLED integration** - Synchronization via `onBeforeShow()`/`onEndShow()`
4. ✅ **Shortest-first scheduling** - Registration order determines execution order
5. ✅ **Detachable buffers** - Zero allocation overhead
6. ✅ **One-shot encoding** - Alternative zero-flicker strategy
7. ✅ **Hybrid approach** - Automatic selection based on strip length
8. ✅ **High interrupt priority** - Optional Level 4/5 (stretch goal)

**Architecture Summary**:
- **K Workers** with double-buffered RMT channels
- **N Controllers** with persistent pixel data buffers
- **Worker Pool** manages worker acquisition/release
- **Custom ISR** with threshold interrupts for refill
- **Per-controller blocking** in `onEndShow()` when workers exhausted (N > K)
- **Completion waiting** in `onBeforeShow()` of next frame
- **No batch coordination** - FastLED framework provides synchronization

## Benefits vs Costs

### Benefits
- ✅ True double buffering like RMT4
- ✅ Resistant to Wi-Fi interference (Level 3 ISR)
- ✅ Support unlimited strips (worker pooling)
- ✅ No allocation churn (detached buffers)
- ✅ Fair waiting distribution (per-controller blocking)
- ✅ Simple integration via `onBeforeShow()`/`onEndShow()`
- ✅ Multiple flicker-mitigation strategies
- ✅ Optional high interrupt priority (Level 4/5 stretch goal)
- ✅ Full control over timing and buffer management
- ✅ No complex batch coordination needed

### Costs
- ❌ Bypasses maintained `led_strip` API
- ❌ Relies on internal IDF structures (fragile)
- ❌ More complex code (harder to maintain)
- ❌ May break on future IDF versions
- ❌ Platform-specific code for each ESP32 variant
- ❌ Higher memory usage if using one-shot encoding

## Conclusion

Implementing RMT4-style double buffering in RMT5 with worker pooling **is technically feasible** but requires significant architectural changes:

1. **Abandoning the high-level LED strip API**
2. **Using low-level RMT TX API with custom ISR**
3. **Direct memory access to RMT buffers**
4. **Platform-specific interrupt handling**
5. **Detachable buffer architecture**

The **hybrid approach** (built-in encoder + custom ISR + direct memory + worker pooling + one-shot option) offers the best balance of:
- Maintainability (leverages IDF encoder where possible)
- Control (custom ISR for timing-critical operations)
- Scalability (worker pool for N > K strips)
- Flexibility (multiple anti-flicker strategies)

**Recommended Implementation Priority**:
1. **Phase 1**: Implement basic double-buffer with Level 3 ISR (RMT4 parity, works on all platforms)
2. **Phase 2**: Add worker pool with `onBeforeShow()`/`onEndShow()` integration (N > K support)
3. **Phase 3**: Implement one-shot encoding as alternative (zero-flicker for short strips)

**Stretch Goals** (Advanced features, not required for core functionality):
- **High-priority ISR (Level 4/5)**: Maximum Wi-Fi immunity
  - RISC-V: Direct C handlers at Level 4/5 (straightforward)
  - Xtensa: Requires assembly trampolines from `src/platforms/esp/32/interrupts/` (experimental)

This approach combines the best of both worlds: RMT4's proven reliability with RMT5's flexibility and modern IDF architecture.

## References

- **RMT4 Implementation**: `src/platforms/esp/32/rmt_4/idf4_rmt_impl.cpp`
- **RMT5 Implementation**: `src/platforms/esp/32/rmt_5/idf5_rmt.cpp`
- **RMT Analysis**: `RMT.md`
- **Feature Spec**: `FEATURE_RMT5_POOL.md`
- **Interrupt Infrastructure**: `src/platforms/esp/32/interrupts/`
- **ESP-IDF RMT Docs**: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html
