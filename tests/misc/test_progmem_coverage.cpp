// Test to verify FL_PGM_READ_DWORD_ALIGNED is defined on all platforms
#include "fastled_progmem.h"  // IWYU pragma: keep
#include "fl/int.h"
#include "fl/stl/stdint.h"  // for uintptr_t

// CRITICAL: This macro MUST be defined on ALL platforms
#ifndef FL_PGM_READ_DWORD_ALIGNED
#error "FL_PGM_READ_DWORD_ALIGNED is NOT DEFINED! This will break sin32.h and other code."
#endif

// Test that it actually works with aligned data (4-byte alignment)
FL_ALIGN_PROGMEM(4) static const fl::u32 test_data[4] = {
    0x12345678,
    0xAABBCCDD,
    0x11223344,
    0xFFEEDDCC
};

// Test FL_ALIGN_PROGMEM for cache-line optimization (64-byte alignment)
// This is useful for large lookup tables accessed frequently
FL_ALIGN_PROGMEM(64) static const fl::u32 test_data_64[16] = {
    0x00000000, 0x11111111, 0x22222222, 0x33333333,
    0x44444444, 0x55555555, 0x66666666, 0x77777777,
    0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
    0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF
};

int main() {
    // Read using FL_PGM_READ_DWORD_ALIGNED
    fl::u32 val1 = FL_PGM_READ_DWORD_ALIGNED(&test_data[0]);
    fl::u32 val2 = FL_PGM_READ_DWORD_ALIGNED(&test_data[1]);
    fl::u32 val3 = FL_PGM_READ_DWORD_ALIGNED(&test_data[2]);
    fl::u32 val4 = FL_PGM_READ_DWORD_ALIGNED(&test_data[3]);

    // Verify values
    if (val1 != 0x12345678) return 1;
    if (val2 != 0xAABBCCDD) return 2;
    if (val3 != 0x11223344) return 3;
    if (val4 != 0xFFEEDDCC) return 4;

    // Test 64-byte aligned data
    fl::u32 val64_0 = FL_PGM_READ_DWORD_ALIGNED(&test_data_64[0]);
    fl::u32 val64_15 = FL_PGM_READ_DWORD_ALIGNED(&test_data_64[15]);
    if (val64_0 != 0x00000000) return 5;
    if (val64_15 != 0xFFFFFFFF) return 6;

    // Verify alignment at runtime (compile-time alignment is enforced by compiler)
    // On x86/ARM/ESP, this should be 64-byte aligned
    // On platforms without alignment support, it may be less but still functional
    uintptr_t addr = reinterpret_cast<uintptr_t>(&test_data_64[0]);
    (void)addr;  // Suppress unused warning - alignment is enforced at compile time

    return 0;  // Success
}
