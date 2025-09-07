# RMT5 Worker Pool Implementation

## Problem Statement

ESP32 RMT5 has hardware limitations on the number of LED strips it can control simultaneously:
- **ESP32**: 8 RMT channels maximum
- **ESP32-S2/S3**: 4 RMT channels maximum  
- **ESP32-C3/H2**: 2 RMT channels maximum

Currently, FastLED's RMT5 implementation creates a one-to-one mapping between `RmtController5` instances and RMT hardware channels. This means you can only control K strips simultaneously, where K is the hardware limit.

**Goal**: Implement a worker pool system to support N strips where N > K, allowing any reasonable number of LED strips to be controlled by recycling RMT workers.

## Current RMT5 Architecture

### Class Hierarchy
```cpp
ClocklessController<PIN, T1, T2, T3, RGB_ORDER>
  └── RmtController5 (mRMTController)
      └── IRmtStrip* (mLedStrip) 
          └── RmtStrip (concrete implementation)
              └── led_strip_handle_t (mStrip) // ESP-IDF handle
                  └── rmt_channel_handle_t    // Hardware channel
```

### Current Flow
1. `RmtController5::loadPixelData()` creates `IRmtStrip` on first call
2. `IRmtStrip::Create()` calls `led_strip_new_rmt_device()` which allocates a hardware RMT channel
3. Channel remains allocated until `RmtStrip` destructor calls `led_strip_del()`
4. No sharing or pooling exists

## RMT4 Worker Pool Reference

RMT4 implements a sophisticated worker pool system:

### Key Components
- **`gControllers[FASTLED_RMT_MAX_CONTROLLERS]`**: Array of all registered controllers
- **`gOnChannel[FASTLED_RMT_MAX_CHANNELS]`**: Currently active controllers per channel
- **Global counters**: `gNumStarted`, `gNumDone`, `gNext` for coordination
- **Semaphore coordination**: `gTX_sem` for synchronization

### RMT4 Worker Pool Flow
1. All controllers register in `gControllers[]` during construction
2. `showPixels()` triggers batch processing when `gNumStarted == gNumControllers`
3. First K controllers start immediately on available channels
4. Remaining controllers queue until channels become available
5. `doneOnChannel()` callback releases channels and starts next queued controller
6. Process continues until all controllers complete

## Proposed RMT5 Worker Pool Architecture

### Core Design Principles

1. **Backward Compatibility**: Existing `RmtController5` API remains unchanged
2. **Transparent Pooling**: Controllers don't know they're sharing workers
3. **Automatic Resource Management**: Workers handle setup/teardown automatically
4. **Thread Safety**: Pool operations are atomic and interrupt-safe

### New Components

#### 1. RmtWorkerPool (Singleton)
```cpp
class RmtWorkerPool {
public:
    static RmtWorkerPool& getInstance();
    
    // Worker management
    RmtWorker* acquireWorker(const RmtWorkerConfig& config);
    void releaseWorker(RmtWorker* worker);
    
    // Coordination
    void registerController(RmtController5* controller);
    void unregisterController(RmtController5* controller);
    void executeDrawCycle();
    
private:
    fl::vector<RmtWorker*> mAvailableWorkers;
    fl::vector<RmtWorker*> mBusyWorkers;
    fl::vector<RmtController5*> mRegisteredControllers;
    
    // Synchronization
    SemaphoreHandle_t mPoolMutex;
    SemaphoreHandle_t mDrawSemaphore;
    
    // State tracking
    int mActiveDrawCount;
    int mCompletedDrawCount;
};
```

