/// @file memory_barrier.h
/// @brief ESP32 platform-specific memory barrier macros
///
/// Provides architecture-specific memory barriers for synchronization between
/// ISR and main thread on ESP32 platforms (Xtensa and RISC-V).
///
/// This file is included by platforms/memory_barrier.h for ESP32 platforms.

// ok no namespace fl

#pragma once

#ifdef ESP32

//=============================================================================
// ESP32 Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading
// non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// Architecture-Specific Barriers:
// - Xtensa (ESP32, ESP32-S3): memw instruction (memory write barrier)
// - RISC-V (ESP32-C6, ESP32-C3, ESP32-H2, ESP32-P4): fence rw,rw instruction
//   (full memory fence)
//
#if defined(__XTENSA__)
// Xtensa architecture (ESP32, ESP32-S3)
#define FL_MEMORY_BARRIER asm volatile("memw" ::: "memory")
#elif defined(__riscv)
// RISC-V architecture (ESP32-C6, ESP32-C3, ESP32-H2, ESP32-P4)
#define FL_MEMORY_BARRIER asm volatile("fence rw, rw" ::: "memory")
#else
#error \
    "Unsupported architecture for ESP32 memory barrier (expected __XTENSA__ or __riscv)"
#endif

#endif // ESP32
