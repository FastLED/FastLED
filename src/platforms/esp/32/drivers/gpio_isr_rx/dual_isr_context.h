#pragma once

// ok no namespace fl

/**
 * @file dual_isr_context.h
 * @brief Dual-ISR GPIO edge capture context structures for ESP32-C6 RISC-V
 *
 * Architecture:
 * - Fast ISR (RISC-V assembly, priority level 6): Captures GPIO edges with MCPWM timestamps
 * - Slow ISR (C, priority level 3, 10µs interval): Processes buffer, applies filtering, manages completion
 *
 * Target: ESP32-C6 (RV32IMAC - 32-bit RISC-V with M/A/C extensions)
 * CPU: 160 MHz (default) or 240 MHz (overclocked)
 * MCPWM Timer: 80 MHz (12.5 ns resolution)
 *
 * Performance:
 * - Fast ISR target: <20 cycles (~83 ns @ 240 MHz)
 * - Timestamp resolution: 12.5 ns (MCPWM hardware timer)
 * - Edge capture rate: >1 MHz
 * - CPU load: <20% during active capture
 *
 * Memory Layout:
 * - All structures placed in IRAM (.iram1 section) for zero-wait-state access
 * - DualIsrContext: Single cache line (64 bytes) for fast ISR access
 * - Edge buffer: Contiguous array of EdgeEntry structs
 * - Output buffer: Separate filtered edge buffer (managed by slow ISR)
 */

#include "fl/stl/stdint.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Edge Entry Structure
// ============================================================================

/**
 * @brief Single edge timestamp entry captured by fast ISR
 *
 * Size: 8 bytes (aligned for fast access)
 * Layout: Packed to 8 bytes for efficient memory usage
 * Alignment: 4-byte aligned (RV32 word access)
 *
 * The fast ISR writes {timestamp, level} atomically to circular buffer.
 * The slow ISR reads and filters these entries.
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t timestamp;  ///< MCPWM capture timer value (12.5 ns ticks @ 80 MHz)
    uint8_t level;       ///< GPIO level at edge (0 or 1)
    uint8_t _pad[3];     ///< Padding to 8 bytes for alignment
} EdgeEntry;

// Static assertions for structure layout (C11/C++11 compatible)
#ifdef __cplusplus
static_assert(sizeof(EdgeEntry) == 8, "EdgeEntry must be exactly 8 bytes");
static_assert(alignof(EdgeEntry) == 4, "EdgeEntry must be 4-byte aligned");
#else
_Static_assert(sizeof(EdgeEntry) == 8, "EdgeEntry must be exactly 8 bytes");
_Static_assert(_Alignof(EdgeEntry) == 4, "EdgeEntry must be 4-byte aligned");
#endif

// ============================================================================
// Dual-ISR Context Structure
// ============================================================================

/**
 * @brief Shared context between fast ISR (assembly) and slow ISR (C)
 *
 * Size: 64 bytes (single cache line on ESP32-C6)
 * Alignment: 64-byte aligned to fit in single cache line
 * Placement: IRAM (.iram1 section) for zero-wait-state access
 *
 * Member Ordering: Hot path variables first (most frequently accessed by fast ISR)
 *
 * Access Pattern:
 * - Fast ISR: Reads armed, writes edges to buffer[write_index], updates write_index
 * - Slow ISR: Reads write_index, reads/filters buffer[read_index], manages state
 * - Main thread: Initializes, polls done flag, reads output buffer
 *
 * Atomicity:
 * - write_index: Atomic uint32_t (updated by fast ISR with RV32A atomic ops or careful ordering)
 * - read_index: Updated only by slow ISR (no atomicity needed)
 * - armed: Atomic bool (slow ISR disarms when done)
 * - done: Atomic bool (slow ISR signals completion)
 *
 * Cache Line Layout (64 bytes):
 * [0-31]:   Hot path - accessed by fast ISR on every edge
 * [32-47]:  Medium-hot - configuration values
 * [48-63]:  Cold path - handles, flags, counters
 */