#### 2. RmtWorker (Replaceable RMT Resource)
```cpp
class RmtWorker {
public:
    // Configuration for worker setup
    struct Config {
        int pin;
        uint32_t ledCount;
        bool isRgbw;
        uint32_t t0h, t0l, t1h, t1l, reset;
        IRmtStrip::DmaMode dmaMode;
    };
    
    // Worker lifecycle
    bool configure(const Config& config);
    void loadPixelData(fl::PixelIterator& pixels);
    void startTransmission();
    void waitForCompletion();
    
    // State management
    bool isAvailable() const;
    bool isConfiguredFor(const Config& config) const;
    void reset();
    
    // Callbacks
    void onTransmissionComplete();
    
private:
    IRmtStrip* mCurrentStrip;
    Config mCurrentConfig;
    bool mIsAvailable;
    bool mTransmissionActive;
    RmtWorkerPool* mPool; // Back reference for release
};
```

#### 3. Modified RmtController5
```cpp
class RmtController5 {
public:
    // Existing API unchanged
    RmtController5(int DATA_PIN, int T1, int T2, int T3, DmaMode dma_mode);
    void loadPixelData(PixelIterator &pixels);
    void showPixels();
    
private:
    // New pooled implementation
    RmtWorkerConfig mWorkerConfig;
    fl::vector<uint8_t> mPixelBuffer; // Persistent buffer
    bool mRegisteredWithPool;
    
    // Remove direct IRmtStrip ownership
    // IRmtStrip *mLedStrip = nullptr; // REMOVED
};
```

### Worker Pool Operation Flow

#### Registration Phase (Constructor)
```cpp
RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3, DmaMode dma_mode) 
    : mPin(DATA_PIN), mT1(T1), mT2(T2), mT3(T3), mDmaMode(dma_mode) {
    
    // Configure worker requirements
    mWorkerConfig = {DATA_PIN, 0, false, t0h, t0l, t1h, t1l, 280, convertDmaMode(dma_mode)};
    
    // Register with pool
    RmtWorkerPool::getInstance().registerController(this);
    mRegisteredWithPool = true;
}
```

#### Data Loading Phase
```cpp
void RmtController5::loadPixelData(PixelIterator &pixels) {
    // Update worker config with actual pixel count
    mWorkerConfig.ledCount = pixels.size();
    mWorkerConfig.isRgbw = pixels.get_rgbw().active();
    
    // Store pixel data in persistent buffer
    storePixelData(pixels);
}

void RmtController5::storePixelData(PixelIterator &pixels) {
    const int bytesPerPixel = mWorkerConfig.isRgbw ? 4 : 3;
    const int bufferSize = mWorkerConfig.ledCount * bytesPerPixel;
    
    mPixelBuffer.resize(bufferSize);
    
    // Copy pixel data to persistent buffer
    uint8_t* bufPtr = mPixelBuffer.data();
    if (mWorkerConfig.isRgbw) {
        while (pixels.has(1)) {
            uint8_t r, g, b, w;
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            *bufPtr++ = r; *bufPtr++ = g; *bufPtr++ = b; *bufPtr++ = w;
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        while (pixels.has(1)) {
            uint8_t r, g, b;
            pixels.loadAndScaleRGB(&r, &g, &b);
            *bufPtr++ = r; *bufPtr++ = g; *bufPtr++ = b;
            pixels.advanceData();
            pixels.stepDithering();
        }
    }
}
```

