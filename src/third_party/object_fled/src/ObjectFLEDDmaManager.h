#pragma once

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)  // Teensy 4.0/4.1 only

#include <Arduino.h>
#include "DMAChannel.h"

namespace fl {

// Forward declarations
// BYTES_PER_DMA controls the dma2 ESG-chained refill chunk size. The ISR
// runs on each major-loop completion and has only one WS2812 bit period
// (~1.25 us) of slack before the next TMR4 trigger reads from the refilled
// half. The ISR's memset + fillbits + dcache_flush takes ~10 us at 600 MHz
// CPU; at BYTES_PER_DMA=60 (480 bits = 600 us per chunk) the race produced
// the residual 0->1 phantom bit flips clustered at 20-LED boundaries
// observed in #3406. At BYTES_PER_DMA=120 the major loop runs 1200 us, so
// the ISR has plenty of margin to refill before the next chunk starts.
// Memory cost: bitdata grows from 15 KB to 30 KB in DMAMEM (OCRAM2).
#ifndef BYTES_PER_DMA
// EXPERIMENT: 150 makes our 100-LED test fit in a single major loop
// (numbytes=300 <= BYTES_PER_DMA*2=300) so the ESG/ISR refill path is
// completely bypassed. If the residual #3406 byte corruption disappears
// with this setting, the bug IS in the refill chain.
#define BYTES_PER_DMA 150
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

    // Shared state for ISR. These are written once in showInternal()
    // before DMA arms and then read by the ISR refill path. The ARM Cortex-M7
    // strongly-ordered memory model + the explicit dma.enable() that follows
    // every showInternal write ensures the ISR sees coherent values without
    // requiring volatile qualifiers (volatile would break memcpy of the
    // array source/destinations). Kept non-volatile; #3406 audit Agent 5
    // was conservative.
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

#endif // FL_IS_TEENSY_4X
