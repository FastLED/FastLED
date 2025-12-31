/// @file memory_barrier.h
/// @brief ESP8266 platform-specific memory barrier macros
///
/// Provides architecture-specific memory barriers for synchronization between
/// ISR and main thread on ESP8266 platforms (Xtensa LX106).
///
/// This file is included by platforms/memory_barrier.h for ESP8266 platforms.

// ok no namespace fl

#pragma once

#ifdef ESP8266

//=============================================================================
// ESP8266 Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading
//   non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// Architecture Details:
// - ESP8266 uses Xtensa LX106 architecture (single-core, but interrupt-driven)
// - memw instruction (memory write barrier) ensures all prior memory writes
//   are completed before subsequent memory operations
// - Xtensa ABI specifies that memw should be executed between loads/stores
//   to volatile variables to ensure proper ordering
//
// Implementation:
// - Uses inline assembly with "memw" instruction (same as ESP32 Xtensa)
// - "memory" clobber tells compiler to treat all memory as modified,
//   preventing reordering of memory accesses across the barrier
// - volatile qualifier prevents compiler from optimizing away the barrier
//
// Why memw is needed on single-core ESP8266:
// - Although ESP8266 is single-core, interrupts can preempt execution
// - Without memw, the compiler or CPU might reorder memory operations
// - ISR writes to volatile variables must be visible to main thread
// - The barrier ensures sequential consistency at the point of the barrier
//
// Reference:
// - Xtensa ISA Summary: memw "finish all mem operations before next op"
// - ESP8266 uses same Xtensa instruction set as ESP32 for memory barriers
// - Instruction encoding: 0x0020c0 (MEMW)
//
#define FL_MEMORY_BARRIER asm volatile("memw" ::: "memory")

#endif // ESP8266
