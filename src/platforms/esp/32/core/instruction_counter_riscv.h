#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// Retired instructions on ESP32-C3/C6/H2 (FastLED#5376), from Espressif's
/// custom machine-mode performance counter: PCER 0x7E0 selects the event
/// (bit 0 cycles, bit 1 instructions), PCMR 0x7E1 enables counting, PCCR
/// 0x7E2 holds the count. The standard minstret/mcycle CSRs trap on these
/// cores.
///
/// ESP-IDF reads this counter as the CPU cycle counter
/// (esp_cpu_get_cycle_count), so stop() restores the cycle event, the mode
/// and a cycle count advanced by the caller's measured interval. A cycle
/// reader that runs between start() and stop() reads instructions; profile
/// only code that does not time itself with the cycle counter.
class InstructionCounter {
  public:
    static constexpr bool kSupported = true;

    void start() FL_NO_EXCEPT {
        asm volatile("csrr %0, 0x7E0" : "=r"(mSavedEvent));
        asm volatile("csrr %0, 0x7E1" : "=r"(mSavedMode));
        asm volatile("csrr %0, 0x7E2" : "=r"(mSavedCount));
        asm volatile("csrw 0x7E0, %0" ::"r"(1u << 1));  // event: instructions
        asm volatile("csrw 0x7E2, %0" ::"r"(0u));
        asm volatile("csrw 0x7E1, %0" ::"r"(1u));       // count enable
    }

    /// Instructions retired since start(). `elapsed_cycles` is the interval
    /// in CPU cycles, used to advance the restored cycle count.
    u32 stop(u32 elapsed_cycles) FL_NO_EXCEPT {
        u32 instructions;
        asm volatile("csrr %0, 0x7E2" : "=r"(instructions));
        asm volatile("csrw 0x7E0, %0" ::"r"(mSavedEvent));
        asm volatile("csrw 0x7E2, %0" ::"r"(mSavedCount + elapsed_cycles));
        asm volatile("csrw 0x7E1, %0" ::"r"(mSavedMode));
        return instructions;
    }

  private:
    u32 mSavedEvent = 0;
    u32 mSavedMode = 0;
    u32 mSavedCount = 0;
};

}  // namespace platforms
}  // namespace fl
