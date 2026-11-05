#pragma once

// IWYU pragma: private
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/has_include.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/cycle_type.h"
#include "platforms/cpu_frequency.h"
#include "platforms/is_platform.h"

#if defined(__AVR__)
#include <util/delay.h> // IWYU pragma: keep
#endif

#if defined(FL_IS_RP) && FL_HAS_INCLUDE(<pico/platform.h>)
#include <pico/platform.h> // IWYU pragma: keep
#define FL_SPI_PICO_CYCLE_WAIT 1
#endif

// Only the nine baseline ESP32 application-core variants have a timing
// proof here. Generic ESP_PLATFORM builds are included through FL_IS_ESP32;
// C61/S31 and future targets require a separately reviewed timing backend.
// IDF3 classic ESP32 lacks CONFIG_IDF_TARGET*: its classic-only CPU-frequency
// configuration plus Xtensa and IDF3 identify the supported legacy target.
// Arduino1.0.4 tools/sdk/include/config/sdkconfig.h defines that configuration.
#if defined(FL_IS_ESP32) && \
    ((defined(__XTENSA__) && \
      (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || \
       defined(CONFIG_IDF_TARGET_ESP32S3) || \
       (ESP_IDF_VERSION_MAJOR == 3 && !defined(CONFIG_IDF_TARGET) && \
        defined(CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ)))) || \
     (defined(__riscv) && \
      (defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
       defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
       defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4))))
#define FL_SPI_ESP_INSTRUCTION_WAIT 1
#endif

// Expand to literal instruction lines BEFORE the compiler estimates inline
// assembly size. Assembler .rept hides its true size and can make GCC choose
// an out-of-range short branch around an inlined wait (ClearCore GCC9).
#define FL_SPI_DELAY_REPEAT_2(INSTRUCTION) INSTRUCTION INSTRUCTION
#define FL_SPI_DELAY_REPEAT_4(INSTRUCTION) FL_SPI_DELAY_REPEAT_2(INSTRUCTION) FL_SPI_DELAY_REPEAT_2(INSTRUCTION)
#define FL_SPI_DELAY_REPEAT_8(INSTRUCTION) FL_SPI_DELAY_REPEAT_4(INSTRUCTION) FL_SPI_DELAY_REPEAT_4(INSTRUCTION)
#define FL_SPI_DELAY_REPEAT_16(INSTRUCTION) FL_SPI_DELAY_REPEAT_8(INSTRUCTION) FL_SPI_DELAY_REPEAT_8(INSTRUCTION)
#define FL_SPI_DELAY_REPEAT_32(INSTRUCTION) FL_SPI_DELAY_REPEAT_16(INSTRUCTION) FL_SPI_DELAY_REPEAT_16(INSTRUCTION)

