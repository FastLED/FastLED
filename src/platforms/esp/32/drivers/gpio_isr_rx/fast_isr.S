/**
 * @file fast_isr.S
 * @brief Ultra-fast GPIO edge capture ISR for ESP32-C6 RISC-V
 *
 * Architecture: Dual-ISR system for sub-microsecond edge timestamping
 * - Fast ISR (this file): Captures GPIO edges with MCPWM timestamps
 * - Slow ISR (C): Processes circular buffer, applies filtering, manages completion
 *
 * Target: ESP32-C6 (RV32IMAC - 32-bit RISC-V with M/A/C extensions)
 * CPU: 160 MHz (default) or 240 MHz (overclocked)
 * MCPWM Timer: 80 MHz (12.5 ns resolution)
 *
 * Performance Goals:
 * - Target execution time: <20 cycles (~83 ns @ 240 MHz)
 * - Zero stack operations (no prologue/epilogue)
 * - Direct register access only
 * - Atomic circular buffer write
 *
 * ISR Workflow:
 * 1. Check armed flag - exit immediately if false
 * 2. Read MCPWM capture register (hardware timestamp)
 * 3. Read GPIO level from GPIO_IN register
 * 4. Load write_index atomically
 * 5. Calculate buffer offset: base + (write_index * sizeof(EdgeEntry))
 * 6. Write {timestamp, level} to buffer[offset]
 * 7. Increment write_index (modulo buffer_size) atomically
 * 8. Return via mret (machine trap return)
 *
 * Register Usage (RISC-V Calling Convention):
 * - a0: DualIsrContext pointer (input, preserved)
 * - t0-t6: Temporary registers (can be clobbered)
 * - sp, ra: NEVER modified (no stack, no function calls)
 *
 * Memory Access Alignment:
 * - All loads/stores are properly aligned:
 *   - Word (32-bit): 4-byte aligned (lw/sw)
 *   - Halfword (16-bit): 2-byte aligned (lh/lhu/sh)
 *   - Byte (8-bit): No alignment requirement (lb/lbu/sb)
 *
 * Atomicity:
 * - write_index update uses RV32A atomic instructions (lr.w/sc.w)
 * - Ensures consistency between fast ISR and slow ISR
 *
 * IRAM Placement:
 * - Entire ISR placed in .iram1 section for zero-wait-state access
 * - Critical for achieving <20 cycle target
 */

#ifdef __riscv  // Only compile for RISC-V platforms (ESP32-C6)

// ============================================================================
// Section Placement
// ============================================================================

.section .iram1, "ax"  // Allocatable + executable, placed in IRAM
.align 2               // 4-byte alignment (2^2)

// ============================================================================
// DualIsrContext Structure Offsets
// ============================================================================

// Context structure layout (from dual_isr_context.h):
// Offset | Size | Member
// -------|------|-------
// 0      | 4    | EdgeEntry* buffer
// 4      | 4    | uint32_t buffer_size
// 8      | 4    | uint32_t buffer_size_mask (NEW - for fast modulo optimization)
// 12     | 4    | volatile uint32_t write_index
// 16     | 4    | uint32_t read_index
// 20     | 1    | volatile bool armed
// 21     | 3    | padding
// 24     | 4    | uint32_t mcpwm_capture_reg_addr
// 28     | 4    | uint32_t gpio_in_reg_addr
// 32     | 4    | uint32_t gpio_bit_mask
// ... (remaining fields not accessed by fast ISR)

// Define offsets as constants for clarity
.equ OFFSET_BUFFER, 0
.equ OFFSET_BUFFER_SIZE, 4
.equ OFFSET_BUFFER_SIZE_MASK, 8
.equ OFFSET_WRITE_INDEX, 12
.equ OFFSET_READ_INDEX, 16
.equ OFFSET_ARMED, 20
.equ OFFSET_MCPWM_CAPTURE_REG_ADDR, 24
.equ OFFSET_GPIO_IN_REG_ADDR, 28
.equ OFFSET_GPIO_BIT_MASK, 32

// EdgeEntry structure size (8 bytes: 4-byte timestamp + 1-byte level + 3-byte padding)
.equ SIZEOF_EDGE_ENTRY, 8

// ============================================================================
// Fast ISR Entry Point
// ============================================================================

.globl gpio_fast_edge_isr
.type gpio_fast_edge_isr, @function

