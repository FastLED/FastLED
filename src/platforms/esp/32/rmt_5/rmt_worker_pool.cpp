#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt_worker_pool.h"
#include "idf5_rmt.h"
#include "fl/assert.h"
#include "fl/algorithm.h"
#include "platforms/esp/32/esp_log_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>  // for memcpy
#include <cstdlib>  // for malloc, free

#define RMT_WORKER_POOL_TAG "rmt_worker_pool"

namespace fl {

// Static completion callback for worker pool
void RmtWorkerPool::staticWorkerCompletionCallback(RmtWorker* worker) {
    RmtWorkerPool::getInstance().onWorkerComplete(worker);
}

//==============================================================================
// RmtWorker Implementation
//==============================================================================

RmtWorker::RmtWorker() 
    : mCurrentStrip(nullptr)
    , mWorkerBuffer(nullptr)
    , mBufferCapacity(0)
    , mIsAvailable(true)
    , mTransmissionActive(false)
    , mIsConfigured(false)
    , mCompletionCallback(nullptr) {
}

RmtWorker::~RmtWorker() {
    reset();
    releaseBuffer();
}

bool RmtWorker::configure(const RmtWorkerConfig& config) {
    // Check if reconfiguration is needed
    if (mIsConfigured && isConfiguredFor(config)) {
        return true; // Already configured correctly
    }
    
    // Wait for any pending transmission
    if (mTransmissionActive) {
        waitForCompletion();
    }
    
    // Tear down current configuration if needed
    if (mCurrentStrip && (!mIsConfigured || !mCurrentConfig.isCompatibleWith(config))) {
        delete mCurrentStrip;
        mCurrentStrip = nullptr;
        mIsConfigured = false;
    }
    
    // Ensure we have adequate buffer capacity
    const size_t requiredBufferSize = config.ledCount * (config.isRgbw ? 4 : 3);
    if (!ensureBufferCapacity(requiredBufferSize)) {
        FASTLED_ESP_LOGE(RMT_WORKER_POOL_TAG, "Failed to allocate buffer of size %zu", requiredBufferSize);
        return false;
    }
    
    // Create new strip if needed
    if (!mCurrentStrip) {
        mCurrentStrip = IRmtStrip::CreateWithExternalBuffer(
            config.pin, config.ledCount, config.isRgbw,
            config.t0h, config.t0l, config.t1h, config.t1l, 
            config.reset, mWorkerBuffer, config.dmaMode, 
            config.interruptPriority
        );
        
        if (!mCurrentStrip) {
            FASTLED_ESP_LOGE(RMT_WORKER_POOL_TAG, "Failed to create RMT strip for pin %d", config.pin);
            return false;
        }
    }
    
    mCurrentConfig = config;
    mIsConfigured = true;
    return true;
}

void RmtWorker::loadPixelData(const uint8_t* pixelData, size_t dataSize) {
    FL_ASSERT(mCurrentStrip != nullptr, "Worker not configured");
    FL_ASSERT(mWorkerBuffer != nullptr, "Worker buffer not allocated");
    FL_ASSERT(dataSize <= mBufferCapacity, "Data size exceeds buffer capacity");
    
    // Copy pixel data to worker's buffer
    memcpy(mWorkerBuffer, pixelData, dataSize);
    
    // Load data into the RMT strip (this updates the strip's internal state)
    const uint32_t pixelCount = dataSize / (mCurrentConfig.isRgbw ? 4 : 3);
    const uint8_t* bufPtr = mWorkerBuffer;
    
    if (mCurrentConfig.isRgbw) {
        for (uint32_t i = 0; i < pixelCount; i++) {
            uint8_t r = *bufPtr++;
            uint8_t g = *bufPtr++;
            uint8_t b = *bufPtr++;
            uint8_t w = *bufPtr++;
            mCurrentStrip->setPixelRGBW(i, r, g, b, w);
        }
    } else {
        for (uint32_t i = 0; i < pixelCount; i++) {
            uint8_t r = *bufPtr++;
            uint8_t g = *bufPtr++;
            uint8_t b = *bufPtr++;
            mCurrentStrip->setPixel(i, r, g, b);
        }
    }
}

void RmtWorker::startTransmission() {
    FL_ASSERT(mCurrentStrip != nullptr, "Worker not configured");
    FL_ASSERT(!mTransmissionActive, "Transmission already active");
    
    mTransmissionActive = true;
    mIsAvailable = false;
    
    // Start async transmission
    mCurrentStrip->drawAsync();
    
    // Note: Completion will be detected by polling in waitForCompletion()
    // ESP-IDF led_strip doesn't provide completion callbacks directly
}

void RmtWorker::waitForCompletion() {
    if (mCurrentStrip && mTransmissionActive) {
        mCurrentStrip->waitDone();
        mTransmissionActive = false;
        
        // Notify completion callback
        if (mCompletionCallback) {
            mCompletionCallback(this);
        }
    }
}

bool RmtWorker::checkTransmissionComplete() {
    if (mCurrentStrip && mTransmissionActive) {
        // Check if transmission is still active without blocking
        if (!mCurrentStrip->isDrawing()) {
            mTransmissionActive = false;
            
            // Notify completion callback
            if (mCompletionCallback) {
                mCompletionCallback(this);
            }
            return true;
        }
    }
    return false;
}

bool RmtWorker::isConfiguredFor(const RmtWorkerConfig& config) const {
    return mIsConfigured && mCurrentConfig == config;
}

void RmtWorker::reset() {
    // Wait for any active transmission
    waitForCompletion();
    
    // Clean up strip
    if (mCurrentStrip) {
        delete mCurrentStrip;
        mCurrentStrip = nullptr;
    }
    
    mIsAvailable = true;
    mTransmissionActive = false;
    mIsConfigured = false;
}

void RmtWorker::setCompletionCallback(void (*callback)(RmtWorker*)) {
    mCompletionCallback = callback;
}

void RmtWorker::transmissionCompleteHandler(void* arg) {
    RmtWorker* worker = static_cast<RmtWorker*>(arg);
    worker->handleTransmissionComplete();
}

void RmtWorker::handleTransmissionComplete() {
    mTransmissionActive = false;
    
    if (mCompletionCallback) {
        mCompletionCallback(this);
    }
}

bool RmtWorker::ensureBufferCapacity(size_t requiredSize) {
    if (mBufferCapacity >= requiredSize) {
        return true; // Already have enough capacity
    }
    
    // Release old buffer
    releaseBuffer();
    
    // Allocate new buffer
    mWorkerBuffer = static_cast<uint8_t*>(malloc(requiredSize));
    if (!mWorkerBuffer) {
        mBufferCapacity = 0;
        return false;
    }
    
    mBufferCapacity = requiredSize;
    return true;
}

void RmtWorker::releaseBuffer() {
    if (mWorkerBuffer) {
        free(mWorkerBuffer);
        mWorkerBuffer = nullptr;
        mBufferCapacity = 0;
    }
}

//==============================================================================
// RmtWorkerPool Implementation
//==============================================================================

RmtWorkerPool& RmtWorkerPool::getInstance() {
    static RmtWorkerPool instance;
    return instance;
}

RmtWorkerPool::RmtWorkerPool() 
    : mActiveDrawCount(0)
    , mCompletedDrawCount(0)
    , mDrawCycleActive(false) {
    
    // Create synchronization primitives
    mPoolMutex = xSemaphoreCreateMutex();
    mDrawSemaphore = xSemaphoreCreateBinary();
    mCompletionSemaphore = xSemaphoreCreateBinary();
    
    FL_ASSERT(mPoolMutex != nullptr, "Failed to create pool mutex");
    FL_ASSERT(mDrawSemaphore != nullptr, "Failed to create draw semaphore");
    FL_ASSERT(mCompletionSemaphore != nullptr, "Failed to create completion semaphore");
    
    // Give the draw semaphore initially
    xSemaphoreGive(mDrawSemaphore);
    
    initialize();
}

RmtWorkerPool::~RmtWorkerPool() {
    // Clean up workers
    for (RmtWorker* worker : mAvailableWorkers) {
        delete worker;
    }
    for (RmtWorker* worker : mBusyWorkers) {
        delete worker;
    }
    
    // Clean up buffer pool
    for (uint8_t* buffer : mBufferPool) {
        if (buffer) {
            free(buffer);
        }
    }
    
    // Clean up synchronization primitives
    if (mPoolMutex) {
        vSemaphoreDelete(mPoolMutex);
    }
    if (mDrawSemaphore) {
        vSemaphoreDelete(mDrawSemaphore);
    }
    if (mCompletionSemaphore) {
        vSemaphoreDelete(mCompletionSemaphore);
    }
}

void RmtWorkerPool::initialize() {
    const int maxChannels = getHardwareChannelCount();
    
    FASTLED_ESP_LOGI(RMT_WORKER_POOL_TAG, "Initializing RMT worker pool with %d workers", maxChannels);
    
    // Create workers
    mAvailableWorkers.reserve(maxChannels);
    for (int i = 0; i < maxChannels; i++) {
        RmtWorker* worker = new RmtWorker();
        worker->setCompletionCallback(staticWorkerCompletionCallback);
        mAvailableWorkers.push_back(worker);
    }
}

int RmtWorkerPool::getHardwareChannelCount() const {
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

void RmtWorkerPool::registerController(RmtController5* controller) {
    FL_ASSERT(controller != nullptr, "Controller cannot be null");
    
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    
    // Check if already registered
    auto it = fl::find(mRegisteredControllers.begin(), mRegisteredControllers.end(), controller);
    if (it == mRegisteredControllers.end()) {
        mRegisteredControllers.push_back(controller);
        FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Registered controller %p", controller);
    }
    
    xSemaphoreGive(mPoolMutex);
}

void RmtWorkerPool::unregisterController(RmtController5* controller) {
    FL_ASSERT(controller != nullptr, "Controller cannot be null");
    
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    
    // Remove from registered controllers
    auto it = fl::find(mRegisteredControllers.begin(), mRegisteredControllers.end(), controller);
    if (it != mRegisteredControllers.end()) {
        mRegisteredControllers.erase(it);
        FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Unregistered controller %p", controller);
    }
    
    // Remove from queued controllers if present
    auto qit = fl::find(mQueuedControllers.begin(), mQueuedControllers.end(), controller);
    if (qit != mQueuedControllers.end()) {
        mQueuedControllers.erase(qit);
    }
    
    xSemaphoreGive(mPoolMutex);
}

void RmtWorkerPool::executeDrawCycle() {
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    
    if (mDrawCycleActive) {
        // Another draw cycle is already in progress
        xSemaphoreGive(mPoolMutex);
        return;
    }
    
    const int numControllers = mRegisteredControllers.size();
    const int numWorkers = mAvailableWorkers.size();
    
    if (numControllers == 0) {
        xSemaphoreGive(mPoolMutex);
        return;
    }
    
    mDrawCycleActive = true;
    mActiveDrawCount = 0;
    mCompletedDrawCount = 0;
    mQueuedControllers.clear();
    
    FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Starting draw cycle: %d controllers, %d workers", 
                     numControllers, numWorkers);
    
    if (numControllers <= numWorkers) {
        // ASYNC_ONLY mode - preserve full async behavior
        executeAsyncOnlyMode();
    } else {
        // MIXED_MODE - async for first K, polling for rest
        executeMixedMode();
    }
    
    mDrawCycleActive = false;
    xSemaphoreGive(mPoolMutex);
}

void RmtWorkerPool::executeAsyncOnlyMode() {
    FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Executing async-only mode");
    
    // Start all controllers immediately - full async behavior preserved
    for (RmtController5* controller : mRegisteredControllers) {
        if (!mAvailableWorkers.empty()) {
            RmtWorker* worker = mAvailableWorkers.back();
            mAvailableWorkers.pop_back();
            mBusyWorkers.push_back(worker);
            
            startControllerWithWorker(controller, worker);
            mActiveDrawCount++;
        }
    }
    // Return immediately - no waiting!
}

void RmtWorkerPool::executeMixedMode() {
    FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Executing mixed mode");
    
    // Start first K controllers immediately (async)
    int startedCount = 0;
    for (RmtController5* controller : mRegisteredControllers) {
        if (startedCount < static_cast<int>(mAvailableWorkers.size())) {
            RmtWorker* worker = mAvailableWorkers.back();
            mAvailableWorkers.pop_back();
            mBusyWorkers.push_back(worker);
            
            startControllerWithWorker(controller, worker);
            mActiveDrawCount++;
            startedCount++;
        } else {
            // Queue remaining controllers
            mQueuedControllers.push_back(controller);
        }
    }
    
    // Poll for completion of queued controllers
    while (!mQueuedControllers.empty()) {
        // Brief delay to prevent busy-waiting
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms delay
        
        // Process any completion events
        processCompletionEvents();
    }
    
    // Wait for all transmissions to complete
    while (mCompletedDrawCount < static_cast<int>(mRegisteredControllers.size())) {
        vTaskDelay(pdMS_TO_TICKS(1));
        processCompletionEvents();
    }
}

bool RmtWorkerPool::canStartImmediately(RmtController5* controller) {
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    bool canStart = !mAvailableWorkers.empty();
    xSemaphoreGive(mPoolMutex);
    return canStart;
}

void RmtWorkerPool::startControllerImmediate(RmtController5* controller) {
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    
    if (!mAvailableWorkers.empty()) {
        RmtWorker* worker = mAvailableWorkers.back();
        mAvailableWorkers.pop_back();
        mBusyWorkers.push_back(worker);
        
        startControllerWithWorker(controller, worker);
    }
    
    xSemaphoreGive(mPoolMutex);
}

void RmtWorkerPool::startControllerQueued(RmtController5* controller) {
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    mQueuedControllers.push_back(controller);
    xSemaphoreGive(mPoolMutex);
    
    // Poll until this controller gets a worker
    while (true) {
        xSemaphoreTake(mPoolMutex, portMAX_DELAY);
        
        // Check if this controller is still in the queue
        auto it = fl::find(mQueuedControllers.begin(), mQueuedControllers.end(), controller);
        if (it == mQueuedControllers.end()) {
            // Controller has been processed
            xSemaphoreGive(mPoolMutex);
            break;
        }
        
        xSemaphoreGive(mPoolMutex);
        
        // Brief delay to prevent busy-waiting
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms delay
        
        // Process completion events
        processCompletionEvents();
    }
}

RmtWorker* RmtWorkerPool::acquireWorker() {
    if (mAvailableWorkers.empty()) {
        return nullptr;
    }
    
    RmtWorker* worker = mAvailableWorkers.back();
    mAvailableWorkers.pop_back();
    mBusyWorkers.push_back(worker);
    return worker;
}

void RmtWorkerPool::releaseWorker(RmtWorker* worker) {
    // Remove from busy workers
    auto it = fl::find(mBusyWorkers.begin(), mBusyWorkers.end(), worker);
    if (it != mBusyWorkers.end()) {
        mBusyWorkers.erase(it);
    }
    
    // Add to available workers
    worker->reset();
    mAvailableWorkers.push_back(worker);
}

void RmtWorkerPool::onWorkerComplete(RmtWorker* worker) {
    // Signal completion
    xSemaphoreGive(mCompletionSemaphore);
}

void RmtWorkerPool::processCompletionEvents() {
    xSemaphoreTake(mPoolMutex, portMAX_DELAY);
    
    // Check all busy workers for completion (non-blocking)
    // Use index-based iteration to avoid iterator invalidation issues
    for (size_t i = 0; i < mBusyWorkers.size();) {
        RmtWorker* worker = mBusyWorkers[i];
        
        // Non-blocking completion check
        if (worker->checkTransmissionComplete() || !worker->isTransmissionActive()) {
            // Worker completed transmission
            mCompletedDrawCount++;
            
            if (!mQueuedControllers.empty()) {
                // Assign to next waiting controller
                RmtController5* nextController = mQueuedControllers.front();
                mQueuedControllers.erase(mQueuedControllers.begin());
                
                startControllerWithWorker(nextController, worker);
                i++; // Keep worker in busy list, advance index
            } else {
                // No waiting controllers - release worker
                mBusyWorkers.erase(mBusyWorkers.begin() + i);
                worker->reset();
                mAvailableWorkers.push_back(worker);
                // Don't increment i since we removed an element
            }
        } else {
            i++; // Advance to next worker
        }
    }
    
    xSemaphoreGive(mPoolMutex);
}

void RmtWorkerPool::startControllerWithWorker(RmtController5* controller, RmtWorker* worker) {
    FL_ASSERT(controller != nullptr, "Controller cannot be null");
    FL_ASSERT(worker != nullptr, "Worker cannot be null");
    
    FASTLED_ESP_LOGD(RMT_WORKER_POOL_TAG, "Starting controller %p with worker %p", controller, worker);
    
    // Get controller configuration and pixel data
    const RmtWorkerConfig& config = controller->getWorkerConfig();
    const uint8_t* pixelData = controller->getPixelBuffer();
    const size_t dataSize = controller->getBufferSize();
    
    // Configure worker for this controller
    if (!worker->configure(config)) {
        FASTLED_ESP_LOGE(RMT_WORKER_POOL_TAG, "Failed to configure worker for controller %p", controller);
        return;
    }
    
    // Load pixel data into worker
    worker->loadPixelData(pixelData, dataSize);
    
    // Start transmission
    worker->startTransmission();
}

RmtWorker* RmtWorkerPool::findCompatibleWorker(const RmtWorkerConfig& config) {
    for (RmtWorker* worker : mAvailableWorkers) {
        if (worker->isConfiguredFor(config) || 
            (worker->getCurrentConfig().isCompatibleWith(config))) {
            return worker;
        }
    }
    return nullptr;
}

bool RmtWorkerPool::assignWorkerToController(RmtController5* controller, RmtWorker* worker) {
    // TODO: Implement assignment logic
    // This requires access to controller's configuration and pixel data
    return false;
}

void RmtWorkerPool::processNextQueuedController() {
    if (mQueuedControllers.empty() || mAvailableWorkers.empty()) {
        return;
    }
    
    RmtController5* nextController = mQueuedControllers.front();
    mQueuedControllers.erase(mQueuedControllers.begin());
    
    RmtWorker* worker = mAvailableWorkers.back();
    mAvailableWorkers.pop_back();
    mBusyWorkers.push_back(worker);
    
    startControllerWithWorker(nextController, worker);
}

uint8_t* RmtWorkerPool::acquireBuffer(size_t size) {
    size_t poolSize = roundUpToPowerOf2(size);
    
    // Look for an existing buffer of adequate size
    for (size_t i = 0; i < mBufferPool.size(); i++) {
        if (mBufferPool[i] && mBufferSizes[i] >= poolSize) {
            uint8_t* buffer = mBufferPool[i];
            mBufferPool[i] = nullptr; // Mark as used
            return buffer;
        }
    }
    
    // Allocate new buffer
    return static_cast<uint8_t*>(malloc(poolSize));
}

void RmtWorkerPool::releaseBuffer(uint8_t* buffer, size_t size) {
    if (!buffer) return;
    
    size_t poolSize = roundUpToPowerOf2(size);
    
    // Find an empty slot or add to the end
    for (size_t i = 0; i < mBufferPool.size(); i++) {
        if (!mBufferPool[i]) {
            mBufferPool[i] = buffer;
            mBufferSizes[i] = poolSize;
            return;
        }
    }
    
    // Add to end if no empty slot found
    mBufferPool.push_back(buffer);
    mBufferSizes.push_back(poolSize);
}

size_t RmtWorkerPool::roundUpToPowerOf2(size_t size) {
    if (size <= 1) return 1;
    
    size_t power = 1;
    while (power < size) {
        power <<= 1;
    }
    return power;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
