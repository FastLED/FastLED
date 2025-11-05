#if defined(__IMXRT1062__)  // Teensy 4.0/4.1 only

#include "ObjectFLEDDmaManager.h"
#include "fl/delay.h"

namespace fl {

// Define static members
DMAMEM uint32_t ObjectFLEDDmaManager::bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));
DMAMEM uint32_t ObjectFLEDDmaManager::bitmask[4] __attribute__((used, aligned(32)));

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

void ObjectFLEDDmaManager::waitForCompletion() {
    // Spin-wait for DMA completion with periodic delays
    while (!dma3.complete()) {
        delayMicroseconds(10);
    }
}

bool ObjectFLEDDmaManager::isBusy() {
    return !dma3.complete();
}

} // namespace fl

#endif // __IMXRT1062__