/**
 * @brief Fast GPIO edge ISR with MCPWM timestamp capture
 *
 * @param a0 DualIsrContext* - Pointer to ISR context structure
 *
 * @note This function is called directly from the interrupt dispatcher
 * @note Returns via mret (machine trap return), NOT ret
 * @note NO stack operations - all work done in registers
 * @note Optimized for minimum cycle count (~15-20 cycles)
 */
gpio_fast_edge_isr:
    // ========================================================================
    // Step 1: Check armed flag (early exit if not armed)
    // ========================================================================
    // Load armed flag (1 byte, offset 16)
    // Use lbu (load byte unsigned) - no alignment requirement
    lbu     t0, OFFSET_ARMED(a0)    // t0 = ctx->armed (0 or 1)
    beqz    t0, .Lexit              // if (!armed) goto exit

    // ========================================================================
    // Step 2: Read MCPWM capture register (hardware timestamp)
    // ========================================================================
    // Load MCPWM capture register address (precomputed during init)
    lw      t1, OFFSET_MCPWM_CAPTURE_REG_ADDR(a0)  // t1 = register address
    lw      t2, 0(t1)               // t2 = *mcpwm_capture_reg (timestamp value)

    // ========================================================================
    // Step 3: Read GPIO level from GPIO_IN register
    // ========================================================================
    // Load GPIO register address and bit mask (precomputed during init)
    lw      t3, OFFSET_GPIO_IN_REG_ADDR(a0)        // t3 = GPIO_IN_REG address
    lw      t4, OFFSET_GPIO_BIT_MASK(a0)           // t4 = bit mask (1 << pin)
    lw      t5, 0(t3)               // t5 = GPIO_IN_REG value (all 32 pins)
    and     t5, t5, t4              // t5 = gpio_value & mask
    snez    t5, t5                  // t5 = (t5 != 0) ? 1 : 0 (normalize to 0 or 1)

    // ========================================================================
    // Step 4: Load circular buffer metadata
    // ========================================================================
    // Load buffer base pointer and buffer size mask
    lw      t6, OFFSET_BUFFER(a0)           // t6 = buffer base address (EdgeEntry*)
    lw      t3, OFFSET_BUFFER_SIZE_MASK(a0) // t3 = buffer_size_mask (for fast modulo)

    // ========================================================================
    // Step 5: Atomic read-modify-write of write_index
    // ========================================================================
    // Use RV32A atomic instructions (lr.w/sc.w) for lock-free circular buffer
    // Compute write_index address ONCE before atomic loop (optimization)
    addi    t4, a0, OFFSET_WRITE_INDEX  // t4 = &ctx->write_index

.Lretry_atomic:
    // Load-reserved: Atomically load write_index and set reservation
    lr.w    t0, (t4)                // t0 = current write_index (atomic load)

    // Calculate next write_index: (write_index + 1) & buffer_size_mask
    // This is equivalent to (write_index + 1) % buffer_size when buffer_size is power of 2
    // Saves 5-38 cycles compared to remu instruction!
    addi    t1, t0, 1               // t1 = write_index + 1
    and     t1, t1, t3              // t1 = (write_index + 1) & mask (fast wrap-around)

    // Store-conditional: Atomically store new write_index if reservation still valid
    // If successful, t5 = 0; if failed (contention), t5 = nonzero
    sc.w    t5, t1, (t4)            // Atomic store to &ctx->write_index
    bnez    t5, .Lretry_atomic      // Retry if store-conditional failed

    // ========================================================================
    // Step 6: Calculate buffer offset and write edge entry
    // ========================================================================
    // Calculate offset: buffer_base + (write_index * sizeof(EdgeEntry))
    // sizeof(EdgeEntry) = 8 bytes, so multiply by 8 (shift left by 3)
    slli    t1, t0, 3               // t1 = write_index * 8 (shift left 3 bits)
    add     t1, t6, t1              // t1 = buffer + offset (address of EdgeEntry)

    // Write EdgeEntry: {timestamp (4 bytes), level (1 byte), padding (3 bytes)}
    sw      t2, 0(t1)               // EdgeEntry.timestamp = MCPWM capture value
    sb      t5, 4(t1)               // EdgeEntry.level = GPIO level (0 or 1)
    // Note: Padding bytes (5-7) are not initialized (don't care)

    // ========================================================================
    // Step 7: Return from interrupt
    // ========================================================================
.Lexit:
    mret                            // Machine trap return (restores PC from MEPC)

.size gpio_fast_edge_isr, .-gpio_fast_edge_isr

#endif // __riscv
