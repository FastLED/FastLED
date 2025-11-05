#pragma once

#if defined(__IMXRT1062__)  // Teensy 4.0/4.1 only

#include <Arduino.h>
#include "DMAChannel.h"

namespace fl {

// Forward declarations
#ifndef BYTES_PER_DMA
#define BYTES_PER_DMA 20
#endif

/// Singleton manager for shared ObjectFLED DMA resources
///
/// Coordinates exclusive access to Teensy 4.x TMR4/DMA hardware across
/// multiple ObjectFLED instances. Provides acquire/release semantics.
class ObjectFLEDDmaManager {
  public:
    static ObjectFLEDDmaManager& getInstance() {
        static ObjectFLEDDmaManager instance;
        return instance;
    }

    void acquire(void* owner);
    void release(void* owner);
    void waitForCompletion();
    bool isBusy();
    void* getCurrentOwner() const { return mCurrentOwner; }

    // Shared DMA resources
    DMAChannel dma1;
    DMAChannel dma2;
    DMAChannel dma3;
    DMASetting dma2next;

    // DMAMEM buffers must be static to work with section attributes
    static DMAMEM uint32_t bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));
    static DMAMEM uint32_t bitmask[4] __attribute__((used, aligned(32)));

    // Shared state for ISR
    volatile uint32_t framebuffer_index = 0;
    uint8_t* frameBuffer = nullptr;
    uint32_t numbytes = 0;
    uint8_t numpins = 0;
    uint8_t pin_bitnum[NUM_DIGITAL_PINS];
    uint8_t pin_offset[NUM_DIGITAL_PINS];

  private:
    ObjectFLEDDmaManager() = default;
    ~ObjectFLEDDmaManager() = default;
    ObjectFLEDDmaManager(const ObjectFLEDDmaManager&) = delete;
    ObjectFLEDDmaManager& operator=(const ObjectFLEDDmaManager&) = delete;

    void* mCurrentOwner = nullptr;
};

} // namespace fl

#endif // __IMXRT1062__
