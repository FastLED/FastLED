#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)  // Teensy 4.0/4.1 only

#include "ObjectFLEDDmaManager.h"
#include "fl/system/delay.h"

namespace fl {

// Define static members
DMAMEM uint32_t ObjectFLEDDmaManager::bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));
DMAMEM uint32_t ObjectFLEDDmaManager::bitmask[4] __attribute__((used, aligned(32)));

// #3416 OF-HIGH-1 / CodeRabbit-flagged: save/restore PRIMASK rather than
// unconditionally re-enabling interrupts, so we don't clobber the
// caller's IRQ-disabled state if we're invoked from inside another
// critical section. Inline asm avoids needing the CMSIS header.
static inline uint32_t fl_save_primask_disable() {
    uint32_t pm;
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r"(pm) :: "memory");
    return pm;
}
static inline void fl_restore_primask(uint32_t pm) {
    __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
}

void ObjectFLEDDmaManager::acquire(void* owner) {
    // Wait for any current transmission to complete
    waitForCompletion();

    const uint32_t pm = fl_save_primask_disable();
    mCurrentOwner = owner;
    fl_restore_primask(pm);
}

void ObjectFLEDDmaManager::release(void* owner) {
    const uint32_t pm = fl_save_primask_disable();
    if (mCurrentOwner != owner) {
        fl_restore_primask(pm);
        return;
    }
    mCurrentOwner = nullptr;
    fl_restore_primask(pm);
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
