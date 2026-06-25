#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)  // Teensy 4.0/4.1 only

#include "ObjectFLEDDmaManager.h"
#include "fl/system/delay.h"

namespace fl {

// Define static members
DMAMEM uint32_t ObjectFLEDDmaManager::bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));
DMAMEM uint32_t ObjectFLEDDmaManager::bitmask[4] __attribute__((used, aligned(32)));

namespace {

void markDmaStopped(DMAChannel& channel) {
    channel.disable();
    channel.clearInterrupt();
    if (channel.TCD != nullptr) {
        channel.TCD->CSR = DMA_TCD_CSR_DREQ | DMA_TCD_CSR_DONE;
    }
}

} // namespace

void ObjectFLEDDmaManager::acquire(void* owner) {
    // Wait for any current transmission to complete
    waitForCompletion();

    // Set ownership
    mCurrentOwner = owner;
}

void ObjectFLEDDmaManager::release(void* owner) {
    // Validate ownership
    if (mCurrentOwner != owner) {
        // Ownership mismatch - ignore for now
        return;
    }

    // Release ownership
    mCurrentOwner = nullptr;
}

bool ObjectFLEDDmaManager::waitForCompletion(uint32_t timeout_us) {
    const uint32_t start = micros();

    while (!dma3.complete()) {
        if (timeout_us > 0 && static_cast<uint32_t>(micros() - start) >= timeout_us) {
            markDmaStopped(dma1);
            markDmaStopped(dma2);
            markDmaStopped(dma3);
            return false;
        }
        delayMicroseconds(10);
    }

    return true;
}

bool ObjectFLEDDmaManager::isBusy() {
    return !dma3.complete();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
