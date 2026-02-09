// Test to verify FL_PGM_READ_DWORD_ALIGNED is defined on all platforms
#include "fastled_progmem.h"
#include "fl/int.h"

// CRITICAL: This macro MUST be defined on ALL platforms
#ifndef FL_PGM_READ_DWORD_ALIGNED
#error "FL_PGM_READ_DWORD_ALIGNED is NOT DEFINED! This will break sin32.h and other code."
#endif

// Test that it actually works with aligned data
FL_ALIGN_PROGMEM static const fl::u32 test_data[4] = {
    0x12345678,
    0xAABBCCDD,
    0x11223344,
    0xFFEEDDCC
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

    return 0;  // Success
}
