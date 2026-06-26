#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)  // Teensy 4.0/4.1 only

#include "ObjectFLEDDmaManager.h"
#include "fl/system/delay.h"

namespace fl {

// Define static members
DMAMEM uint32_t ObjectFLEDDmaManager::bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));
DMAMEM uint32_t ObjectFLEDDmaManager::bitmask[4] __attribute__((used, aligned(32)));

void ObjectFLEDDmaManager::acquire(void* owner) {
    // Wait for any current transmission to complete
    waitForCompletion();

    // #3416 OF-HIGH-1: protect mCurrentOwner against ISR/cross-instance
    // races. ObjectFLED is documented as singleton-coordinated but
    // nothing physically prevents a USB-CDC or SysTick ISR from calling
    // show() on a different instance while we're mid-acquire. Without
    // the noInterrupts() guard, two acquire() calls could both believe
    // they own the channel and scramble the in-flight TCD.
    noInterrupts();
    mCurrentOwner = owner;
    interrupts();
}

void ObjectFLEDDmaManager::release(void* owner) {
    noInterrupts();
    // Validate ownership while interrupts are disabled so we can't see
    // a half-set/half-cleared mCurrentOwner from a racing release.
    if (mCurrentOwner != owner) {
        interrupts();
        return;
    }
    mCurrentOwner = nullptr;
    interrupts();
}

void ObjectFLEDDmaManager::waitForCompletion() {
    // Spin-wait for DMA completion with periodic delays.
    while (!dma3.complete()) {
        delayMicroseconds(10);
    }
    // #3416 OF-MED-6: dma3.complete() reflects DMA major-loop done but
    // does NOT include the WS2812 reset latch period. If the caller is
    // about to destroy the LED buffer or hand the line off to another
    // peripheral, returning here can corrupt the final frame's tail
    // bits. The destructor wait path adds its own latch margin (see
    // ObjectFLED::~ObjectFLED) but defensive callers should also gate
    // on busy()==0 which factors in LATCH_DELAY. Documented here so
    // the next reader doesn't mistake "DMA done" for "frame done".
}

bool ObjectFLEDDmaManager::isBusy() {
    return !dma3.complete();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