namespace fl {
namespace detail {

#if defined(__arm__) && ((defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')) || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__))
// Cortex-M0/M0+ TRM instruction summary: ADD is one cycle. For the
// superscalar Cortex-M7, budget at most TWO instructions issued per cycle
// (Arm Cortex-M7 Processor Datasheet, "six-stage superscalar pipeline").
// Emit TWO additions per requested cycle; do not credit RAW dependencies or
// assume NOP latency. Single-issue cores deliberately receive extra padding.
// Sources: Arm DDI0432C table3-1 and Cortex-M7 Processor Datasheet:
// https://documentation-service.arm.com/static/5e906da3c8052b1608760e3d
// https://www.st.com/resource/en/product_training/STM32F7_System_Core.pdf
template <u8 Count> struct SpiArmPadding {
    FASTLED_FORCE_INLINE static void run(u32& dependency) FL_NO_EXCEPT {
        __asm__ __volatile__("adds %0, %0, #1\n\tadds %0, %0, #1"
                             : "+l"(dependency) : : "cc", "memory");
        SpiArmPadding<Count - 1>::run(dependency);
    }
};
template <> struct SpiArmPadding<0> {
    FASTLED_FORCE_INLINE static void run(u32&) FL_NO_EXCEPT {}
};
#endif

#if defined(FL_SPI_ESP_INSTRUCTION_WAIT)
// ESP32 LX6/LX7 and ESP-RISC-V application cores are scalar; TWO retained
// additions per requested cycle deliberately overpad them. This uses only
// the issue-width bound, not NOP latency or dependent-instruction timing.
// Primary sources: ESP32-S3 TRM1.7 instruction pipeline; ESP32-C3/C6/H2
// TRM1.1 core overview; ESP32-C5 TRM2.1 HP overview (v1.0, p49); P4 TRM1.1.
// C2 is ESP8684: its TRMv1.3 section1.1 (p22) specifies a scalar pipeline.
// https://www.espressif.com/sites/default/files/documentation/esp8684_technical_reference_manual_en.pdf#page=22
// https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
// https://documentation.espressif.com/esp32-c5_technical_reference_manual_en.pdf
// https://documentation.espressif.com/esp32-p4_technical_reference_manual_en.pdf
// LX6/LX7 compiler issue model: https://github.com/gcc-mirror/gcc/blob/master/gcc/config/xtensa/xtensa.md
// CPU padding remains conservative if the actual clock never exceeds the
// configured F_CPU. Migration, slower DFS clocks and interrupts add time;
// per-core counter resets/rewrites cannot shorten this instruction sequence.
// This is not evidence of peripheral-store completion or electrical timing.
template <u8 Count> struct SpiEspPadding {
    FASTLED_FORCE_INLINE static void run(u32& dependency) FL_NO_EXCEPT {
#if defined(__XTENSA__)
        __asm__ __volatile__("addi %0, %0, 1\n\taddi %0, %0, 1"
                             : "+a"(dependency) : : "memory");
#else
        __asm__ __volatile__("addi %0, %0, 1\n\taddi %0, %0, 1"
                             : "+r"(dependency) : : "memory");
#endif
        SpiEspPadding<Count - 1>::run(dependency);
    }
};
template <> struct SpiEspPadding<0> {
    FASTLED_FORCE_INLINE static void run(u32&) FL_NO_EXCEPT {}
};
#endif

// SPI-local timing only: this does not change the clockless delaycycles ABI.
// A non-positive request is always a no-op, including values below -10.
template <i64 Cycles, bool Positive = (Cycles > 0)> struct SpiCycleWait {
    FASTLED_FORCE_INLINE static void run() FL_NO_EXCEPT {}
};
template <i64 Cycles> struct SpiCycleWait<Cycles, true> {
    FASTLED_FORCE_INLINE static void run() FL_NO_EXCEPT {
#if defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
        // Host tests prove linkage/arguments/GPIO ordering, not physical time.
        __asm__ __volatile__("" : : : "memory");
#elif defined(__AVR__) && defined(__HAS_DELAY_CYCLES) && __HAS_DELAY_CYCLES
        // AVR cycle_t is signed16, but SPI phase requests are intentionally
        // wider. Each builtin argument stays within its unsigned32 ABI.
        for (u64 chunks = static_cast<u64>(Cycles) / 0x7fffffffu; chunks > 0; --chunks) {
            __builtin_avr_delay_cycles(0x7fffffffu);
        }
        __builtin_avr_delay_cycles(static_cast<u32>(Cycles % 0x7fffffffu));
#elif defined(FL_SPI_PICO_CYCLE_WAIT)
        // Pico SDK promises at least the requested count, independent of
        // Cortex-M0+/M33 instruction issue. Chunk to the API's u32 range.
        u64 remaining = Cycles;
        while (remaining > 0) {
            const u32 chunk = remaining > 0x7fffffffu ? 0x7fffffffu : static_cast<u32>(remaining);
            busy_wait_at_least_cycles(chunk);
            remaining -= chunk;
        }
#elif defined(__arm__) && ((defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')) || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__))
        u32 dependency = 0;
        // The counted loop is assembly so the compiler cannot unroll a
        // large compile-time request into an unbounded instruction sequence.
        u64 blocks = static_cast<u64>(Cycles) / 16;
        while (blocks > 0) {
            const u32 chunk = blocks > 0x7fffffffu ? 0x7fffffffu : static_cast<u32>(blocks);
            u32 count = chunk;
            __asm__ __volatile__(
                "1:\n\t"
                FL_SPI_DELAY_REPEAT_32("adds %0, %0, #1\n\t")
                "subs %1, %1, #1\n\tbne 1b"
                : "+&l"(dependency), "+&l"(count) : : "cc", "memory");
            blocks -= chunk;
        }
        SpiArmPadding<static_cast<u8>(Cycles % 16)>::run(dependency);
#elif defined(FL_SPI_ESP_INSTRUCTION_WAIT)
        u32 dependency = 0;
        u64 blocks = static_cast<u64>(Cycles) / 16;
        while (blocks > 0) {
            const u32 chunk = blocks > 0x7fffffffu ? 0x7fffffffu : static_cast<u32>(blocks);
            u32 count = chunk;
            // Exactly one 32-ADDI body in the emitted loop. Keep assembly
            // counting so even huge compile-time requests cannot be unrolled.
#if defined(__XTENSA__)
            __asm__ __volatile__(
                "1:\n\t"
                FL_SPI_DELAY_REPEAT_32("addi %0, %0, 1\n\t")
                "addi %1, %1, -1\n\tbnez %1, 1b"
                : "+&a"(dependency), "+&a"(count) : : "memory");
#else
            __asm__ __volatile__(
                "1:\n\t"
                FL_SPI_DELAY_REPEAT_32("addi %0, %0, 1\n\t")
                "addi %1, %1, -1\n\tbnez %1, 1b"
                : "+&r"(dependency), "+&r"(count) : : "memory");
#endif
            blocks -= chunk;
        }
        SpiEspPadding<static_cast<u8>(Cycles % 16)>::run(dependency);
#elif defined(ESP8266) || defined(FL_IS_CI13XX)
        // These SDKs enable their CPU cycle counter during startup. Use only
        // a low-word subtraction and bounded chunks to tolerate counter wrap.
        u64 remaining = Cycles;
        while (remaining > 0) {
            const u32 chunk = remaining > 0x7fffffffu ? 0x7fffffffu : static_cast<u32>(remaining);
            u32 start;
#if defined(__XTENSA__)
            __asm__ __volatile__("rsr %0,ccount" : "=a"(start) : : "memory");
#else
            __asm__ __volatile__("csrr %0,mcycle" : "=r"(start) : : "memory");
#endif
            u32 now;
            do {
#if defined(__XTENSA__)
                __asm__ __volatile__("rsr %0,ccount" : "=a"(now) : : "memory");
#else
                __asm__ __volatile__("csrr %0,mcycle" : "=r"(now) : : "memory");
#endif
            } while (static_cast<u32>(now - start) < chunk);
            remaining -= chunk;
        }
#else
        // No proven minimum-time primitive for this architecture. A platform
        // may provide BitbangPin::delay, but do not silently replace cycle
        // waits with NOPs or an interrupt-dependent micros() polling loop.
        FL_STATIC_ASSERT(Cycles <= 0,
            "Software SPI needs a platform minimum-cycle delay backend");
#endif
    }
};

} // namespace detail

struct BitbangTiming {
    static constexpr bool kCalibratedTiming = false;
    template <i64 Cycles>
    FASTLED_FORCE_INLINE static void delay() FL_NO_EXCEPT {
        detail::SpiCycleWait<Cycles>::run();
    }
};

} // namespace fl

#ifdef FL_SPI_PICO_CYCLE_WAIT
#undef FL_SPI_PICO_CYCLE_WAIT
#endif

#ifdef FL_SPI_ESP_INSTRUCTION_WAIT
#undef FL_SPI_ESP_INSTRUCTION_WAIT
#endif

#undef FL_SPI_DELAY_REPEAT_32
#undef FL_SPI_DELAY_REPEAT_16
#undef FL_SPI_DELAY_REPEAT_8
#undef FL_SPI_DELAY_REPEAT_4
#undef FL_SPI_DELAY_REPEAT_2