typedef struct __attribute__((aligned(64))) {
    // ========================================================================
    // HOT PATH: Fast ISR accesses on EVERY edge (bytes 0-31)
    // ========================================================================

    /**
     * @brief Circular buffer base pointer
     * Type: EdgeEntry* (pointer to edge buffer array)
     * Access: Fast ISR reads (loads buffer address)
     * Alignment: Must be 4-byte aligned (pointer)
     */
    EdgeEntry* buffer;

    /**
     * @brief Circular buffer size (number of EdgeEntry elements)
     * Type: uint32_t (must be power of 2 for fast modulo optimization)
     * Access: Fast ISR reads (for wrap-around check)
     * Note: Power of 2 required to use bitwise AND instead of slow modulo
     */
    uint32_t buffer_size;

    /**
     * @brief Circular buffer size mask (buffer_size - 1)
     * Type: uint32_t (precomputed for fast modulo: index & mask)
     * Access: Fast ISR reads (for wrap-around: write_index & mask)
     * Note: Only valid if buffer_size is power of 2
     * Performance: Saves 5-38 cycles per edge (vs remu instruction)
     */
    uint32_t buffer_size_mask;

    /**
     * @brief Current write index (updated by fast ISR)
     * Type: uint32_t (atomic)
     * Access: Fast ISR writes (atomic increment), slow ISR reads (atomic load)
     * Atomicity: Updated with RV32A atomic instructions (lr.w/sc.w) or careful ordering
     * Range: [0, buffer_size - 1]
     */
    volatile uint32_t write_index;

    /**
     * @brief Current read index (updated by slow ISR)
     * Type: uint32_t (no atomicity needed - only slow ISR writes)
     * Access: Slow ISR writes, fast ISR never reads
     * Range: [0, buffer_size - 1]
     */
    uint32_t read_index;

    /**
     * @brief Armed flag - controls fast ISR operation
     * Type: bool (atomic)
     * Access: Fast ISR reads (first check), slow ISR writes (atomic store)
     * Behavior: Fast ISR exits immediately if false
     */
    volatile bool armed;

    /**
     * @brief MCPWM capture register address (precomputed)
     * Type: uint32_t (hardware register address)
     * Access: Fast ISR reads (loads timestamp from this address)
     * Note: Direct register address for fastest possible access
     */
    uint32_t mcpwm_capture_reg_addr;

    /**
     * @brief GPIO input register address (precomputed)
     * Type: uint32_t (GPIO_IN_REG or GPIO_IN1_REG)
     * Access: Fast ISR reads (loads pin level from this address)
     */
    uint32_t gpio_in_reg_addr;

    /**
     * @brief GPIO bit mask (precomputed)
     * Type: uint32_t (bit mask: 1 << pin_offset)
     * Access: Fast ISR reads (extracts pin level: (reg & mask) ? 1 : 0)
     */
    uint32_t gpio_bit_mask;

    // ========================================================================
    // MEDIUM-HOT PATH: Configuration values (bytes 32-47)
    // ========================================================================

    /**
     * @brief GPIO pin number
     * Type: int (gpio_num_t)
     * Access: Stored for debug/logging
     */
    int pin;

    /**
     * @brief Skip signals counter (edges to skip before capturing)
     * Type: uint32_t
     * Access: Slow ISR reads/decrements
     * Purpose: Skip initial edges when buffer is memory-constrained
     */
    uint32_t skip_signals;

    /**
     * @brief Minimum pulse width in MCPWM ticks (jitter filter)
     * Type: uint32_t (in 12.5 ns ticks)
     * Access: Slow ISR reads (filters out edges with duration < min_pulse_ticks)
     * Purpose: Noise rejection - ignore glitches shorter than threshold
     */
    uint32_t min_pulse_ticks;

    /**
     * @brief Timeout in MCPWM ticks (idle detection)
     * Type: uint32_t (in 12.5 ns ticks)
     * Access: Slow ISR reads (checks elapsed time since last edge)
     * Purpose: Completion detection - signal done when no edges for timeout period
     */
    uint32_t timeout_ticks;

    // ========================================================================
    // COLD PATH: Handles, flags, counters (bytes 48-63)
    // ========================================================================

    /**
     * @brief Hardware timer handle for slow ISR
     * Type: void* (gptimer_handle_t)
     * Access: Slow ISR reads (for timer control)
     */
    void* gptimer_handle;

    /**
     * @brief Direct interrupt handle for GPIO fast ISR
     * Type: void* (intr_handle_t)
     * Access: Allocated by esp_intr_alloc() in begin(), freed by esp_intr_free() in destructor
     * Purpose: Direct interrupt allocation at Level 4+ for assembly ISR (no GPIO ISR service)
     * Note: Must be nullptr when unallocated
     */
    void* intr_handle;

    /**
     * @brief Output buffer pointer (filtered edges)
     * Type: EdgeEntry* (pointer to output array)
     * Access: Slow ISR writes filtered edges here
     * Note: Separate from circular buffer (no fast ISR access)
     */
    EdgeEntry* output_buffer;

    /**
     * @brief Output buffer size (capacity in EdgeEntry elements)
     * Type: uint32_t
     * Access: Slow ISR reads (for overflow check)
     */
    uint32_t output_buffer_size;

    /**
     * @brief Output count (number of filtered edges written)
     * Type: uint32_t
     * Access: Slow ISR writes, main thread reads
     */
    volatile uint32_t output_count;

    /**
     * @brief Last edge timestamp (for timeout detection)
     * Type: uint32_t (MCPWM ticks)
     * Access: Slow ISR reads/writes
     */
    uint32_t last_edge_timestamp;

    /**
     * @brief Done flag - signals completion to main thread
     * Type: bool (atomic)
     * Access: Slow ISR writes (atomic store), main thread reads (atomic load)
     */
    volatile bool done;

} DualIsrContext;