#### Execution Phase (Coordinated Draw)
```cpp
void RmtController5::showPixels() {
    // Trigger coordinated draw cycle
    RmtWorkerPool::getInstance().executeDrawCycle();
}

void RmtWorkerPool::executeDrawCycle() {
    // Similar to RMT4 coordination logic
    mActiveDrawCount = 0;
    mCompletedDrawCount = 0;
    
    // Take draw semaphore
    xSemaphoreTake(mDrawSemaphore, portMAX_DELAY);
    
    // Start as many controllers as we have workers
    int startedCount = 0;
    for (auto* controller : mRegisteredControllers) {
        if (startedCount < mAvailableWorkers.size()) {
            startController(controller);
            startedCount++;
        }
    }
    
    // Wait for all controllers to complete
    while (mCompletedDrawCount < mRegisteredControllers.size()) {
        xSemaphoreTake(mDrawSemaphore, portMAX_DELAY);
        
        // Start next queued controller if workers available
        startNextQueuedController();
        
        xSemaphoreGive(mDrawSemaphore);
    }
}

void RmtWorkerPool::startController(RmtController5* controller) {
    // Acquire worker from pool
    RmtWorker* worker = acquireWorker(controller->getWorkerConfig());
    if (!worker) {
        // This should not happen if pool is sized correctly
        return;
    }
    
    // Configure worker for this controller
    worker->configure(controller->getWorkerConfig());
    
    // Load pixel data from controller's persistent buffer
    loadPixelDataToWorker(worker, controller);
    
    // Start transmission
    worker->startTransmission();
    
    mActiveDrawCount++;
}

void RmtWorkerPool::onWorkerComplete(RmtWorker* worker) {
    // Called from worker's completion callback
    mCompletedDrawCount++;
    
    // Release worker back to pool
    releaseWorker(worker);
    
    // Signal main thread
    xSemaphoreGive(mDrawSemaphore);
}
```

### Worker Reconfiguration Strategy

#### Efficient Worker Reuse
```cpp
bool RmtWorker::configure(const Config& newConfig) {
    // Check if reconfiguration is needed
    if (isConfiguredFor(newConfig)) {
        return true; // Already configured correctly
    }
    
    // Tear down current configuration
    if (mCurrentStrip) {
        // Wait for any pending transmission
        if (mTransmissionActive) {
            mCurrentStrip->waitDone();
        }
        
        // Clean shutdown
        delete mCurrentStrip;
        mCurrentStrip = nullptr;
    }
    
    // Create new strip with new configuration
    mCurrentStrip = IRmtStrip::Create(
        newConfig.pin, newConfig.ledCount, newConfig.isRgbw,
        newConfig.t0h, newConfig.t0l, newConfig.t1h, newConfig.t1l, 
        newConfig.reset, newConfig.dmaMode
    );
    
    if (!mCurrentStrip) {
        return false;
    }
    
    mCurrentConfig = newConfig;
    return true;
}
```

#### Pin State Management
```cpp
void RmtWorker::reset() {
    if (mCurrentStrip) {
        // Ensure transmission is complete
        if (mTransmissionActive) {
            mCurrentStrip->waitDone();
        }
        
        // Set pin to safe state before teardown
        gpio_set_level((gpio_num_t)mCurrentConfig.pin, 0);
        gpio_set_direction((gpio_num_t)mCurrentConfig.pin, GPIO_MODE_OUTPUT);
        
        // Clean up strip
        delete mCurrentStrip;
        mCurrentStrip = nullptr;
    }
    
    mTransmissionActive = false;
    mIsAvailable = true;
}
```

### Memory Management Strategy

#### Persistent Pixel Buffers
Each `RmtController5` maintains its own pixel buffer to avoid data races:

```cpp
class RmtController5 {
private:
    fl::vector<uint8_t> mPixelBuffer;  // Persistent storage
    RmtWorkerConfig mWorkerConfig;     // Configuration cache
    
    void storePixelData(PixelIterator& pixels);
    void restorePixelData(RmtWorker* worker);
};
```

#### Worker Pool Sizing
```cpp
void RmtWorkerPool::initialize() {
    // Determine hardware channel count
    int maxChannels = getHardwareChannelCount();
    
    // Create one worker per hardware channel
    mAvailableWorkers.reserve(maxChannels);
    for (int i = 0; i < maxChannels; i++) {
        mAvailableWorkers.push_back(new RmtWorker());
    }
}

int RmtWorkerPool::getHardwareChannelCount() {
#if CONFIG_IDF_TARGET_ESP32
    return 8;
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3  
    return 4;
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
    return 2;
#else
    return 2; // Conservative default
#endif
}
```

## Key Implementation Insights

### Critical Async Decision Point
The worker pool must make a **per-controller decision** at `showPixels()` time:

