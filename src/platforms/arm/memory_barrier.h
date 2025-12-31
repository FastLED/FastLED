/// @file memory_barrier.h
/// @brief ARM Cortex-M platform-specific memory barrier macros
///
/// Provides architecture-specific memory barriers for synchronization between
/// ISR and main thread on ARM Cortex-M platforms (M0/M0+/M3/M4/M7/M23/M33/M35P).
///
/// This file is included by platforms/memory_barrier.h for ARM platforms.

// ok no namespace fl

#pragma once

#ifdef FASTLED_ARM

//=============================================================================
// ARM Cortex-M Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading
//   non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// ARM Cortex-M Architecture Barriers:
// - Cortex-M3/M4/M7 (ARMv7-M): DMB instruction (Data Memory Barrier)
// - Cortex-M23/M33/M35P (ARMv8-M): DMB instruction (Data Memory Barrier)
// - Cortex-M0/M0+ (ARMv6-M): No DMB instruction - use compiler barrier only
//
// DMB (Data Memory Barrier):
// - Ensures all memory accesses before the barrier complete before any
//   memory accesses after the barrier
// - "dmb" defaults to "dmb sy" (full system barrier, all shareability domains)
// - Appropriate for ISR synchronization on single-core MCUs
// - Lighter weight than DSB (Data Synchronization Barrier)
//
// Why DMB instead of DSB?
// - DSB (Data Synchronization Barrier) waits for all instructions to complete,
//   not just memory accesses. It's heavier weight and unnecessary for our use case.
// - ISB (Instruction Synchronization Barrier) is for pipeline/cache flushing
//   when modifying code or processor state. Not needed for data synchronization.
//
// Cortex-M0/M0+ Limitation:
// - ARMv6-M architecture (Cortex-M0/M0+) does NOT have DMB, DSB, or ISB instructions
// - These were added in ARMv7-M (Cortex-M3 and later)
// - For M0/M0+, we use compiler barrier only: __asm__ __volatile__ ("" ::: "memory")
// - The volatile qualifier on ISR variables provides the primary synchronization
// - Compiler barrier prevents compiler reordering but provides no hardware barrier
//
// Architecture Detection:
// - __ARM_ARCH_7M__: Cortex-M3
// - __ARM_ARCH_7EM__: Cortex-M4/M7 (with DSP/FPU extensions)
// - __ARM_ARCH_8M_MAIN__: Cortex-M33/M35P (ARMv8-M Mainline)
// - __ARM_ARCH_8M_BASE__: Cortex-M23 (ARMv8-M Baseline)
// - __ARM_ARCH_6M__: Cortex-M0/M0+/M1
// - __ARM_ARCH: Numeric value (6, 7, 8, etc.) - used as fallback
//
// FastLED ARM Platforms:
// - STM32 (all variants): M0/M0+/M3/M4/M7 depending on variant
// - nRF52 (Nordic): Cortex-M4
// - RP2040 (Raspberry Pi Pico): Cortex-M0+
// - RP2350 (Raspberry Pi Pico 2): Cortex-M33
// - SAMD21 (Arduino Zero, Adafruit): Cortex-M0+
// - SAMD51/SAME51 (Adafruit): Cortex-M4
// - SAM3X8E (Arduino Due): Cortex-M3
// - Teensy 3.x (K20/K66): Cortex-M4
// - Teensy 4.x (IMXRT1062): Cortex-M7
// - Renesas RA4M1 (Arduino UNO R4): Cortex-M4
// - Apollo3 (SparkFun): Cortex-M4
// - Silicon Labs EFM32/MGM240: Cortex-M4/M33
//
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || \
    defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8M_BASE__) || \
    (defined(__ARM_ARCH) && __ARM_ARCH >= 7)
    // ARMv7-M or ARMv8-M: Cortex-M3/M4/M7/M23/M33/M35P
    // Use DMB (Data Memory Barrier) instruction
    // "dmb" defaults to "dmb sy" (full system barrier)
    // The "memory" clobber tells compiler that memory is modified, preventing
    // reordering of memory accesses across the barrier
    #define FL_MEMORY_BARRIER __asm__ __volatile__ ("dmb" ::: "memory")
#else
    // ARMv6-M (Cortex-M0/M0+) or unknown ARM architecture
    // No DMB instruction available - use compiler barrier only
    // This prevents compiler reordering but provides no hardware memory barrier
    // The volatile qualifier on ISR variables provides the primary synchronization
    #define FL_MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")
#endif

#endif // FASTLED_ARM
