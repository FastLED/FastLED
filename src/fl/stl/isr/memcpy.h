/// @file fl/stl/isr/memcpy.h
/// @brief ISR-safe memory operations (inline, header-only)

#pragma once

// allow-include-after-namespace

#include "fl/stl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {
namespace isr {

/// @brief Check if a pointer is aligned to a specific byte boundary
/// @param ptr Pointer to check
/// @param alignment Alignment requirement in bytes (must be power of 2)
/// @return true if aligned, false otherwise
FASTLED_FORCE_INLINE bool is_aligned(const void* ptr, size_t alignment) {
    return (fl::ptr_to_int(ptr) & (alignment - 1)) == 0;
}

/// @brief ISR-optimized 32-bit block copy for 4-byte aligned memory
/// @param dst Destination pointer (must be 4-byte aligned)
/// @param src Source pointer (must be 4-byte aligned)
/// @param count Number of 32-bit words to copy (NOT bytes)
/// @note Only call with 4-byte aligned pointers and valid count
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy_32(u32* FL_RESTRICT_PARAM dst,
               const u32* FL_RESTRICT_PARAM src,
               size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized 16-bit block copy for 2-byte aligned memory
/// @param dst Destination pointer (must be 2-byte aligned)
/// @param src Source pointer (must be 2-byte aligned)
/// @param count Number of 16-bit words to copy (NOT bytes)
/// @note Only call with 2-byte aligned pointers and valid count
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy_16(u16* FL_RESTRICT_PARAM dst,
               const u16* FL_RESTRICT_PARAM src,
               size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized byte copy
/// @param dst Destination pointer
/// @param src Source pointer
/// @param count Number of bytes to copy
/// @note No alignment requirements
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy_byte(u8* FL_RESTRICT_PARAM dst,
                 const u8* FL_RESTRICT_PARAM src,
                 size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized memcpy with alignment detection and switch dispatch
/// @param dst Destination pointer
/// @param src Source pointer
/// @param num_bytes Number of bytes to copy
/// @note Automatically uses 32-bit, 16-bit, or byte copy based on alignment
/// @note Uses switch for dispatch (compiler may optimize to jump table)
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy(void* FL_RESTRICT_PARAM dst,
            const void* FL_RESTRICT_PARAM src,
            size_t num_bytes) {
    uintptr_t dst_addr = fl::ptr_to_int(dst);
    uintptr_t src_addr = fl::ptr_to_int(src);

    size_t align2 = (((dst_addr | src_addr | num_bytes) & 1) == 0);
    size_t align4 = (((dst_addr | src_addr | num_bytes) & 3) == 0);

    int index = align2 + align4;

    switch (index) {
        case 2:
            memcpy_32(static_cast<u32*>(dst),
                      static_cast<const u32*>(src),
                      num_bytes >> 2);
            break;
        case 1:
            memcpy_16(static_cast<u16*>(dst),
                      static_cast<const u16*>(src),
                      num_bytes >> 1);
            break;
        case 0:
        default:
            memcpy_byte(static_cast<u8*>(dst),
                        static_cast<const u8*>(src),
                        num_bytes);
            break;
    }
}

/// @brief ISR-safe memset replacement (byte-by-byte zero)
/// @param dest Destination pointer
/// @param count Number of bytes to zero
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero_byte(u8* dest, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = 0x0;
    }
}

/// @brief ISR-safe word-aligned memset (4-byte writes)
/// @param dest Destination pointer (must be 4-byte aligned)
/// @param count Number of bytes to zero (will process in 4-byte chunks)
/// @note Handles remainder bytes using byte writes
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero_word(u8* dest, size_t count) {
    u32* dest32 = fl::bit_cast<u32*>(dest);
    size_t count32 = count / 4;
    size_t remainder = count % 4;

    for (size_t i = 0; i < count32; i++) {
        dest32[i] = 0;
    }

    if (remainder > 0) {
        u8* remainder_ptr = dest + (count32 * 4);
        memset_zero_byte(remainder_ptr, remainder);
    }
}

/// @brief ISR-safe memset with alignment optimization
/// @param dest Destination pointer
/// @param count Number of bytes to zero
/// @note Automatically selects word or byte writes based on alignment
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero(u8* dest, size_t count) {
    uintptr_t address = fl::ptr_to_int(dest);

    if ((address % 4 == 0) && (count >= 4)) {
        memset_zero_word(dest, count);
    } else {
        memset_zero_byte(dest, count);
    }
}

} // namespace isr
} // namespace fl