```cpp
void RmtController5::showPixels() {
    RmtWorkerPool& pool = RmtWorkerPool::getInstance();
    
    // CRITICAL: This decision determines async vs blocking behavior
    if (pool.hasAvailableWorker()) {
        // ASYNC PATH: Start immediately and return
        pool.startControllerImmediate(this);
        return; // Returns immediately - preserves async!
    } else {
        // BLOCKING PATH: This specific controller must wait
        pool.startControllerQueued(this); // May block with polling
    }
}
```

### ESP-IDF RMT Channel Management
Direct integration with ESP-IDF RMT5 APIs instead of using led_strip wrapper:

```cpp
class RmtWorker {
private:
    rmt_channel_handle_t mChannel;
    rmt_encoder_handle_t mEncoder;
    
public:
    // Direct RMT channel creation for maximum control
    bool createChannel(int pin) {
        rmt_tx_channel_config_t config = {
            .gpio_num = pin,
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10000000,
            .mem_block_symbols = 64,
            .trans_queue_depth = 1, // Single transmission per worker
        };
        
        ESP_ERROR_CHECK(rmt_new_tx_channel(&config, &mChannel));
        
        // Register callback for async completion
        rmt_tx_event_callbacks_t callbacks = {
            .on_trans_done = onTransComplete,
        };
        ESP_ERROR_CHECK(rmt_tx_register_event_callbacks(mChannel, &callbacks, this));
        
        return true;
    }
    
    void transmitAsync(uint8_t* pixelData, size_t dataSize) {
        // Direct transmission - bypasses led_strip wrapper
        ESP_ERROR_CHECK(rmt_enable(mChannel));
        ESP_ERROR_CHECK(rmt_transmit(mChannel, mEncoder, pixelData, dataSize, &mTxConfig));
        // Returns immediately - async transmission started
    }
};
```

### Polling Strategy Implementation
Use `delayMicroseconds(100)` only for queued controllers:

```cpp
void RmtWorkerPool::startControllerQueued(RmtController5* controller) {
    // Add to queue
    mQueuedControllers.push_back(controller);
    
    // Poll until this controller gets a worker
    while (true) {
        if (RmtWorker* worker = tryAcquireWorker()) {
            // Remove from queue and start
            mQueuedControllers.remove(controller);
            startControllerImmediate(controller, worker);
            break;
        }
        
        // Brief delay to prevent busy-wait
        delayMicroseconds(100);
        
        // Yield periodically for FreeRTOS
        static uint32_t pollCount = 0;
        if (++pollCount % 50 == 0) {
            yield();
        }
    }
}
```

## Implementation Plan

### Phase 1: Core Infrastructure
1. **Create `RmtWorkerPool` singleton**
   - Basic worker management (acquire/release)
   - Controller registration system
   - Thread-safe operations with mutexes

2. **Implement `RmtWorker` class**
   - Worker lifecycle management
   - Configuration and reconfiguration logic
   - Completion callbacks

3. **Modify `RmtController5`**
   - Add persistent pixel buffer
   - Integrate with worker pool
   - Maintain backward-compatible API

### Phase 2: Coordination Logic
1. **Implement coordinated draw cycle**
   - Batch processing similar to RMT4
   - Semaphore-based synchronization
   - Queue management for excess controllers

2. **Add worker completion handling**
   - Async completion callbacks
   - Automatic worker recycling
   - Next controller startup

### Phase 3: Optimization & Safety
1. **Optimize worker reconfiguration**
   - Minimize teardown/setup when possible
   - Cache compatible configurations
   - Efficient pin state management

2. **Add error handling**
   - Worker allocation failures
   - Transmission errors
   - Recovery mechanisms

3. **Memory optimization**
   - Minimize buffer copying
   - Efficient pixel data transfer
   - Memory pool for workers

### Phase 4: Testing & Integration
1. **Unit tests**
   - Worker pool operations
   - Controller coordination
   - Error scenarios

