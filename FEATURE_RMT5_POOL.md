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

## Benefits

1. **Scalability**: Support unlimited LED strips (within memory constraints)
2. **Backward Compatibility**: Existing code works unchanged
3. **Resource Efficiency**: Optimal use of limited RMT hardware
4. **Reliability**: Proper error handling and recovery
5. **Performance**: Minimal overhead for worker switching

## Considerations

1. **Memory Usage**: Each controller needs persistent pixel buffer
2. **Latency**: Worker switching adds small overhead
3. **Complexity**: More complex than direct mapping
4. **Debugging**: Pool coordination harder to debug than direct control

## Migration Path

The implementation maintains full backward compatibility. Existing code using `RmtController5` will automatically benefit from the worker pool without any changes required.

## Future Enhancements

1. **Priority System**: Allow high-priority strips to get workers first
2. **Smart Batching**: Group compatible strips to minimize reconfiguration
3. **Dynamic Scaling**: Adjust worker count based on usage patterns
4. **Metrics**: Add performance monitoring and statistics