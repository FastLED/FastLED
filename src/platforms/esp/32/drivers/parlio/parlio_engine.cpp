/// @file parlio_engine.cpp
/// @brief ESP32-P4 PARLIO DMA engine implementation
///
/// Implementation of the ParlioEngine singleton - the powerhouse that manages
/// exclusive access to the ESP32-P4 PARLIO DMA hardware for parallel LED transmission.

#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_engine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fl/log.h"
#include "fl/singleton.h"

namespace fl {

// Concrete implementation - all platform-specific types hidden in cpp
class ParlioEngine : public IParlioEngine {
public:
    ParlioEngine()
        : mOwner(nullptr)
        , mBusy(false)
        , mSemaphore(nullptr)
    {
        // Create binary semaphore for hardware arbitration
        mSemaphore = xSemaphoreCreateBinary();
        if (!mSemaphore) {
            FL_WARN("PARLIO Engine: Failed to create semaphore!");
            return;
        }

        // Initially give semaphore (hardware available)
        xSemaphoreGive(mSemaphore);

        FL_DBG("PARLIO Engine: Initialized");
    }

    ~ParlioEngine() override {
        if (mSemaphore) {
            vSemaphoreDelete(mSemaphore);
            mSemaphore = nullptr;
        }
        FL_DBG("PARLIO Engine: Destroyed");
    }

    void acquire(void* owner) override;
    void release(void* owner) override;
    void waitForCompletion() override;
    bool isBusy() const override;
    void* getCurrentOwner() const override;

private:
    void* mOwner;                   ///< Current owner of the hardware (nullptr if idle)
    volatile bool mBusy;            ///< Flag indicating hardware is in use
    SemaphoreHandle_t mSemaphore;   ///< Binary semaphore for hardware arbitration
};

// Static method - returns interface but creates concrete singleton
IParlioEngine& IParlioEngine::getInstance() {
    return fl::Singleton<ParlioEngine>::instance();
}

void ParlioEngine::acquire(void* owner) {
    // Wait for hardware to become available (blocking)
    xSemaphoreTake(mSemaphore, portMAX_DELAY);

    mOwner = owner;
    mBusy = true;

    FL_DBG("PARLIO Engine: Acquired by " << owner);
}

void ParlioEngine::release(void* owner) {
    if (mOwner != owner) {
        FL_WARN("PARLIO Engine: Release called by non-owner "
               << owner << " (current owner: " << mOwner << ")");
        return;
    }

    FL_DBG("PARLIO Engine: Released by " << owner);

    // Mark hardware as available for next acquire
    mOwner = nullptr;
    mBusy = false;

    // Give semaphore to unblock next waiting driver
    xSemaphoreGive(mSemaphore);
}

void ParlioEngine::waitForCompletion() {
    // Try to acquire (will block if busy)
    xSemaphoreTake(mSemaphore, portMAX_DELAY);

    FL_DBG("PARLIO Engine: Wait completed");

    // Immediately release (we just wanted to wait)
    xSemaphoreGive(mSemaphore);
}

bool ParlioEngine::isBusy() const {
    return mBusy;
}

void* ParlioEngine::getCurrentOwner() const {
    return mOwner;
}

}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