2. **Integration testing**
   - Multiple strip configurations
   - High load scenarios
   - Hardware limit validation

3. **Performance benchmarking**
   - Throughput comparison with RMT4
   - Memory usage analysis
   - Latency measurements

## CRITICAL: Async Behavior Preservation

**Key Requirement**: RMT5 currently provides async drawing where `endShowLeds()` returns immediately without waiting. This must be preserved when N ≤ K, and only use polling/waiting when N > K.

### Current RMT5 Async Flow
```cpp
// Current behavior - MUST PRESERVE when N ≤ K
void ClocklessController::endShowLeds(void *data) {
    CPixelLEDController<RGB_ORDER>::endShowLeds(data);
    mRMTController.showPixels();  // Calls drawAsync() - returns immediately!
}
```

### Async Strategy for Worker Pool

#### When N ≤ K (Preserve Full Async)
- **Direct Assignment**: Each controller gets dedicated worker immediately
- **No Waiting**: `endShowLeds()` returns immediately after starting transmission
- **Callback-Driven**: Use ESP-IDF `rmt_tx_event_callbacks_t::on_trans_done` for completion
- **Zero Overhead**: Maintain current performance characteristics

#### When N > K (Controlled Polling)
- **Immediate Start**: First K controllers start immediately (async)
- **Queue Remaining**: Controllers K+1 through N queue for workers
- **Polling Strategy**: Use `delayMicroseconds(100)` polling for queued controllers
- **Callback Coordination**: Workers signal completion via callbacks to start next queued controller

## ESP-IDF RMT5 Callback Integration

### Callback Registration Pattern
```cpp
class RmtWorker {
private:
    rmt_channel_handle_t mRmtChannel;
    RmtWorkerPool* mPool;
    
    static bool IRAM_ATTR onTransmissionComplete(
        rmt_channel_handle_t channel,
        const rmt_tx_done_event_data_t *edata,
        void *user_data) {
        
        RmtWorker* worker = static_cast<RmtWorker*>(user_data);
        worker->handleTransmissionComplete();
        return false; // No high-priority task woken
    }
    
public:
    bool initialize() {
        // Create RMT channel
        rmt_tx_channel_config_t tx_config = {
            .gpio_num = mPin,
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10000000, // 10MHz
            .mem_block_symbols = 64,
            .trans_queue_depth = 4,
        };
        
        if (rmt_new_tx_channel(&tx_config, &mRmtChannel) != ESP_OK) {
            return false;
        }
        
        // Register completion callback
        rmt_tx_event_callbacks_t callbacks = {
            .on_trans_done = onTransmissionComplete,
        };
        
        return rmt_tx_register_event_callbacks(mRmtChannel, &callbacks, this) == ESP_OK;
    }
    
    void handleTransmissionComplete() {
        // Signal pool that this worker is available
        mPool->onWorkerComplete(this);
    }
};
```

## Revised Worker Pool Architecture

