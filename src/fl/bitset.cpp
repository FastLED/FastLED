#include "fl/compiler_control.h"
#include "fl/bitset_dynamic.h"

#if !FASTLED_ALL_SRC
#include "fl/bitset.cpp.hpp"
#endif

// Implementation for bitset_dynamic constructor from bitstring
// This needs to be non-inline to be included in the library
fl::bitset_dynamic::bitset_dynamic(const char* bitstring) : _blocks(nullptr), _block_count(0), _size(0) {
    if (!bitstring) return;
    fl::u32 len = 0;
    while (bitstring[len] == '0' || bitstring[len] == '1') ++len;
    if (len == 0) return;
    resize(len);
    
    // Parse the bitstring and set bits (first character is MSB)
    for (fl::u32 i = 0; i < len; ++i) {
        if (bitstring[i] == '1') set(i, true);
        else if (bitstring[i] == '0') set(i, false);
        // ignore other chars
    }
}
