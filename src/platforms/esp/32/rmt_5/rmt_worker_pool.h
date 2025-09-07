#pragma once

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "strip_rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

namespace fl {

class RmtController5;

// Configuration for an RMT worker
struct RmtWorkerConfig {
    int pin;
    uint32_t ledCount;
    bool isRgbw;
    uint32_t t0h, t0l, t1h, t1l, reset;
    IRmtStrip::DmaMode dmaMode;
    uint8_t interruptPriority;
    
    bool operator==(const RmtWorkerConfig& other) const {
        return pin == other.pin &&
               ledCount == other.ledCount &&
               isRgbw == other.isRgbw &&
               t0h == other.t0h &&
               t0l == other.t0l &&
               t1h == other.t1h &&
               t1l == other.t1l &&
               reset == other.reset &&
               dmaMode == other.dmaMode &&
               interruptPriority == other.interruptPriority;
    }
    
    bool isCompatibleWith(const RmtWorkerConfig& other) const {
        // Check if configurations are compatible (can reuse RMT channel)
        return pin == other.pin &&
               isRgbw == other.isRgbw &&
               t0h == other.t0h &&
               t0l == other.t0l &&
               t1h == other.t1h &&
               t1l == other.t1l &&
               reset == other.reset &&
               dmaMode == other.dmaMode &&
               interruptPriority == other.interruptPriority;
        // Note: ledCount can be different for compatible configs
    }
};

// Individual RMT worker that can be assigned to different controllers
class RmtWorker {
public:
    RmtWorker();
    ~RmtWorker();
    
    // Worker lifecycle
    bool configure(const RmtWorkerConfig& config);
    void loadPixelData(const uint8_t* pixelData, size_t dataSize);
    void startTransmission();
    void waitForCompletion();
    
    // State management
    bool isAvailable() const { return mIsAvailable; }
    bool isConfiguredFor(const RmtWorkerConfig& config) const;
    bool isTransmissionActive() const { return mTransmissionActive; }
    bool checkTransmissionComplete();  // Non-blocking completion check
    void reset();
    
    // Get current configuration
    const RmtWorkerConfig& getCurrentConfig() const { return mCurrentConfig; }
    
    // Set completion callback
    void setCompletionCallback(void (*callback)(RmtWorker*));
    
private:
    IRmtStrip* mCurrentStrip;
    RmtWorkerConfig mCurrentConfig;
    uint8_t* mWorkerBuffer;
    size_t mBufferCapacity;
    bool mIsAvailable;
    bool mTransmissionActive;
    bool mIsConfigured;
    void (*mCompletionCallback)(RmtWorker*);
    
    // Static completion handler for ESP-IDF callbacks
    static void transmissionCompleteHandler(void* arg);
    void handleTransmissionComplete();
    
    // Buffer management
    bool ensureBufferCapacity(size_t requiredSize);
    void releaseBuffer();
};

// Singleton worker pool that manages RMT workers and coordinates drawing
class RmtWorkerPool {
public:
    static RmtWorkerPool& getInstance();
    
    // Controller management
    void registerController(RmtController5* controller);
    void unregisterController(RmtController5* controller);
    
    // Drawing coordination
    void executeDrawCycle();
    bool canStartImmediately(RmtController5* controller);
    void startControllerImmediate(RmtController5* controller);
    void startControllerQueued(RmtController5* controller);
    
    // Worker management
    RmtWorker* acquireWorker();
    void releaseWorker(RmtWorker* worker);
    
    // Completion handling
    void onWorkerComplete(RmtWorker* worker);
    
    // Configuration
    int getHardwareChannelCount() const;
    
private:
    RmtWorkerPool();
    ~RmtWorkerPool();
    
    // Prevent copying
    RmtWorkerPool(const RmtWorkerPool&) = delete;
    RmtWorkerPool& operator=(const RmtWorkerPool&) = delete;
    
    // Worker management
    fl::vector<RmtWorker*> mAvailableWorkers;
    fl::vector<RmtWorker*> mBusyWorkers;
    
    // Controller management
    fl::vector<RmtController5*> mRegisteredControllers;
    fl::vector<RmtController5*> mQueuedControllers;
    
    // Synchronization
    SemaphoreHandle_t mPoolMutex;
    SemaphoreHandle_t mDrawSemaphore;
    SemaphoreHandle_t mCompletionSemaphore;
    
    // State tracking
    int mActiveDrawCount;
    int mCompletedDrawCount;
    bool mDrawCycleActive;
    
    // Internal methods
    void initialize();
    void executeAsyncOnlyMode();
    void executeMixedMode();
    void processCompletionEvents();
    void startControllerWithWorker(RmtController5* controller, RmtWorker* worker);
    RmtWorker* findCompatibleWorker(const RmtWorkerConfig& config);
    bool assignWorkerToController(RmtController5* controller, RmtWorker* worker);
    void processNextQueuedController();
    
    // Buffer pool management
    fl::map<size_t, fl::vector<uint8_t*>> mBuffersBySize;
    uint8_t* acquireBuffer(size_t size);
    void releaseBuffer(uint8_t* buffer, size_t size);
    size_t roundUpToPowerOf2(size_t size);
    
    // Static completion callback
    static void staticWorkerCompletionCallback(RmtWorker* worker);
};

} // namespace fl

#endif // FASTLED_RMT5