### Async-Aware Worker Pool
```cpp
class RmtWorkerPool {
public:
    enum class DrawMode {
        ASYNC_ONLY,    // N ≤ K: All controllers async
        MIXED_MODE     // N > K: Some async, some polled
    };
    
    void executeDrawCycle() {
        const int numControllers = mRegisteredControllers.size();
        const int numWorkers = mAvailableWorkers.size();
        
        if (numControllers <= numWorkers) {
            // ASYNC_ONLY mode - preserve full async behavior
            executeAsyncOnlyMode();
        } else {
            // MIXED_MODE - async for first K, polling for rest
            executeMixedMode();
        }
    }
    
private:
    void executeAsyncOnlyMode() {
        // Start all controllers immediately - full async behavior preserved
        for (auto* controller : mRegisteredControllers) {
            RmtWorker* worker = acquireWorker(controller->getWorkerConfig());
            startControllerAsync(controller, worker);
        }
        // Return immediately - no waiting!
    }
    
    void executeMixedMode() {
        // Start first K controllers immediately (async)
        int startedCount = 0;
        for (auto* controller : mRegisteredControllers) {
            if (startedCount < mAvailableWorkers.size()) {
                RmtWorker* worker = acquireWorker(controller->getWorkerConfig());
                startControllerAsync(controller, worker);
                startedCount++;
            } else {
                // Queue remaining controllers
                mQueuedControllers.push_back(controller);
            }
        }
        
        // Poll for completion of queued controllers
        while (!mQueuedControllers.empty()) {
            delayMicroseconds(100); // Non-blocking poll interval
            // Callback-driven worker completion will process queue
        }
    }
    
    void onWorkerComplete(RmtWorker* worker) {
        // Called from ISR context via callback
        releaseWorker(worker);
        
        // Start next queued controller if available
        if (!mQueuedControllers.empty()) {
            RmtController5* nextController = mQueuedControllers.front();
            mQueuedControllers.pop_front();
            
            // Reconfigure worker and start transmission
            startControllerAsync(nextController, worker);
        }
    }
};
```

### Modified RmtController5 for Async Preservation
```cpp
class RmtController5 {
public:
    void showPixels() {
        // This method MUST return immediately when N ≤ K
        // Only block when this specific controller is queued (N > K)
        
        RmtWorkerPool& pool = RmtWorkerPool::getInstance();
        
        if (pool.canStartImmediately(this)) {
            // Async path - return immediately
            pool.startControllerImmediate(this);
        } else {
            // This controller is queued - must wait for worker
            pool.startControllerQueued(this);
        }
    }
};
```

## Polling Strategy Details

### Microsecond Polling Pattern
```cpp
void RmtWorkerPool::waitForQueuedControllers() {
    while (!mQueuedControllers.empty()) {
        // Non-blocking check for available workers
        if (hasAvailableWorker()) {
            processNextQueuedController();
        } else {
            // Short delay to prevent busy-waiting
            delayMicroseconds(100); // 100μs polling interval
        }
        
        // Yield to other tasks periodically
        static uint32_t pollCount = 0;
        if (++pollCount % 50 == 0) { // Every 5ms (50 * 100μs)
            yield();
        }
    }
}
```

### Callback-Driven Queue Processing
```cpp
void RmtWorker::handleTransmissionComplete() {
    // Called from ISR context - keep minimal
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Signal completion to pool
    xSemaphoreGiveFromISR(mPool->getCompletionSemaphore(), &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void RmtWorkerPool::processCompletionEvents() {
    // Called from main task context
    while (xSemaphoreTake(mCompletionSemaphore, 0) == pdTRUE) {
        // Process one completion event
        if (!mQueuedControllers.empty()) {
            RmtController5* nextController = mQueuedControllers.front();
            mQueuedControllers.pop_front();
            
            // Find available worker and start next transmission
            RmtWorker* worker = findAvailableWorker();
            if (worker) {
                startControllerAsync(nextController, worker);
            }
        }
    }
}
```

## Benefits

1. **Async Preservation**: Full async behavior maintained when N ≤ K
2. **Scalability**: Support unlimited LED strips (within memory constraints)
3. **Backward Compatibility**: Existing code works unchanged
4. **Resource Efficiency**: Optimal use of limited RMT hardware
5. **Controlled Blocking**: Only blocks when specific controller is queued
6. **Callback Efficiency**: ISR-driven completion for minimal latency
7. **Polling Optimization**: 100μs intervals prevent busy-waiting

## Considerations

1. **Memory Usage**: Each controller needs persistent pixel buffer
2. **Latency**: Worker switching adds small overhead
3. **Complexity**: More complex than direct mapping
4. **Debugging**: Pool coordination harder to debug than direct control

## Migration Path

The implementation maintains full backward compatibility. Existing code using `RmtController5` will automatically benefit from the worker pool without any changes required.

## CRITICAL: LED Buffer Transfer Analysis