// Static assertions for context layout (C11/C++11 compatible)
// Note: Structure is now >64 bytes due to buffer_size_mask optimization field
// First 64 bytes (hot path + medium-hot) should still fit in first cache line
#ifdef __cplusplus
static_assert(sizeof(DualIsrContext) <= 128, "DualIsrContext must fit in 2 cache lines or less");
static_assert(alignof(DualIsrContext) == 64, "DualIsrContext must be 64-byte aligned");
#else
_Static_assert(sizeof(DualIsrContext) <= 128, "DualIsrContext must fit in 2 cache lines or less");
_Static_assert(_Alignof(DualIsrContext) == 64, "DualIsrContext must be 64-byte aligned");
#endif

// ============================================================================
// Memory Layout Notes
// ============================================================================

/**
 * Memory Placement:
 *
 * 1. DualIsrContext structure:
 *    - MUST be placed in IRAM (.iram1 section)
 *    - Use FL_IRAM attribute on instance declaration
 *    - Example: FL_IRAM static DualIsrContext g_isr_ctx;
 *
 * 2. Fast ISR code:
 *    - MUST be placed in IRAM (.section .iram1 directive)
 *    - Assembly file directive: .section .iram1, "ax"
 *    - Zero-wait-state access for minimum latency
 *
 * 3. Circular buffer (EdgeEntry array):
 *    - SHOULD be placed in IRAM for fast access (optional but recommended)
 *    - Can be in DRAM if IRAM constrained (adds ~1-2 cycles per access)
 *    - Must be 4-byte aligned (natural alignment for EdgeEntry)
 *
 * 4. Output buffer:
 *    - Can be in DRAM (only slow ISR accesses)
 *    - No IRAM requirement (not performance-critical)
 *
 * Alignment Requirements:
 *
 * 1. DualIsrContext: 64-byte aligned (cache line)
 *    - Ensures entire structure fits in single cache line
 *    - Prevents cache line splits (performance optimization)
 *
 * 2. EdgeEntry: 4-byte aligned (RV32 word)
 *    - Allows efficient lw/sw instructions (load/store word)
 *    - Misaligned access causes exception on RISC-V
 *
 * 3. Buffer pointers: Must point to properly aligned EdgeEntry arrays
 *    - buffer: Must be 4-byte aligned minimum
 *    - output_buffer: Must be 4-byte aligned minimum
 *
 * Atomic Access Patterns:
 *
 * 1. write_index (fast ISR to slow ISR communication):
 *    - Fast ISR: Atomic increment with wrap-around
 *      ```asm
 *      loop:
 *          lr.w t1, (write_index_addr)      # Load-reserved
 *          addi t2, t1, 1                    # Increment
 *          remu t2, t2, buffer_size          # Wrap-around (or AND if power of 2)
 *          sc.w t3, t2, (write_index_addr)   # Store-conditional
 *          bnez t3, loop                     # Retry if failed
 *      ```
 *    - Slow ISR: Atomic load
 *      ```c
 *      uint32_t current_write = __atomic_load_n(&ctx->write_index, __ATOMIC_ACQUIRE);
 *      ```
 *
 * 2. armed flag (slow ISR to fast ISR communication):
 *    - Slow ISR: Atomic store (disarm when done)
 *      ```c
 *      __atomic_store_n(&ctx->armed, false, __ATOMIC_RELEASE);
 *      ```
 *    - Fast ISR: Non-atomic load (early exit if false, no strict ordering needed)
 *      ```asm
 *      lbu t0, armed_offset(a0)   # Load byte (armed flag)
 *      beqz t0, exit              # Exit if not armed
 *      ```
 *
 * 3. done flag (slow ISR to main thread communication):
 *    - Slow ISR: Atomic store (signal completion)
 *      ```c
 *      __atomic_store_n(&ctx->done, true, __ATOMIC_RELEASE);
 *      ```
 *    - Main thread: Atomic load (poll for completion)
 *      ```c
 *      bool is_done = __atomic_load_n(&ctx->done, __ATOMIC_ACQUIRE);
 *      ```
 *
 * Buffer Overflow Handling:
 *
 * 1. Circular buffer (fast ISR):
 *    - Fast ISR writes until buffer full
 *    - Slow ISR reads as fast as possible
 *    - If write_index catches read_index: overflow (should never happen with proper sizing)
 *    - Buffer size must be large enough to handle burst edges between slow ISR invocations
 *
 * 2. Output buffer (slow ISR):
 *    - Slow ISR checks output_count < output_buffer_size before writing
 *    - If full: Set done flag, disarm fast ISR, stop capture
 *
 * Performance Considerations:
 *
 * 1. Cache line alignment (64 bytes):
 *    - Single cache line load brings entire context into cache
 *    - Subsequent accesses are cache hits (zero wait states)
 *    - Critical for fast ISR latency (<20 cycles)
 *
 * 2. Hot path ordering:
 *    - Most frequently accessed members at start of structure
 *    - Fast ISR accesses: buffer, buffer_size, write_index, armed, register addresses
 *    - Reduces cache miss probability
 *
 * 3. Pointer precomputation:
 *    - MCPWM capture register address precomputed (no HAL calls in ISR)
 *    - GPIO register address precomputed (direct register access)
 *    - Bit mask precomputed (no shift in ISR)
 *
 * 4. Atomic operation cost:
 *    - RV32A lr.w/sc.w: ~5-10 cycles (depends on contention)
 *    - Non-atomic load: ~1-2 cycles
 *    - Use atomics only where strictly necessary
 */

#ifdef __cplusplus
}
#endif