### ESP-IDF LED Buffer Management

**Key Finding**: The ESP-IDF led_strip driver **supports external pixel buffers** but **cannot change buffers after creation**.

#### Current Buffer Architecture
```cpp
typedef struct {
    led_strip_t base;
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t strip_encoder;
    uint8_t *pixel_buf;                    // ← Buffer pointer (fixed at creation)
    bool pixel_buf_allocated_internally;   // ← Ownership flag
    // ... other fields
} led_strip_rmt_obj;
```

#### Buffer Creation Options
```cpp
led_strip_config_t strip_config = {
    .strip_gpio_num = pin,
    .max_leds = led_count,
    .external_pixel_buf = external_buffer,  // ← Can provide external buffer
    // ... other config
};
```

### Buffer Transfer Constraints

**❌ IMPOSSIBLE**: Change buffer pointer after `led_strip_new_rmt_device()` creation
- No API exists to modify `pixel_buf` after creation
- Buffer pointer is fixed for the lifetime of the led_strip object
- `rmt_transmit()` uses the internal `pixel_buf` pointer directly

**✅ POSSIBLE**: Use external buffers that can be swapped at the controller level
- ESP-IDF supports external pixel buffers via `external_pixel_buf` parameter
- External buffers are **not freed** by the driver when destroyed
- FastLED can manage buffer ownership and transfer

### Buffer Transfer Solution: Worker Reconfiguration

Since buffers cannot be moved between existing led_strip objects, **workers must be recreated** with new configurations when switching between controllers.

#### Worker Buffer Management Strategy
```cpp
class RmtWorker {
private:
    led_strip_handle_t mCurrentStrip;
    uint8_t* mCurrentBuffer;           // External buffer managed by FastLED
    WorkerConfig mCurrentConfig;       // Current configuration
    
public:
    // Reconfigure worker with new controller's buffer
    bool reconfigure(const WorkerConfig& newConfig, uint8_t* controllerBuffer) {
        // Must destroy and recreate led_strip with new buffer
        if (mCurrentStrip) {
            // Wait for completion first
            if (isTransmitting()) {
                led_strip_refresh_wait_done(mCurrentStrip);
            }
            
            // Clean teardown
            led_strip_del(mCurrentStrip);
            mCurrentStrip = nullptr;
        }
        
        // Create new led_strip with controller's external buffer
        led_strip_config_t config = {
            .strip_gpio_num = newConfig.pin,
            .max_leds = newConfig.ledCount,
            .external_pixel_buf = controllerBuffer,  // ← Use controller's buffer
            // ... other config from newConfig
        };
        
        led_strip_rmt_config_t rmt_config = {
            // ... RMT configuration
        };
        
        esp_err_t result = led_strip_new_rmt_device(&config, &rmt_config, &mCurrentStrip);
        if (result != ESP_OK) {
            return false;
        }
        
        mCurrentBuffer = controllerBuffer;
        mCurrentConfig = newConfig;
        return true;
    }
    
    // Start transmission using the current external buffer
    bool startTransmission() {
        if (!mCurrentStrip) return false;
        
        // Buffer data is already in mCurrentBuffer (controller's buffer)
        // led_strip will read from this external buffer during transmission
        return led_strip_refresh_async(mCurrentStrip) == ESP_OK;
    }
};
```

#### Controller Buffer Management
```cpp
class RmtController5 {
private:
    fl::vector<uint8_t> mPersistentBuffer;  // Controller owns its buffer
    WorkerConfig mWorkerConfig;
    
public:
    void loadPixelData(PixelIterator& pixels) {
        // Store pixel data in persistent buffer
        const int bytesPerPixel = mWorkerConfig.isRgbw ? 4 : 3;
        const int bufferSize = pixels.size() * bytesPerPixel;
        
        mPersistentBuffer.resize(bufferSize);
        
        // Load pixel data into our persistent buffer
        uint8_t* bufPtr = mPersistentBuffer.data();
        if (mWorkerConfig.isRgbw) {
            while (pixels.has(1)) {
                uint8_t r, g, b, w;
                pixels.loadAndScaleRGBW(&r, &g, &b, &w);
                *bufPtr++ = r; *bufPtr++ = g; *bufPtr++ = b; *bufPtr++ = w;
                pixels.advanceData();
                pixels.stepDithering();
            }
        } else {
            while (pixels.has(1)) {
                uint8_t r, g, b;
                pixels.loadAndScaleRGB(&r, &g, &b);
                *bufPtr++ = r; *bufPtr++ = g; *bufPtr++ = b;
                pixels.advanceData();
                pixels.stepDithering();
            }
        }
    }
    
    // Provide buffer to worker pool
    uint8_t* getPixelBuffer() { return mPersistentBuffer.data(); }
    size_t getBufferSize() const { return mPersistentBuffer.size(); }
};
```

#### Worker Pool Buffer Coordination
```cpp
class RmtWorkerPool {
public:
    void startController(RmtController5* controller, RmtWorker* worker) {
        // Get controller's buffer and configuration
        uint8_t* controllerBuffer = controller->getPixelBuffer();
        const WorkerConfig& config = controller->getWorkerConfig();
        
        // Reconfigure worker with controller's buffer
        if (!worker->reconfigure(config, controllerBuffer)) {
            // Handle reconfiguration failure
            return;
        }
        
        // Start transmission - worker will read from controller's buffer
        worker->startTransmission();
    }
    
    void onWorkerComplete(RmtWorker* worker) {
        // Worker completed transmission
        releaseWorker(worker);
        
        // Start next queued controller if available
        if (!mQueuedControllers.empty()) {
            RmtController5* nextController = mQueuedControllers.front();
            mQueuedControllers.pop_front();
            
            // Reconfigure same worker with next controller's buffer
            startController(nextController, worker);
        }
    }
};
```

### Buffer Transfer Implementation Details

#### Memory Safety
- **Controller Ownership**: Each `RmtController5` owns its persistent buffer
- **External Buffer Contract**: ESP-IDF won't free external buffers
- **Worker Lifecycle**: Workers destroy/recreate led_strip objects as needed
- **Buffer Validity**: Controllers must keep buffers valid during transmission

#### Performance Considerations
- **Reconfiguration Cost**: Creating new led_strip objects has overhead
- **Buffer Copying**: No copying needed - workers use external buffers directly
- **Memory Efficiency**: Only one buffer per controller (no duplication)

#### Error Handling
- **Reconfiguration Failures**: Handle led_strip creation failures gracefully
- **Buffer Size Mismatches**: Validate buffer sizes during reconfiguration
- **Transmission Errors**: Proper cleanup on transmission failures

### Buffer Transfer Summary

**✅ SOLUTION**: Worker reconfiguration with external buffers
1. **Controllers maintain persistent buffers** containing pixel data
2. **Workers destroy/recreate led_strip objects** when switching controllers
3. **External buffer pointers** are passed during led_strip creation
4. **ESP-IDF transmits directly** from controller's external buffer
5. **No buffer copying** required - efficient memory usage

**❌ NOT POSSIBLE**: Direct buffer pointer swapping
- ESP-IDF led_strip API doesn't support changing buffers after creation
- Internal `pixel_buf` pointer is fixed at creation time

**🔧 IMPLEMENTATION REQUIREMENT**: Workers must support full reconfiguration
- Teardown current led_strip object
- Create new led_strip with different external buffer
- Handle reconfiguration failures and cleanup

## Future Enhancements

1. **Priority System**: Allow high-priority strips to get workers first
2. **Smart Batching**: Group compatible strips to minimize reconfiguration
3. **Dynamic Scaling**: Adjust worker count based on usage patterns
4. **Metrics**: Add performance monitoring and statistics
5. **Buffer Pool Optimization**: Cache led_strip objects for common configurations